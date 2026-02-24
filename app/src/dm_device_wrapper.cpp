#include "dm_device_wrapper.h"
#include "bit_extractor.h"

#include <QMetaObject>
#include <QMutexLocker>
#include <QString>

DmDeviceWrapper* DmDeviceWrapper::s_instance = nullptr;

DmDeviceWrapper::DmDeviceWrapper(QObject* parent)
    : QObject(parent)
{
    s_instance = this;
    // Load default profile
    QVector<MotorProfile> profiles = defaultMotorProfiles();
    if (!profiles.isEmpty()) {
        m_activeProfile = profiles.first();
    }
}

DmDeviceWrapper::~DmDeviceWrapper()
{
    close();
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

void DmDeviceWrapper::setActiveProfile(const MotorProfile& profile)
{
    QMutexLocker locker(&m_mutex);
    m_activeProfile = profile;
}

void DmDeviceWrapper::setDeviceType(device_def_t type)
{
    QMutexLocker locker(&m_mutex);
    if (m_open) {
        return;
    }
    m_deviceType = type;
}

void DmDeviceWrapper::setChannel(uint8_t channel)
{
    QMutexLocker locker(&m_mutex);
    m_channel = channel;
}

void DmDeviceWrapper::setBaud(int arbitration, int data, float can_sp, float canfd_sp)
{
    QMutexLocker locker(&m_mutex);
    if (!m_device) {
        return;
    }
    device_channel_set_baud_with_sp(m_device, m_channel, true, arbitration, data, can_sp, canfd_sp);
}

bool DmDeviceWrapper::open()
{
    QMutexLocker locker(&m_mutex);
    if (m_open) {
        return true;
    }

    m_handle = damiao_handle_create(m_deviceType);
    if (!m_handle) {
        emit deviceStatusChanged(false, QStringLiteral("Failed to create SDK handle"));
        return false;
    }

    int device_cnt = damiao_handle_find_devices(m_handle);
    if (device_cnt <= 0) {
        damiao_handle_destroy(m_handle);
        m_handle = nullptr;
        emit deviceStatusChanged(false, QStringLiteral("No device found"));
        return false;
    }

    device_handle* dev_list[16];
    int handle_cnt = 0;
    damiao_handle_get_devices(m_handle, dev_list, &handle_cnt);
    if (handle_cnt <= 0) {
        damiao_handle_destroy(m_handle);
        m_handle = nullptr;
        emit deviceStatusChanged(false, QStringLiteral("No device handle available"));
        return false;
    }

    m_device = dev_list[0];
    if (!device_open(m_device)) {
        damiao_handle_destroy(m_handle);
        m_handle = nullptr;
        m_device = nullptr;
        emit deviceStatusChanged(false, QStringLiteral("Open device failed"));
        return false;
    }

    device_hook_to_rec(m_device, &DmDeviceWrapper::recCallbackThunk);
    device_open_channel(m_device, m_channel);
    m_open = true;
    emit deviceStatusChanged(true, QStringLiteral("Device opened"));
    return true;
}

void DmDeviceWrapper::close()
{
    QMutexLocker locker(&m_mutex);
    if (!m_open) {
        return;
    }
    if (m_device) {
        device_close_channel(m_device, m_channel);
        device_close(m_device);
    }
    if (m_handle) {
        damiao_handle_destroy(m_handle);
    }
    m_device = nullptr;
    m_handle = nullptr;
    m_open = false;
    emit deviceStatusChanged(false, QStringLiteral("Device closed"));
}

bool DmDeviceWrapper::isOpen() const
{
    return m_open;
}

int16_t DmDeviceWrapper::clampValue(int value) const
{
    int32_t min = m_activeProfile.controlLimits.min;
    int32_t max = m_activeProfile.controlLimits.max;
    if (value > max) {
        return static_cast<int16_t>(max);
    }
    if (value < min) {
        return static_cast<int16_t>(min);
    }
    return static_cast<int16_t>(value);
}

void DmDeviceWrapper::sendGroup(int groupIndex, const QVector<int16_t>& values)
{
    QMutexLocker locker(&m_mutex);
    if (!m_open || !m_device) {
        return;
    }
    if (values.size() < 4) {
        return;
    }

    // Get command group from profile
    if (groupIndex < 0 || groupIndex >= m_activeProfile.commandGroups.size()) {
        return;
    }
    const MotorCommandGroup& group = m_activeProfile.commandGroups[groupIndex];

    uint8_t payload[8];

    for (int i = 0; i < 4; ++i) {
        int16_t v = clampValue(values[i]);
        if (group.littleEndian) {
            // Little endian: LSB first
            payload[i * 2] = static_cast<uint8_t>(v & 0xFF);
            payload[i * 2 + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
        } else {
            // Big endian: MSB first
            payload[i * 2] = static_cast<uint8_t>((v >> 8) & 0xFF);
            payload[i * 2 + 1] = static_cast<uint8_t>(v & 0xFF);
        }
    }

    device_channel_send_fast(m_device, m_channel, group.canId, 1, false, false, false, 8, payload);
}

void DmDeviceWrapper::recCallbackThunk(usb_rx_frame_t* frame)
{
    if (!s_instance || !frame) {
        return;
    }
    s_instance->handleRecFrame(frame);
}

int DmDeviceWrapper::matchMotor(uint32_t canId) const
{
    for (int i = 0; i < m_activeProfile.motors.size(); ++i) {
        if (m_activeProfile.motors[i].canIdMatcher.matches(canId)) {
            return i;
        }
    }
    return -1;
}

MotorMeasure DmDeviceWrapper::parseFrame(int motorIndex, const uint8_t* payload) const
{
    MotorMeasure measure;

    if (motorIndex < 0 || motorIndex >= m_activeProfile.motors.size()) {
        return measure;
    }

    const MotorDescriptor& motor = m_activeProfile.motors[motorIndex];

    // Parse each field using bit extractor
    for (const FieldDefinition& field : motor.fields) {
        int32_t rawValue = BitExtractor::extract(
            payload,
            field.byteOffset,
            field.bits.start,
            field.bits.length,
            field.littleEndian,
            field.signedValue
        );

        double scaledValue = static_cast<double>(rawValue) * field.scale;
        measure.fields.insert(field.id, scaledValue);

        // Also populate legacy fields for backward compatibility
        if (field.id == QStringLiteral("ecd")) {
            measure.ecd = static_cast<uint16_t>(rawValue);
        } else if (field.id == QStringLiteral("speed")) {
            measure.speed_rpm = static_cast<int16_t>(rawValue);
        } else if (field.id == QStringLiteral("current")) {
            measure.current = static_cast<int16_t>(rawValue);
        } else if (field.id == QStringLiteral("rotor_temp")) {
            measure.rotor_temperature = static_cast<uint8_t>(rawValue);
        } else if (field.id == QStringLiteral("pcb_temp")) {
            measure.pcb_temperature = static_cast<uint8_t>(rawValue);
        }
    }

    return measure;
}

void DmDeviceWrapper::handleRecFrame(usb_rx_frame_t* frame)
{
    int motorIndex = matchMotor(frame->head.can_id);
    if (motorIndex < 0) {
        return;
    }

    MotorMeasure measure = parseFrame(motorIndex, frame->payload);

    QMetaObject::invokeMethod(this, [this, motorIndex, measure]() {
        emit motorUpdated(motorIndex, measure);
    }, Qt::QueuedConnection);
}
