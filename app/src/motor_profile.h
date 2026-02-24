#ifndef MOTOR_PROFILE_H
#define MOTOR_PROFILE_H

#include <QString>
#include <QVector>
#include <QHash>

#include <cstdint>

// ============================================================================
// New flexible field-based configuration system
// ============================================================================

struct BitRange
{
    int start = 0;    // Starting bit position within the extracted bytes
    int length = 16;  // Number of bits to extract
};

struct DisplayLimits
{
    double min = 0.0;
    double max = 65535.0;
};

struct FieldDefinition
{
    QString id;              // Unique identifier: "ecd", "speed", "current", etc.
    QString label;           // Display label
    int byteOffset = 0;      // Starting byte offset in CAN frame (0-7)
    BitRange bits;           // Bit extraction parameters
    bool littleEndian = false;
    bool signedValue = false;
    double scale = 1.0;      // Value multiplier for display
    DisplayLimits displayLimits;
    QString unit;            // Unit string: "rpm", "mA", "C", etc.
};

struct CanIdMatcher
{
    enum class Mode { Exact, Mask };
    Mode mode = Mode::Exact;
    uint32_t canId = 0;      // For exact match
    uint32_t mask = 0;       // For mask match: (receivedId & mask) == value
    uint32_t value = 0;      // For mask match

    bool matches(uint32_t receivedId) const
    {
        if (mode == Mode::Exact) {
            return receivedId == canId;
        }
        return (receivedId & mask) == value;
    }
};

struct ControlLimits
{
    int32_t min = -16384;
    int32_t max = 16384;
};

// ============================================================================
// Motor and profile configuration
// ============================================================================

struct MotorDescriptor
{
    QString label;
    CanIdMatcher canIdMatcher;
    QVector<FieldDefinition> fields;           // Effective fields for this motor
    QHash<QString, FieldDefinition> fieldOverrides;  // Per-motor field overrides
};

struct MotorCommandGroup
{
    QString label;
    uint32_t canId = 0;
    QVector<int> motorIndices;
    bool littleEndian = false;  // Command byte order for this group
};

struct MotorProfile
{
    int version = 1;
    QString name;
    QString description;
    QString filePath;                          // Source file (empty for builtin)
    ControlLimits controlLimits;
    QVector<FieldDefinition> defaultFields;    // Profile-wide field definitions
    QVector<MotorDescriptor> motors;
    QVector<MotorCommandGroup> commandGroups;
};

// ============================================================================
// Dynamic motor measurement (supports arbitrary fields)
// ============================================================================

struct MotorMeasure
{
    // Legacy fixed fields for backward compatibility
    uint16_t ecd = 0;
    int16_t speed_rpm = 0;
    int16_t current = 0;
    uint8_t rotor_temperature = 0;
    uint8_t pcb_temperature = 0;

    // Dynamic field storage (field_id -> scaled value)
    QHash<QString, double> fields;

    // Get field by ID (returns from dynamic fields, falls back to legacy)
    double field(const QString& id) const
    {
        if (fields.contains(id)) {
            return fields.value(id);
        }
        // Fallback to legacy fields
        if (id == "ecd") return static_cast<double>(ecd);
        if (id == "speed") return static_cast<double>(speed_rpm);
        if (id == "current") return static_cast<double>(current);
        if (id == "rotor_temp") return static_cast<double>(rotor_temperature);
        if (id == "pcb_temp") return static_cast<double>(pcb_temperature);
        return 0.0;
    }
};

// ============================================================================
// Profile loading utilities
// ============================================================================

// Get builtin default profiles (backward compatibility)
QVector<MotorProfile> defaultMotorProfiles();

// Create default field definitions for standard motor telemetry
QVector<FieldDefinition> defaultFieldDefinitions();

#endif // MOTOR_PROFILE_H
