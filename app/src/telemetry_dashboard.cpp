#include "telemetry_dashboard.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QSpinBox>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QSplitter>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QHeaderView>

#include <algorithm>

// Color palette for series (colorblind-friendly)
static const QColor kSeriesColors[] = {
    QColor(31, 119, 180),   // Blue
    QColor(255, 127, 14),   // Orange
    QColor(44, 160, 44),    // Green
    QColor(214, 39, 40),    // Red
    QColor(148, 103, 189),  // Purple
    QColor(140, 86, 75),    // Brown
    QColor(227, 119, 194),  // Pink
    QColor(127, 127, 127),  // Gray
    QColor(188, 189, 34),   // Olive
    QColor(23, 190, 207),   // Cyan
};
static const int kNumColors = sizeof(kSeriesColors) / sizeof(kSeriesColors[0]);

// Custom roles for tree items
static const int kMotorIndexRole = Qt::UserRole;
static const int kFieldIdRole = Qt::UserRole + 1;

TelemetryDashboard::TelemetryDashboard(QWidget* parent)
    : QWidget(parent)
{
    setupUi();

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(100);  // 10 Hz refresh
    connect(m_refreshTimer, &QTimer::timeout, this, &TelemetryDashboard::refreshChart);
    m_refreshTimer->start();
}

void TelemetryDashboard::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // Toolbar
    m_toolbar = new QToolBar();
    m_toolbar->setIconSize(QSize(16, 16));

    // History size spinner
    QLabel* historyLabel = new QLabel(QStringLiteral("History:"));
    m_toolbar->addWidget(historyLabel);

    m_historySpin = new QSpinBox();
    m_historySpin->setRange(50, 2000);
    m_historySpin->setValue(200);
    m_historySpin->setSuffix(QStringLiteral(" samples"));
    m_historySpin->setToolTip(QStringLiteral("Number of samples to display"));
    connect(m_historySpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &TelemetryDashboard::onHistoryChanged);
    m_toolbar->addWidget(m_historySpin);

    m_toolbar->addSeparator();

    // Pause button
    m_pauseButton = new QPushButton(QStringLiteral("Pause"));
    m_pauseButton->setCheckable(true);
    connect(m_pauseButton, &QPushButton::clicked, this, &TelemetryDashboard::onPauseClicked);
    m_toolbar->addWidget(m_pauseButton);

    m_toolbar->addSeparator();

    // Y-axis mode
    QLabel* yAxisLabel = new QLabel(QStringLiteral("Y-Axis:"));
    m_toolbar->addWidget(yAxisLabel);

    m_yAxisMode = new QComboBox();
    m_yAxisMode->addItem(QStringLiteral("Auto Scale"));
    m_yAxisMode->addItem(QStringLiteral("Fixed Range"));
    m_yAxisMode->setToolTip(QStringLiteral("Y-axis scaling mode"));
    connect(m_yAxisMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TelemetryDashboard::onYAxisModeChanged);
    m_toolbar->addWidget(m_yAxisMode);

    mainLayout->addWidget(m_toolbar);

    // Splitter with tree on left, chart on right
    m_splitter = new QSplitter(Qt::Horizontal);

    // Series selection tree
    m_seriesTree = new QTreeWidget();
    m_seriesTree->setHeaderLabel(QStringLiteral("Select Series"));
    m_seriesTree->setMinimumWidth(150);
    m_seriesTree->setMaximumWidth(250);
    connect(m_seriesTree, &QTreeWidget::itemChanged,
            this, &TelemetryDashboard::onSeriesToggled);
    m_splitter->addWidget(m_seriesTree);

    // Chart
    m_chart = new QChart();
    m_chart->setAnimationOptions(QChart::NoAnimation);
    m_chart->legend()->setVisible(true);
    m_chart->legend()->setAlignment(Qt::AlignBottom);
    m_chart->setTitle(QStringLiteral("Telemetry"));

    m_axisX = new QValueAxis();
    m_axisX->setTitleText(QStringLiteral("Sample"));
    m_axisX->setLabelFormat("%d");
    m_chart->addAxis(m_axisX, Qt::AlignBottom);

    m_axisY = new QValueAxis();
    m_axisY->setTitleText(QStringLiteral("Value"));
    m_chart->addAxis(m_axisY, Qt::AlignLeft);

    m_chartView = new QChartView(m_chart);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    m_splitter->addWidget(m_chartView);

    // Set splitter proportions (tree: 1, chart: 4)
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 4);

    mainLayout->addWidget(m_splitter, 1);
}

void TelemetryDashboard::setDataStore(TelemetryDataStore* store)
{
    m_dataStore = store;
    if (m_dataStore) {
        m_dataStore->setHistorySize(m_historySpin->value());
    }
}

void TelemetryDashboard::setActiveProfile(const MotorProfile& profile)
{
    m_activeProfile = profile;
    rebuildTree();
}

