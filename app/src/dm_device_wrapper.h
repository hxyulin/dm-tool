#ifndef DM_DEVICE_WRAPPER_H
#define DM_DEVICE_WRAPPER_H

#include <QObject>
#include <QMutex>
#include <QVector>
#include <QString>
#include <cstdint>

#include "pub_user.h"
#include "motor_profile.h"

class DmDeviceWrapper : public QObject
{
    Q_OBJECT
public:
    explicit DmDeviceWrapper(QObject* parent = nullptr);
    ~DmDeviceWrapper() override;

    bool open();
    void close();

    bool isOpen() const;

    void setDeviceType(device_def_t type);
    void setChannel(uint8_t channel);
    void setBaud(int arbitration, int data, float can_sp = 0.75f, float canfd_sp = 0.75f);

    // Profile management
    void setActiveProfile(const MotorProfile& profile);
    const MotorProfile& activeProfile() const { return m_activeProfile; }

    // Send motor command group (uses profile for CAN ID and endianness)
    void sendGroup(int groupIndex, const QVector<int16_t>& values);

signals:
    void deviceStatusChanged(bool ok, const QString& message);
    void motorUpdated(int motorIndex, MotorMeasure measure);

private:
    static void recCallbackThunk(usb_rx_frame_t* frame);
    void handleRecFrame(usb_rx_frame_t* frame);

    // Find motor index by CAN ID using profile matchers
    int matchMotor(uint32_t canId) const;

    // Parse frame using profile field definitions
    MotorMeasure parseFrame(int motorIndex, const uint8_t* payload) const;

    // Clamp value to profile control limits
    int16_t clampValue(int value) const;

    QMutex m_mutex;
    damiao_handle* m_handle = nullptr;
    device_handle* m_device = nullptr;
    device_def_t m_deviceType = DEV_USB2CANFD_DUAL;
    uint8_t m_channel = 0;
    bool m_open = false;

    MotorProfile m_activeProfile;

    static DmDeviceWrapper* s_instance;
};

#endif
