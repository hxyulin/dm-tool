#ifndef MOTOR_PROFILE_LOADER_H
#define MOTOR_PROFILE_LOADER_H

#include "motor_profile.h"

#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>

class MotorProfileLoader
{
public:
    struct LoadResult
    {
        bool success = false;
        QString errorMessage;
        MotorProfile profile;
    };

    struct ValidationResult
    {
        bool valid = true;
        QStringList warnings;
        QStringList errors;
    };

    // Load from JSON file
    static LoadResult loadFromFile(const QString& filePath);

    // Load from JSON data
    static LoadResult loadFromJson(const QByteArray& jsonData,
                                   const QString& sourceName = QString());

    // Save profile to JSON file
    static bool saveToFile(const MotorProfile& profile, const QString& filePath);

    // Validate a loaded profile
    static ValidationResult validate(const MotorProfile& profile);

    // Get all profiles from standard locations
    static QVector<MotorProfile> loadAllProfiles();

    // Get profile search paths
    static QStringList profileSearchPaths();

    // Builtin default profile
    static MotorProfile builtinDefault();

private:
    static FieldDefinition parseFieldDef(const QJsonObject& obj, QString& error);
    static MotorDescriptor parseMotorDef(const QJsonObject& obj,
                                         const QVector<FieldDefinition>& defaultFields,
                                         QString& error);
    static MotorCommandGroup parseCommandGroup(const QJsonObject& obj, QString& error);
    static CanIdMatcher parseCanIdMatcher(const QJsonObject& obj, QString& error);

    // Parse CAN ID from string ("0x301") or integer (769)
    static uint32_t parseCanId(const QJsonValue& value);

    // Convert profile to JSON
    static QJsonObject profileToJson(const MotorProfile& profile);
    static QJsonObject fieldDefToJson(const FieldDefinition& field);
    static QJsonObject motorDefToJson(const MotorDescriptor& motor);
    static QJsonObject commandGroupToJson(const MotorCommandGroup& group);
};

#endif // MOTOR_PROFILE_LOADER_H
