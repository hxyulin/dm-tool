#include "main_window.h"
#include "motor_profile_loader.h"
#include "telemetry_data_store.h"
#include "telemetry_dashboard.h"

#include <QApplication>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QSpacerItem>
#include <QVBoxLayout>

namespace {
constexpr int kGroupCount = 2;
constexpr int kMotorsPerGroup = 4;
constexpr int kMotorCount = 8;
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_device(new DmDeviceWrapper(this))
    , m_dataStore(new TelemetryDataStore(this))
{
    setWindowTitle(QStringLiteral("DM CAN Control"));

    // Load profiles
    loadProfiles();

    QWidget* root = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(root);

    layout->addWidget(buildConnectionBar());

    // Create tab widget
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->addTab(buildControlsTab(), QStringLiteral("Controls"));

    // Create and add telemetry dashboard
    m_dashboard = new TelemetryDashboard(this);
    m_dashboard->setDataStore(m_dataStore);
    m_dashboard->setActiveProfile(m_activeProfile);
    m_tabWidget->addTab(m_dashboard, QStringLiteral("Telemetry Dashboard"));

    layout->addWidget(m_tabWidget, 1);

    setCentralWidget(root);

    connect(m_device, &DmDeviceWrapper::deviceStatusChanged, this, &MainWindow::updateStatus);
    connect(m_device, &DmDeviceWrapper::motorUpdated, this, &MainWindow::updateMotorRow);
    connect(m_device, &DmDeviceWrapper::motorUpdated, m_dataStore, &TelemetryDataStore::onMotorUpdated);
}

void MainWindow::loadProfiles()
{
    m_profiles = MotorProfileLoader::loadAllProfiles();
    if (!m_profiles.isEmpty()) {
        m_activeProfile = m_profiles.first();
        m_device->setActiveProfile(m_activeProfile);
    }
}

void MainWindow::onProfileChanged(int index)
{
    if (index < 0 || index >= m_profiles.size()) {
        return;
    }
    applyProfile(m_profiles[index]);
}

void MainWindow::applyProfile(const MotorProfile& profile)
{
    m_activeProfile = profile;
    m_device->setActiveProfile(profile);

    // Update control limits
    int min = profile.controlLimits.min;
    int max = profile.controlLimits.max;
    for (int g = 0; g < kGroupCount; ++g) {
        for (int i = 0; i < kMotorsPerGroup; ++i) {
            if (i < m_groups[g].sliders.size()) {
                m_groups[g].sliders[i]->setRange(min, max);
            }
            if (i < m_groups[g].spins.size()) {
                m_groups[g].spins[i]->setRange(min, max);
            }
        }
    }

    // Update dashboard
    if (m_dashboard) {
        m_dashboard->setActiveProfile(profile);
    }
}

QWidget* MainWindow::buildConnectionBar()
{
    QWidget* bar = new QWidget(this);
    QHBoxLayout* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(0, 0, 0, 0);

    // Profile selector
    m_profileCombo = new QComboBox(bar);
    for (const MotorProfile& profile : m_profiles) {
        m_profileCombo->addItem(profile.name);
    }
    connect(m_profileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onProfileChanged);

    m_deviceType = new QComboBox(bar);
    m_deviceType->addItem(QStringLiteral("USB2CANFD"), DEV_USB2CANFD);
    m_deviceType->addItem(QStringLiteral("USB2CANFD_DUAL"), DEV_USB2CANFD_DUAL);
    m_deviceType->addItem(QStringLiteral("ECAT2CANFD"), DEV_ECAT2CANFD);

    m_channelSpin = new QSpinBox(bar);
    m_channelSpin->setRange(0, 1);
    m_channelSpin->setValue(0);

    m_baudArb = new QSpinBox(bar);
    m_baudArb->setRange(1000, 2000000);
    m_baudArb->setValue(1000000);

    m_baudData = new QSpinBox(bar);
    m_baudData->setRange(1000, 8000000);
    m_baudData->setValue(5000000);

    m_openButton = new QPushButton(QStringLiteral("Open"), bar);
    m_closeButton = new QPushButton(QStringLiteral("Close"), bar);
    m_statusLabel = new QLabel(QStringLiteral("Disconnected"), bar);

    layout->addWidget(new QLabel(QStringLiteral("Profile"), bar));
    layout->addWidget(m_profileCombo);
    layout->addWidget(new QLabel(QStringLiteral("Device"), bar));
    layout->addWidget(m_deviceType);
    layout->addWidget(new QLabel(QStringLiteral("Channel"), bar));
    layout->addWidget(m_channelSpin);
    layout->addWidget(new QLabel(QStringLiteral("Arb Baud"), bar));
    layout->addWidget(m_baudArb);
    layout->addWidget(new QLabel(QStringLiteral("Data Baud"), bar));
    layout->addWidget(m_baudData);
    layout->addWidget(m_openButton);
    layout->addWidget(m_closeButton);
    layout->addWidget(m_statusLabel);
    layout->addStretch(1);

    connect(m_openButton, &QPushButton::clicked, this, [this]() {
        m_device->setDeviceType(static_cast<device_def_t>(m_deviceType->currentData().toInt()));
        m_device->setChannel(static_cast<uint8_t>(m_channelSpin->value()));
        m_device->open();
        m_device->setBaud(m_baudArb->value(), m_baudData->value());
    });

    connect(m_closeButton, &QPushButton::clicked, this, [this]() {
        m_device->close();
    });

    return bar;
}

QWidget* MainWindow::buildControlsTab()
{
    QWidget* container = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(container);

    layout->addWidget(buildControls());
    layout->addWidget(buildReceiveTable());

    return container;
}

QWidget* MainWindow::buildControls()
{
    QWidget* container = new QWidget(this);
    QHBoxLayout* layout = new QHBoxLayout(container);

    int valueMin = m_activeProfile.controlLimits.min;
    int valueMax = m_activeProfile.controlLimits.max;

    for (int g = 0; g < kGroupCount; ++g) {
        QString groupLabel;
        if (g < m_activeProfile.commandGroups.size()) {
            groupLabel = m_activeProfile.commandGroups[g].label;
        } else {
            groupLabel = g == 0 ? QStringLiteral("Group 1-4 (0x3FE)")
                                : QStringLiteral("Group 5-8 (0x4FE)");
        }

        QGroupBox* box = new QGroupBox(groupLabel, container);
        QVBoxLayout* boxLayout = new QVBoxLayout(box);

        QGridLayout* grid = new QGridLayout();
        for (int i = 0; i < kMotorsPerGroup; ++i) {
            QString motorLabel;
            int motorIdx = g * kMotorsPerGroup + i;
            if (motorIdx < m_activeProfile.motors.size()) {
                motorLabel = m_activeProfile.motors[motorIdx].label;
            } else {
                motorLabel = QStringLiteral("Motor %1").arg(motorIdx + 1);
            }

            QLabel* label = new QLabel(motorLabel, box);
            QSlider* slider = new QSlider(Qt::Horizontal, box);
            slider->setRange(valueMin, valueMax);
            slider->setValue(0);

            QSpinBox* spin = new QSpinBox(box);
            spin->setRange(valueMin, valueMax);
            spin->setValue(0);

            grid->addWidget(label, i, 0);
            grid->addWidget(slider, i, 1);
            grid->addWidget(spin, i, 2);

            m_groups[g].sliders.push_back(slider);
            m_groups[g].spins.push_back(spin);

            connect(slider, &QSlider::valueChanged, this, [this, g, i](int value) {
                applyValuePair(g, i, value);
            });
            connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, g, i](int value) {
                applyValuePair(g, i, value);
            });
        }

