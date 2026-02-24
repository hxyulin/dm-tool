#ifndef TELEMETRY_DASHBOARD_H
#define TELEMETRY_DASHBOARD_H

#include <QWidget>
#include <QVector>
#include <QTimer>
#include <QColor>

#include <QChart>
#include <QChartView>
#include <QLineSeries>
#include <QValueAxis>

#include "motor_profile.h"
#include "telemetry_data_store.h"

class QTreeWidget;
class QTreeWidgetItem;
class QToolBar;
class QSpinBox;
class QPushButton;
class QComboBox;
class QSplitter;

class TelemetryDashboard : public QWidget
{
    Q_OBJECT
public:
    explicit TelemetryDashboard(QWidget* parent = nullptr);

    void setDataStore(TelemetryDataStore* store);
    void setActiveProfile(const MotorProfile& profile);

public slots:
    void refreshChart();
    void setPaused(bool paused);

private slots:
    void onHistoryChanged(int value);
    void onPauseClicked();
    void onYAxisModeChanged(int index);
    void onSeriesToggled(QTreeWidgetItem* item, int column);

private:
    void setupUi();
    void rebuildTree();
    void addSeries(int motorIndex, const QString& fieldId, const QString& displayName);
    void removeSeries(int motorIndex, const QString& fieldId);
    QColor nextSeriesColor();
    void updateAxisRanges();

    // Series tracking
    struct PlotSeries {
        int motorIndex;
        QString fieldId;
        QString displayName;
        QLineSeries* series;
        QColor color;
    };

    QVector<PlotSeries> m_activeSeries;
    int m_colorIndex = 0;

    // Data
    TelemetryDataStore* m_dataStore = nullptr;
    MotorProfile m_activeProfile;

    // Chart components
    QChart* m_chart = nullptr;
    QChartView* m_chartView = nullptr;
    QValueAxis* m_axisX = nullptr;
    QValueAxis* m_axisY = nullptr;

    // UI components
    QSplitter* m_splitter = nullptr;
    QTreeWidget* m_seriesTree = nullptr;
    QTimer* m_refreshTimer = nullptr;

    // Toolbar controls
    QToolBar* m_toolbar = nullptr;
    QSpinBox* m_historySpin = nullptr;
    QPushButton* m_pauseButton = nullptr;
    QComboBox* m_yAxisMode = nullptr;

    bool m_paused = false;
    bool m_autoScale = true;
};

#endif // TELEMETRY_DASHBOARD_H
