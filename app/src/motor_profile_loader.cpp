#include "motor_profile_loader.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>

static constexpr int CURRENT_SCHEMA_VERSION = 1;

uint32_t MotorProfileLoader::parseCanId(const QJsonValue& value)
{
    if (value.isDouble()) {
        return static_cast<uint32_t>(value.toInt());
    }
    if (value.isString()) {
        QString str = value.toString();
        bool ok = false;
        uint32_t id = 0;
        if (str.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
            id = str.mid(2).toUInt(&ok, 16);
        } else {
            id = str.toUInt(&ok, 10);
        }
        return ok ? id : 0;
    }
    return 0;
}

FieldDefinition MotorProfileLoader::parseFieldDef(const QJsonObject& obj, QString& error)
{
    FieldDefinition field;

    field.id = obj.value(QStringLiteral("id")).toString();
    if (field.id.isEmpty()) {
        error = QStringLiteral("Field missing required 'id'");
        return field;
    }

    field.label = obj.value(QStringLiteral("label")).toString(field.id);
    field.byteOffset = obj.value(QStringLiteral("offset")).toInt(0);

    // Parse bits
    QJsonObject bitsObj = obj.value(QStringLiteral("bits")).toObject();
    field.bits.start = bitsObj.value(QStringLiteral("start")).toInt(0);
    field.bits.length = bitsObj.value(QStringLiteral("length")).toInt(16);

    // Parse endianness
    QString endian = obj.value(QStringLiteral("endianness")).toString(QStringLiteral("big"));
    field.littleEndian = (endian.toLower() == QStringLiteral("little"));

    field.signedValue = obj.value(QStringLiteral("signed")).toBool(false);
    field.scale = obj.value(QStringLiteral("scale")).toDouble(1.0);

    // Parse display limits
    QJsonObject limitsObj = obj.value(QStringLiteral("displayLimits")).toObject();
    field.displayLimits.min = limitsObj.value(QStringLiteral("min")).toDouble(0.0);
    field.displayLimits.max = limitsObj.value(QStringLiteral("max")).toDouble(65535.0);

    field.unit = obj.value(QStringLiteral("unit")).toString();

    return field;
}

CanIdMatcher MotorProfileLoader::parseCanIdMatcher(const QJsonObject& obj, QString& error)
{
    Q_UNUSED(error);
    CanIdMatcher matcher;

    // Check for exact canId first
    if (obj.contains(QStringLiteral("canId"))) {
        matcher.mode = CanIdMatcher::Mode::Exact;
        matcher.canId = parseCanId(obj.value(QStringLiteral("canId")));
    }
    // Check for mask-based matching
    else if (obj.contains(QStringLiteral("canIdMask"))) {
        QJsonObject maskObj = obj.value(QStringLiteral("canIdMask")).toObject();
        matcher.mode = CanIdMatcher::Mode::Mask;
        matcher.mask = parseCanId(maskObj.value(QStringLiteral("mask")));
        matcher.value = parseCanId(maskObj.value(QStringLiteral("value")));
    }

    return matcher;
}

MotorDescriptor MotorProfileLoader::parseMotorDef(const QJsonObject& obj,
                                                   const QVector<FieldDefinition>& defaultFields,
                                                   QString& error)
{
    MotorDescriptor motor;

    motor.label = obj.value(QStringLiteral("label")).toString();
    motor.canIdMatcher = parseCanIdMatcher(obj, error);

    // Start with default fields
    motor.fields = defaultFields;

    // Apply field overrides if present
    if (obj.contains(QStringLiteral("fieldOverrides"))) {
        QJsonObject overrides = obj.value(QStringLiteral("fieldOverrides")).toObject();
        for (auto it = overrides.begin(); it != overrides.end(); ++it) {
            QString fieldId = it.key();
            FieldDefinition override = parseFieldDef(it.value().toObject(), error);
            override.id = fieldId;  // Ensure ID matches the key

            // Find and replace in fields vector
            for (int i = 0; i < motor.fields.size(); ++i) {
                if (motor.fields[i].id == fieldId) {
                    motor.fields[i] = override;
                    break;
                }
            }
            motor.fieldOverrides.insert(fieldId, override);
        }
    }

    return motor;
}