void TelemetryDashboard::rebuildTree()
{
    // Block signals while rebuilding
    m_seriesTree->blockSignals(true);
    m_seriesTree->clear();

    int numMotors = m_activeProfile.motors.size();
    if (numMotors == 0) {
        numMotors = 8;  // Default fallback
    }

    // Get field definitions from profile
    QVector<FieldDefinition> fields = m_activeProfile.defaultFields;
    if (fields.isEmpty()) {
        // Fallback to standard fields
        FieldDefinition f1; f1.id = QStringLiteral("current"); f1.label = QStringLiteral("Current");
        FieldDefinition f2; f2.id = QStringLiteral("ecd"); f2.label = QStringLiteral("ECD");
        FieldDefinition f3; f3.id = QStringLiteral("speed"); f3.label = QStringLiteral("Velocity");
        fields = {f1, f2, f3};
    }

    // Create tree items for each motor
    for (int motorIdx = 0; motorIdx < numMotors; ++motorIdx) {
        QString motorLabel = (motorIdx < m_activeProfile.motors.size())
                                 ? m_activeProfile.motors[motorIdx].label
                                 : QStringLiteral("Motor %1").arg(motorIdx + 1);

        QTreeWidgetItem* motorItem = new QTreeWidgetItem(m_seriesTree);
        motorItem->setText(0, motorLabel);
        motorItem->setData(0, kMotorIndexRole, motorIdx);
        motorItem->setFlags(motorItem->flags() | Qt::ItemIsAutoTristate);

        // Add field items under each motor
        for (const FieldDefinition& field : fields) {
            QTreeWidgetItem* fieldItem = new QTreeWidgetItem(motorItem);
            fieldItem->setText(0, field.label);
            fieldItem->setData(0, kMotorIndexRole, motorIdx);
            fieldItem->setData(0, kFieldIdRole, field.id);
            fieldItem->setFlags(fieldItem->flags() | Qt::ItemIsUserCheckable);
            fieldItem->setCheckState(0, Qt::Unchecked);
        }
    }

    m_seriesTree->expandAll();
    m_seriesTree->blockSignals(false);
}

void TelemetryDashboard::onSeriesToggled(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);

    // Only handle leaf items (field items, not motor items)
    if (item->childCount() > 0) {
        return;
    }

    int motorIndex = item->data(0, kMotorIndexRole).toInt();
    QString fieldId = item->data(0, kFieldIdRole).toString();

    if (fieldId.isEmpty()) {
        return;
    }

    QString motorLabel = item->parent() ? item->parent()->text(0) : QStringLiteral("Motor");
    QString fieldLabel = item->text(0);
    QString displayName = QStringLiteral("%1 - %2").arg(motorLabel, fieldLabel);

    if (item->checkState(0) == Qt::Checked) {
        addSeries(motorIndex, fieldId, displayName);
    } else {
        removeSeries(motorIndex, fieldId);
    }
}

QColor TelemetryDashboard::nextSeriesColor()
{
    QColor color = kSeriesColors[m_colorIndex % kNumColors];
    m_colorIndex++;
    return color;
}

void TelemetryDashboard::addSeries(int motorIndex, const QString& fieldId, const QString& displayName)
{
    // Check if series already exists
    for (const PlotSeries& ps : m_activeSeries) {
        if (ps.motorIndex == motorIndex && ps.fieldId == fieldId) {
            return;
        }
    }

    // Create new series
    QLineSeries* series = new QLineSeries();
    series->setName(displayName);

    QColor color = nextSeriesColor();
    QPen pen(color);
    pen.setWidth(2);
    series->setPen(pen);

    m_chart->addSeries(series);
    series->attachAxis(m_axisX);
    series->attachAxis(m_axisY);

    PlotSeries ps;
    ps.motorIndex = motorIndex;
    ps.fieldId = fieldId;
    ps.displayName = displayName;
    ps.series = series;
    ps.color = color;
    m_activeSeries.append(ps);
}

void TelemetryDashboard::removeSeries(int motorIndex, const QString& fieldId)
{
    for (int i = 0; i < m_activeSeries.size(); ++i) {
        if (m_activeSeries[i].motorIndex == motorIndex &&
            m_activeSeries[i].fieldId == fieldId) {

            m_chart->removeSeries(m_activeSeries[i].series);
            delete m_activeSeries[i].series;
            m_activeSeries.removeAt(i);
            return;
        }
    }
}

void TelemetryDashboard::refreshChart()
{
    if (m_paused || !m_dataStore || m_activeSeries.isEmpty()) {
        return;
    }

    double xMin = std::numeric_limits<double>::max();
    double xMax = std::numeric_limits<double>::lowest();
    double yMin = std::numeric_limits<double>::max();
    double yMax = std::numeric_limits<double>::lowest();

    for (PlotSeries& ps : m_activeSeries) {
        QVector<QPointF> points = m_dataStore->getSeries(ps.motorIndex, ps.fieldId);
        ps.series->replace(points);

        if (!points.isEmpty()) {
            xMin = std::min(xMin, points.first().x());
            xMax = std::max(xMax, points.last().x());

            if (m_autoScale) {
                for (const QPointF& p : points) {
                    yMin = std::min(yMin, p.y());
                    yMax = std::max(yMax, p.y());
                }
            }
        }
    }

    // Update axis ranges
    if (xMin < xMax) {
        m_axisX->setRange(xMin, xMax);
    }

    if (m_autoScale && yMin < yMax) {
        double padding = (yMax - yMin) * 0.1;
        if (padding < 1.0) padding = 1.0;
        m_axisY->setRange(yMin - padding, yMax + padding);
    }
}

void TelemetryDashboard::setPaused(bool paused)
{
    m_paused = paused;
    m_pauseButton->setChecked(paused);
    m_pauseButton->setText(paused ? QStringLiteral("Resume") : QStringLiteral("Pause"));
}

void TelemetryDashboard::onHistoryChanged(int value)
{
    if (m_dataStore) {
        m_dataStore->setHistorySize(value);
    }
}

void TelemetryDashboard::onPauseClicked()
{
    setPaused(m_pauseButton->isChecked());
}

void TelemetryDashboard::onYAxisModeChanged(int index)
{
    m_autoScale = (index == 0);
    if (!m_autoScale) {
        // Set a reasonable fixed range
        m_axisY->setRange(-20000, 20000);
    }
}

void TelemetryDashboard::updateAxisRanges()
{
    // Called when Y-axis mode changes or profile changes
    if (!m_autoScale) {
        m_axisY->setRange(-20000, 20000);
    }
}