        m_groups[g].sendOnChange = new QCheckBox(QStringLiteral("Auto send"), box);
        m_groups[g].sendOnChange->setChecked(true);
        m_groups[g].rateSpin = new QSpinBox(box);
        m_groups[g].rateSpin->setRange(1, 500);
        m_groups[g].rateSpin->setValue(20);
        m_groups[g].rateSpin->setSuffix(QStringLiteral(" Hz"));
        m_groups[g].sendButton = new QPushButton(QStringLiteral("Send now"), box);
        m_groups[g].timer = new QTimer(box);
        m_groups[g].timer->setInterval(50);

        boxLayout->addLayout(grid);
        boxLayout->addWidget(m_groups[g].sendOnChange);
        boxLayout->addWidget(m_groups[g].rateSpin);
        boxLayout->addWidget(m_groups[g].sendButton);

        connect(m_groups[g].sendButton, &QPushButton::clicked, this, [this, g]() {
            sendGroup(g);
        });

        connect(m_groups[g].timer, &QTimer::timeout, this, [this, g]() {
            if (!m_groups[g].sendOnChange->isChecked()) {
                return;
            }
            sendGroup(g);
            m_groups[g].dirty = false;
        });
        connect(m_groups[g].rateSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, g](int value) {
            int interval = 1000 / value;
            m_groups[g].timer->setInterval(interval);
        });
        m_groups[g].timer->start();

        layout->addWidget(box, 1);
    }

    return container;
}