MotorCommandGroup MotorProfileLoader::parseCommandGroup(const QJsonObject& obj, QString& error)
{
    Q_UNUSED(error);
    MotorCommandGroup group;

    group.label = obj.value(QStringLiteral("label")).toString();
    group.canId = parseCanId(obj.value(QStringLiteral("canId")));

    QJsonArray indicesArray = obj.value(QStringLiteral("motorIndices")).toArray();
    for (const QJsonValue& v : indicesArray) {
        group.motorIndices.push_back(v.toInt());
    }

    QString endian = obj.value(QStringLiteral("commandEndianness")).toString(QStringLiteral("big"));
    group.littleEndian = (endian.toLower() == QStringLiteral("little"));

    return group;
}

MotorProfileLoader::LoadResult MotorProfileLoader::loadFromFile(const QString& filePath)
{
    LoadResult result;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        result.errorMessage = QStringLiteral("Cannot open file: %1").arg(file.errorString());
        return result;
    }

    QByteArray data = file.readAll();
    file.close();

    result = loadFromJson(data, filePath);
    if (result.success) {
        result.profile.filePath = filePath;
    }
    return result;
}

MotorProfileLoader::LoadResult MotorProfileLoader::loadFromJson(const QByteArray& jsonData,
                                                                 const QString& sourceName)
{
    LoadResult result;

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        result.errorMessage = QStringLiteral("JSON parse error at offset %1: %2")
                                  .arg(parseError.offset)
                                  .arg(parseError.errorString());
        return result;
    }

    if (!doc.isObject()) {
        result.errorMessage = QStringLiteral("JSON root must be an object");
        return result;
    }

    QJsonObject root = doc.object();
    MotorProfile& profile = result.profile;

    // Parse version
    profile.version = root.value(QStringLiteral("version")).toInt(1);

    // Parse basic info
    profile.name = root.value(QStringLiteral("name")).toString(sourceName);
    profile.description = root.value(QStringLiteral("description")).toString();

    // Parse control limits
    QJsonObject limitsObj = root.value(QStringLiteral("controlLimits")).toObject();
    profile.controlLimits.min = limitsObj.value(QStringLiteral("min")).toInt(-16384);
    profile.controlLimits.max = limitsObj.value(QStringLiteral("max")).toInt(16384);

    // Parse default fields
    QString fieldError;
    QJsonArray fieldsArray = root.value(QStringLiteral("fields")).toArray();
    for (const QJsonValue& v : fieldsArray) {
        FieldDefinition field = parseFieldDef(v.toObject(), fieldError);
        if (!fieldError.isEmpty()) {
            result.errorMessage = fieldError;
            return result;
        }
        profile.defaultFields.push_back(field);
    }

    // If no fields defined, use defaults
    if (profile.defaultFields.isEmpty()) {
        profile.defaultFields = defaultFieldDefinitions();
    }

    // Parse motors
    QString motorError;
    QJsonArray motorsArray = root.value(QStringLiteral("motors")).toArray();
    for (const QJsonValue& v : motorsArray) {
        MotorDescriptor motor = parseMotorDef(v.toObject(), profile.defaultFields, motorError);
        if (!motorError.isEmpty()) {
            result.errorMessage = motorError;
            return result;
        }
        profile.motors.push_back(motor);
    }

    // Parse command groups
    QString groupError;
    QJsonArray groupsArray = root.value(QStringLiteral("commandGroups")).toArray();
    for (const QJsonValue& v : groupsArray) {
        MotorCommandGroup group = parseCommandGroup(v.toObject(), groupError);
        if (!groupError.isEmpty()) {
            result.errorMessage = groupError;
            return result;
        }
        profile.commandGroups.push_back(group);
    }

    result.success = true;
    return result;
}

