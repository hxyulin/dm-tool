#include "motor_profile.h"

QVector<FieldDefinition> defaultFieldDefinitions()
{
    QVector<FieldDefinition> fields;

    // ECD (Encoder Count)
    FieldDefinition ecd;
    ecd.id = QStringLiteral("ecd");
    ecd.label = QStringLiteral("Encoder");
    ecd.byteOffset = 0;
    ecd.bits = {0, 16};
    ecd.littleEndian = false;
    ecd.signedValue = false;
    ecd.scale = 1.0;
    ecd.displayLimits = {0, 65535};
    ecd.unit = QStringLiteral("counts");
    fields.push_back(ecd);

    // Speed
    FieldDefinition speed;
    speed.id = QStringLiteral("speed");
    speed.label = QStringLiteral("Speed");
    speed.byteOffset = 2;
    speed.bits = {0, 16};
    speed.littleEndian = false;
    speed.signedValue = true;
    speed.scale = 1.0;
    speed.displayLimits = {-10000, 10000};
    speed.unit = QStringLiteral("rpm");
    fields.push_back(speed);

    // Current
    FieldDefinition current;
    current.id = QStringLiteral("current");
    current.label = QStringLiteral("Current");
    current.byteOffset = 4;
    current.bits = {0, 16};
    current.littleEndian = false;
    current.signedValue = true;
    current.scale = 1.0;
    current.displayLimits = {-20000, 20000};
    current.unit = QStringLiteral("mA");
    fields.push_back(current);

    // Rotor Temperature
    FieldDefinition rotorTemp;
    rotorTemp.id = QStringLiteral("rotor_temp");
    rotorTemp.label = QStringLiteral("Rotor Temp");
    rotorTemp.byteOffset = 6;
    rotorTemp.bits = {0, 8};
    rotorTemp.littleEndian = false;
    rotorTemp.signedValue = false;
    rotorTemp.scale = 1.0;
    rotorTemp.displayLimits = {0, 150};
    rotorTemp.unit = QStringLiteral("C");
    fields.push_back(rotorTemp);

    // PCB Temperature
    FieldDefinition pcbTemp;
    pcbTemp.id = QStringLiteral("pcb_temp");
    pcbTemp.label = QStringLiteral("PCB Temp");
    pcbTemp.byteOffset = 7;
    pcbTemp.bits = {0, 8};
    pcbTemp.littleEndian = false;
    pcbTemp.signedValue = false;
    pcbTemp.scale = 1.0;
    pcbTemp.displayLimits = {0, 150};
    pcbTemp.unit = QStringLiteral("C");
    fields.push_back(pcbTemp);

    return fields;
}

QVector<MotorProfile> defaultMotorProfiles()
{
    QVector<MotorProfile> profiles;

    MotorProfile damiao;
    damiao.version = 1;
    damiao.name = QStringLiteral("Damiao 8-motor (default)");
    damiao.description = QStringLiteral("Default profile for 8 Damiao motors (CAN IDs 0x301-0x308)");
    damiao.controlLimits = {-16384, 16384};
    damiao.defaultFields = defaultFieldDefinitions();

    // Create 8 motors with CAN IDs 0x301-0x308
    for (int i = 0; i < 8; ++i) {
        MotorDescriptor motor;
        motor.label = QStringLiteral("Motor %1").arg(i + 1);
        motor.canIdMatcher.mode = CanIdMatcher::Mode::Exact;
        motor.canIdMatcher.canId = 0x301 + i;
        motor.fields = damiao.defaultFields;  // Copy default fields
        damiao.motors.push_back(motor);
    }

    // Command group 1: Motors 1-4
    MotorCommandGroup group1;
    group1.label = QStringLiteral("Motors 1-4 (0x3FE)");
    group1.canId = 0x3FE;
    group1.motorIndices = {0, 1, 2, 3};
    group1.littleEndian = false;
    damiao.commandGroups.push_back(group1);

    // Command group 2: Motors 5-8
    MotorCommandGroup group2;
    group2.label = QStringLiteral("Motors 5-8 (0x4FE)");
    group2.canId = 0x4FE;
    group2.motorIndices = {4, 5, 6, 7};
    group2.littleEndian = false;
    damiao.commandGroups.push_back(group2);

    profiles.push_back(damiao);
    return profiles;
}
