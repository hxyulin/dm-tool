#include "dm_device_wrapper.h"

#include <QMetaObject>
#include <QMutexLocker>
#include <QString>

DmDeviceWrapper* DmDeviceWrapper::s_instance = nullptr;

DmDeviceWrapper::DmDeviceWrapper(QObject* parent)
    : QObject(parent)
{
    s_instance = this;
}

DmDeviceWrapper::~DmDeviceWrapper()
{
    close();
    if (s_instance == this) {
        s_instance = nullptr;
    }
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

int16_t DmDeviceWrapper::clampValue(int value)
{
    if (value > 16384) {
        return 16384;
    }
    if (value < -16384) {
        return -16384;
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

    uint32_t can_id = (groupIndex == 0) ? 0x3FE : 0x4FE;
    uint8_t payload[8];

    for (int i = 0; i < 4; ++i) {
        int16_t v = clampValue(values[i]);
        payload[i * 2] = static_cast<uint8_t>((v >> 8) & 0xFF);
        payload[i * 2 + 1] = static_cast<uint8_t>(v & 0xFF);
    }

    device_channel_send_fast(m_device, m_channel, can_id, 1, false, false, false, 8, payload);
}

void DmDeviceWrapper::recCallbackThunk(usb_rx_frame_t* frame)
{
    if (!s_instance || !frame) {
        return;
    }
    s_instance->handleRecFrame(frame);
}

int DmDeviceWrapper::toMotorId(uint32_t canId) const
{
    if (canId >= 0x301 && canId <= 0x308) {
        return static_cast<int>(canId - 0x300);
    }
    return -1;
}

void DmDeviceWrapper::handleRecFrame(usb_rx_frame_t* frame)
{
    int motorId = toMotorId(frame->head.can_id);
    if (motorId < 0) {
        return;
    }

    MotorMeasure measure;
    const uint8_t* data = frame->payload;
    measure.ecd = static_cast<uint16_t>(data[0] << 8 | data[1]);
    measure.speed_rpm = static_cast<int16_t>(data[2] << 8 | data[3]);
    measure.current = static_cast<int16_t>(data[4] << 8 | data[5]);
    measure.rotor_temperature = data[6];
    measure.pcb_temperature = data[7];

    QMetaObject::invokeMethod(this, [this, motorId, measure]() {
        emit motorUpdated(motorId, measure);
    }, Qt::QueuedConnection);
}