QJsonObject MotorProfileLoader::fieldDefToJson(const FieldDefinition& field)
{
    QJsonObject obj;
    obj[QStringLiteral("id")] = field.id;
    obj[QStringLiteral("label")] = field.label;
    obj[QStringLiteral("offset")] = field.byteOffset;

    QJsonObject bits;
    bits[QStringLiteral("start")] = field.bits.start;
    bits[QStringLiteral("length")] = field.bits.length;
    obj[QStringLiteral("bits")] = bits;

    obj[QStringLiteral("endianness")] = field.littleEndian ? QStringLiteral("little") : QStringLiteral("big");
    obj[QStringLiteral("signed")] = field.signedValue;
    obj[QStringLiteral("scale")] = field.scale;

    QJsonObject limits;
    limits[QStringLiteral("min")] = field.displayLimits.min;
    limits[QStringLiteral("max")] = field.displayLimits.max;
    obj[QStringLiteral("displayLimits")] = limits;

    obj[QStringLiteral("unit")] = field.unit;
    return obj;
}

QJsonObject MotorProfileLoader::motorDefToJson(const MotorDescriptor& motor)
{
    QJsonObject obj;
    obj[QStringLiteral("label")] = motor.label;

    if (motor.canIdMatcher.mode == CanIdMatcher::Mode::Exact) {
        obj[QStringLiteral("canId")] = QStringLiteral("0x%1").arg(motor.canIdMatcher.canId, 0, 16);
    } else {
        QJsonObject maskObj;
        maskObj[QStringLiteral("mask")] = QStringLiteral("0x%1").arg(motor.canIdMatcher.mask, 0, 16);
        maskObj[QStringLiteral("value")] = QStringLiteral("0x%1").arg(motor.canIdMatcher.value, 0, 16);
        obj[QStringLiteral("canIdMask")] = maskObj;
    }

    // Only include field overrides, not all fields
    if (!motor.fieldOverrides.isEmpty()) {
        QJsonObject overrides;
        for (auto it = motor.fieldOverrides.begin(); it != motor.fieldOverrides.end(); ++it) {
            overrides[it.key()] = fieldDefToJson(it.value());
        }
        obj[QStringLiteral("fieldOverrides")] = overrides;
    }

    return obj;
}

QJsonObject MotorProfileLoader::commandGroupToJson(const MotorCommandGroup& group)
{
    QJsonObject obj;
    obj[QStringLiteral("label")] = group.label;
    obj[QStringLiteral("canId")] = QStringLiteral("0x%1").arg(group.canId, 0, 16);

    QJsonArray indices;
    for (int idx : group.motorIndices) {
        indices.append(idx);
    }
    obj[QStringLiteral("motorIndices")] = indices;

    obj[QStringLiteral("commandEndianness")] = group.littleEndian ? QStringLiteral("little") : QStringLiteral("big");
    return obj;
}

QJsonObject MotorProfileLoader::profileToJson(const MotorProfile& profile)
{
    QJsonObject root;
    root[QStringLiteral("version")] = profile.version;
    root[QStringLiteral("name")] = profile.name;
    root[QStringLiteral("description")] = profile.description;

    QJsonObject limits;
    limits[QStringLiteral("min")] = profile.controlLimits.min;
    limits[QStringLiteral("max")] = profile.controlLimits.max;
    root[QStringLiteral("controlLimits")] = limits;

    QJsonArray fields;
    for (const FieldDefinition& field : profile.defaultFields) {
        fields.append(fieldDefToJson(field));
    }
    root[QStringLiteral("fields")] = fields;

    QJsonArray motors;
    for (const MotorDescriptor& motor : profile.motors) {
        motors.append(motorDefToJson(motor));
    }
    root[QStringLiteral("motors")] = motors;

    QJsonArray groups;
    for (const MotorCommandGroup& group : profile.commandGroups) {
        groups.append(commandGroupToJson(group));
    }
    root[QStringLiteral("commandGroups")] = groups;

    return root;
}

