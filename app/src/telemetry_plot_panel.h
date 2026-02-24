#ifndef TELEMETRY_PLOT_PANEL_H
#define TELEMETRY_PLOT_PANEL_H

#include <QWidget>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include <QMap>
#include <QQueue>
#include <QTimer>

#include "motor_profile.h"
#include "dm_device_wrapper.h"

QT_BEGIN_NAMESPACE
class QComboBox;
class QPushButton;
class QSpinBox;
QT_END_NAMESPACE

class TelemetryPlotPanel : public QWidget
{
    Q_OBJECT
public:
    explicit TelemetryPlotPanel(QWidget* parent = nullptr);

    void setProfiles(const QVector<MotorProfile>& profiles);
    void setActiveProfile(int index);

public slots:
    void onMotorUpdated(int motorId, const MotorMeasure& measure);

private slots:
    void refreshChart();
    void onMetricChanged(int index);
    void onMotorSelectionChanged(int index);

private:
    enum class Metric {
        Current,
        Ecd,
        Velocity
    };

    void rebuildMotorOptions();
    void resetSeries();
    void appendSample(int motorId, double value);
    double getMetricValue(const MotorMeasure& measure) const;

    QVector<MotorProfile> m_profiles;
    int m_activeProfile = -1;

    QChart* m_chart = nullptr;
    QChartView* m_chartView = nullptr;
    QLineSeries* m_series = nullptr;
    QValueAxis* m_axisX = nullptr;
    QValueAxis* m_axisY = nullptr;
    QTimer* m_refreshTimer = nullptr;

    QComboBox* m_metricCombo = nullptr;
    QComboBox* m_motorCombo = nullptr;
    QSpinBox* m_historySpin = nullptr;

    Metric m_currentMetric = Metric::Current;
    int m_selectedMotorIndex = 0;
    int m_historySamples = 200;

    struct SampleBuffer {
        QQueue<QPointF> points;
    };

    QMap<int, SampleBuffer> m_samples;
    qint64 m_lastTimestampMs = 0;
};

#endif  // TELEMETRY_PLOT_PANEL_H
