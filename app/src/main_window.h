#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include <QPointer>
#include <QTableWidget>
#include <QVector>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <QTabWidget>

#include "dm_device_wrapper.h"
#include "motor_profile.h"

class TelemetryDataStore;
class TelemetryDashboard;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

private:
    struct ControlGroup
    {
        QVector<QSlider*> sliders;
        QVector<QSpinBox*> spins;
        QCheckBox* sendOnChange = nullptr;
        QSpinBox* rateSpin = nullptr;
        QPushButton* sendButton = nullptr;
        QTimer* timer = nullptr;
        bool dirty = false;
    };

    QWidget* buildControls();
    QWidget* buildReceiveTable();
    QWidget* buildConnectionBar();
    QWidget* buildControlsTab();

    void applyValuePair(int group, int index, int value);
    QVector<int16_t> groupValues(int group) const;
    void sendGroup(int group);

    void updateStatus(bool ok, const QString& message);
    void updateMotorRow(int motorIndex, const MotorMeasure& measure);

    void loadProfiles();
    void onProfileChanged(int index);
    void applyProfile(const MotorProfile& profile);

    QPointer<DmDeviceWrapper> m_device;
    ControlGroup m_groups[2];
    QTableWidget* m_table = nullptr;

    QComboBox* m_deviceType = nullptr;
    QSpinBox* m_channelSpin = nullptr;
    QSpinBox* m_baudArb = nullptr;
    QSpinBox* m_baudData = nullptr;
    QPushButton* m_openButton = nullptr;
    QPushButton* m_closeButton = nullptr;
    QLabel* m_statusLabel = nullptr;

    // Profile selection
    QComboBox* m_profileCombo = nullptr;
    QVector<MotorProfile> m_profiles;
    MotorProfile m_activeProfile;

    // Telemetry dashboard
    QTabWidget* m_tabWidget = nullptr;
    TelemetryDataStore* m_dataStore = nullptr;
    TelemetryDashboard* m_dashboard = nullptr;
};

#endif