QWidget* MainWindow::buildReceiveTable()
{
    QWidget* container = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(container);

    m_table = new QTableWidget(kMotorCount, 6, container);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("Motor"),
        QStringLiteral("ECD"),
        QStringLiteral("Speed"),
        QStringLiteral("Current"),
        QStringLiteral("Rotor Temp"),
        QStringLiteral("PCB Temp")
    });
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    for (int i = 0; i < kMotorCount; ++i) {
        QString motorLabel;
        if (i < m_activeProfile.motors.size()) {
            motorLabel = m_activeProfile.motors[i].label;
        } else {
            motorLabel = QString::number(i + 1);
        }
        m_table->setItem(i, 0, new QTableWidgetItem(motorLabel));
        for (int c = 1; c < 6; ++c) {
            m_table->setItem(i, c, new QTableWidgetItem(QStringLiteral("-")));
        }
    }

    layout->addWidget(m_table);
    return container;
}

void MainWindow::applyValuePair(int group, int index, int value)
{
    QSignalBlocker blocker1(m_groups[group].sliders[index]);
    QSignalBlocker blocker2(m_groups[group].spins[index]);
    m_groups[group].sliders[index]->setValue(value);
    m_groups[group].spins[index]->setValue(value);
    m_groups[group].dirty = true;
}

QVector<int16_t> MainWindow::groupValues(int group) const
{
    QVector<int16_t> values;
    for (int i = 0; i < kMotorsPerGroup; ++i) {
        values.push_back(static_cast<int16_t>(m_groups[group].spins[i]->value()));
    }
    return values;
}

void MainWindow::sendGroup(int group)
{
    if (!m_device) {
        return;
    }
    m_device->sendGroup(group, groupValues(group));
}

void MainWindow::updateStatus(bool ok, const QString& message)
{
    m_statusLabel->setText(message);
    if (ok) {
        m_statusLabel->setStyleSheet(QStringLiteral("color: green;"));
    } else {
        m_statusLabel->setStyleSheet(QStringLiteral("color: red;"));
    }
}

void MainWindow::updateMotorRow(int motorIndex, const MotorMeasure& measure)
{
    if (!m_table) {
        return;
    }
    // motorIndex is now 0-based
    if (motorIndex < 0 || motorIndex >= kMotorCount) {
        return;
    }
    m_table->item(motorIndex, 1)->setText(QString::number(measure.ecd));
    m_table->item(motorIndex, 2)->setText(QString::number(measure.speed_rpm));
    m_table->item(motorIndex, 3)->setText(QString::number(measure.current));
    m_table->item(motorIndex, 4)->setText(QString::number(measure.rotor_temperature));
    m_table->item(motorIndex, 5)->setText(QString::number(measure.pcb_temperature));
}
