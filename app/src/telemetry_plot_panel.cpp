#include "telemetry_plot_panel.h"

#include <QComboBox>
#include <QDateTime>
#include <QGridLayout>
#include <QLabel>
#include <QSpinBox>

using namespace Qt::StringLiterals;

TelemetryPlotPanel::TelemetryPlotPanel(QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);

    QHBoxLayout* controls = new QHBoxLayout();
    m_metricCombo = new QComboBox(this);
    m_metricCombo->addItem("Current"_L1, QVariant::fromValue(static_cast<int>(Metric::Current)));
    m_metricCombo->addItem("ECD"_L1, QVariant::fromValue(static_cast<int>(Metric::Ecd)));
    m_metricCombo->addItem("Velocity"_L1, QVariant::fromValue(static_cast<int>(Metric::Velocity)));
    connect(m_metricCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TelemetryPlotPanel::onMetricChanged);

    m_motorCombo = new QComboBox(this);
    connect(m_motorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TelemetryPlotPanel::onMotorSelectionChanged);

    m_historySpin = new QSpinBox(this);
    m_historySpin->setRange(50, 2000);
    m_historySpin->setValue(m_historySamples);
    m_historySpin->setSuffix(" pts"_L1);
    connect(m_historySpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        m_historySamples = value;
    });

    controls->addWidget(new QLabel("Metric"_L1, this));
    controls->addWidget(m_metricCombo);
    controls->addWidget(new QLabel("Motor"_L1, this));
    controls->addWidget(m_motorCombo);
    controls->addWidget(new QLabel("History"_L1, this));
    controls->addWidget(m_historySpin);
    controls->addStretch(1);

    layout->addLayout(controls);

    m_chart = new QChart();
    m_chartView = new QChartView(m_chart, this);
    m_chartView->setRenderHint(QPainter::Antialiasing);

    m_series = new QLineSeries(this);
    m_chart->addSeries(m_series);

    m_axisX = new QValueAxis(this);
    m_axisY = new QValueAxis(this);
    m_axisX->setTitleText("Samples"_L1);

    m_chart->setAxisX(m_axisX, m_series);
    m_chart->setAxisY(m_axisY, m_series);

    layout->addWidget(m_chartView, 1);

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(100);
    connect(m_refreshTimer, &QTimer::timeout, this, &TelemetryPlotPanel::refreshChart);
    m_refreshTimer->start();
}

void TelemetryPlotPanel::setProfiles(const QVector<MotorProfile>& profiles)
{
    m_profiles = profiles;
    setActiveProfile(0);
}

void TelemetryPlotPanel::setActiveProfile(int index)
{
    if (index < 0 || index >= m_profiles.size()) {
        m_activeProfile = -1;
        return;
    }
    m_activeProfile = index;
    rebuildMotorOptions();
}

void TelemetryPlotPanel::rebuildMotorOptions()
{
    m_motorCombo->clear();
    m_samples.clear();
    if (m_activeProfile < 0) {
        return;
    }
    const auto& profile = m_profiles[m_activeProfile];
    for (int i = 0; i < profile.motors.size(); ++i) {
        m_motorCombo->addItem(profile.motors[i].label, i);
    }
    m_motorCombo->setCurrentIndex(0);
    m_selectedMotorIndex = 0;
    resetSeries();
}

void TelemetryPlotPanel::resetSeries()
{
    m_series->clear();
    m_samples.clear();
    m_axisX->setRange(0, m_historySamples);
    m_axisY->setRange(-20000, 20000);
}

void TelemetryPlotPanel::appendSample(int motorId, double value)
{
    auto& buffer = m_samples[motorId];
    const qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
    const double x = buffer.points.isEmpty() ? 0 : buffer.points.back().x() + 1;
    buffer.points.enqueue(QPointF(x, value));
    while (buffer.points.size() > m_historySamples) {
        buffer.points.dequeue();
    }
}

double TelemetryPlotPanel::getMetricValue(const MotorMeasure& measure) const
{
    switch (m_currentMetric) {
    case Metric::Current:
        return measure.current;
    case Metric::Ecd:
        return measure.ecd;
    case Metric::Velocity:
        return measure.speed_rpm;
    }
    return 0.0;
}

void TelemetryPlotPanel::onMotorUpdated(int motorId, const MotorMeasure& measure)
{
    if (m_activeProfile < 0) {
        return;
    }
    const auto& profile = m_profiles[m_activeProfile];
    if (motorId < 1 || motorId > profile.motors.size()) {
        return;
    }
    appendSample(motorId, getMetricValue(measure));
}

void TelemetryPlotPanel::refreshChart()
{
    if (m_activeProfile < 0) {
        return;
    }
    const int motorId = m_selectedMotorIndex + 1;
    auto it = m_samples.find(motorId);
    if (it == m_samples.end()) {
        return;
    }
    m_series->replace(it->points);
    if (!it->points.isEmpty()) {
        double minY = it->points.first().y();
        double maxY = minY;
        for (const QPointF& pt : it->points) {
            minY = std::min(minY, pt.y());
            maxY = std::max(maxY, pt.y());
        }
        if (minY == maxY) {
            maxY += 1;
            minY -= 1;
        }
        m_axisY->setRange(minY, maxY);
        m_axisX->setRange(it->points.first().x(), it->points.last().x());
    }
}

void TelemetryPlotPanel::onMetricChanged(int index)
{
    m_currentMetric = static_cast<Metric>(m_metricCombo->currentData().toInt());
    resetSeries();
}

void TelemetryPlotPanel::onMotorSelectionChanged(int index)
{
    m_selectedMotorIndex = index;
    resetSeries();
}
