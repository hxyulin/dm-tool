#ifndef DM_DEVICE_WRAPPER_H
#define DM_DEVICE_WRAPPER_H

#include <QObject>
#include <QMutex>
#include <QVector>
#include <QString>
#include <cstdint>

#include "pub_user.h"

struct MotorMeasure
{
    uint16_t ecd = 0;
    int16_t speed_rpm = 0;
    int16_t current = 0;
    uint8_t rotor_temperature = 0;
    uint8_t pcb_temperature = 0;
};

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

    void sendGroup(int groupIndex, const QVector<int16_t>& values);

signals:
    void deviceStatusChanged(bool ok, const QString& message);
    void motorUpdated(int motorId, MotorMeasure measure);

private:
    static void recCallbackThunk(usb_rx_frame_t* frame);
    void handleRecFrame(usb_rx_frame_t* frame);

    int toMotorId(uint32_t canId) const;
    static int16_t clampValue(int value);

    QMutex m_mutex;
    damiao_handle* m_handle = nullptr;
    device_handle* m_device = nullptr;
    device_def_t m_deviceType = DEV_USB2CANFD_DUAL;
    uint8_t m_channel = 0;
    bool m_open = false;

    static DmDeviceWrapper* s_instance;
};

#endif