bool MotorProfileLoader::saveToFile(const MotorProfile& profile, const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    QJsonObject root = profileToJson(profile);
    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

MotorProfileLoader::ValidationResult MotorProfileLoader::validate(const MotorProfile& profile)
{
    ValidationResult result;

    // Version check
    if (profile.version > CURRENT_SCHEMA_VERSION) {
        result.warnings << QStringLiteral("Profile version %1 is newer than supported %2")
                               .arg(profile.version)
                               .arg(CURRENT_SCHEMA_VERSION);
    }

    // Field validation
    QSet<QString> fieldIds;
    for (const FieldDefinition& field : profile.defaultFields) {
        if (field.id.isEmpty()) {
            result.errors << QStringLiteral("Field missing required 'id'");
            result.valid = false;
        }
        if (fieldIds.contains(field.id)) {
            result.errors << QStringLiteral("Duplicate field id: %1").arg(field.id);
            result.valid = false;
        }
        fieldIds.insert(field.id);

        if (field.byteOffset < 0 || field.byteOffset > 7) {
            result.errors << QStringLiteral("Field %1: byteOffset must be 0-7").arg(field.id);
            result.valid = false;
        }
        if (field.bits.length < 1 || field.bits.length > 32) {
            result.errors << QStringLiteral("Field %1: bit length must be 1-32").arg(field.id);
            result.valid = false;
        }
    }

    // Motor validation
    for (int i = 0; i < profile.motors.size(); ++i) {
        const MotorDescriptor& motor = profile.motors[i];
        if (motor.label.isEmpty()) {
            result.warnings << QStringLiteral("Motor %1 has no label").arg(i);
        }
    }

    // Command group validation
    for (const MotorCommandGroup& group : profile.commandGroups) {
        for (int idx : group.motorIndices) {
            if (idx < 0 || idx >= profile.motors.size()) {
                result.errors << QStringLiteral("Command group '%1': motor index %2 out of range")
                                    .arg(group.label)
                                    .arg(idx);
                result.valid = false;
            }
        }
    }

    return result;
}

QStringList MotorProfileLoader::profileSearchPaths()
{
    QStringList paths;

    // User config location
    QString userConfig = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (!userConfig.isEmpty()) {
        paths << userConfig + QStringLiteral("/profiles");
    }

    // Application directory
    QString appDir = QCoreApplication::applicationDirPath();
    paths << appDir + QStringLiteral("/config/profiles");

    // System-wide locations
    for (const QString& loc : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation)) {
        paths << loc + QStringLiteral("/dm-tool/profiles");
    }

    return paths;
}

QVector<MotorProfile> MotorProfileLoader::loadAllProfiles()
{
    QVector<MotorProfile> profiles;

    // Always include builtin default
    profiles.push_back(builtinDefault());

    // Search all paths for JSON profiles
    for (const QString& searchPath : profileSearchPaths()) {
        QDir dir(searchPath);
        if (!dir.exists()) {
            continue;
        }

        QDirIterator it(searchPath, {QStringLiteral("*.json")}, QDir::Files);
        while (it.hasNext()) {
            QString filePath = it.next();
            LoadResult result = loadFromFile(filePath);
            if (result.success) {
                ValidationResult validation = validate(result.profile);
                if (validation.valid) {
                    profiles.push_back(result.profile);
                }
            }
        }
    }

    return profiles;
}

MotorProfile MotorProfileLoader::builtinDefault()
{
    QVector<MotorProfile> defaults = defaultMotorProfiles();
    if (!defaults.isEmpty()) {
        return defaults.first();
    }

    // Fallback empty profile
    MotorProfile profile;
    profile.name = QStringLiteral("Empty Profile");
    return profile;
}
