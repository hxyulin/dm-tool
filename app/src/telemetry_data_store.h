#ifndef TELEMETRY_DATA_STORE_H
#define TELEMETRY_DATA_STORE_H

#include <QObject>
#include <QMutex>
#include <QHash>
#include <QVector>
#include <QPointF>
#include <QSet>

#include "motor_profile.h"

class TelemetryDataStore : public QObject
{
    Q_OBJECT
public:
    enum class Metric {
        Current,
        ECD,
        Velocity
    };
    Q_ENUM(Metric)

    explicit TelemetryDataStore(QObject* parent = nullptr);

    // Configuration
    void setHistorySize(int samples);
    int historySize() const { return m_historySize; }

    // Data access
    QVector<QPointF> getSeries(int motorIndex, Metric metric) const;
    QVector<QPointF> getSeries(int motorIndex, const QString& fieldId) const;

    // Get motors that have been updated since last call
    QSet<int> consumeChangedMotors();

    // Clear all data
    void clear();

public slots:
    void onMotorUpdated(int motorIndex, MotorMeasure measure);

signals:
    void dataUpdated(int motorIndex);

private:
    struct Sample {
        qint64 sampleIndex;
        double current;
        double ecd;
        double velocity;
        QHash<QString, double> fields;
    };

    struct MotorBuffer {
        QVector<Sample> samples;
        qint64 nextSampleIndex = 0;
    };

    mutable QMutex m_mutex;
    QHash<int, MotorBuffer> m_buffers;
    QSet<int> m_changedMotors;
    int m_historySize = 200;
};

#endif // TELEMETRY_DATA_STORE_H
