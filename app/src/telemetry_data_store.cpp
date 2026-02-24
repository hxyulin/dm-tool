#include "telemetry_data_store.h"
#include <QMutexLocker>

TelemetryDataStore::TelemetryDataStore(QObject* parent)
    : QObject(parent)
{
}

void TelemetryDataStore::setHistorySize(int samples)
{
    QMutexLocker locker(&m_mutex);
    m_historySize = qBound(50, samples, 2000);
}

void TelemetryDataStore::onMotorUpdated(int motorIndex, MotorMeasure measure)
{
    QMutexLocker locker(&m_mutex);

    MotorBuffer& buffer = m_buffers[motorIndex];

    Sample sample;
    sample.sampleIndex = buffer.nextSampleIndex++;
    sample.current = static_cast<double>(measure.current);
    sample.ecd = static_cast<double>(measure.ecd);
    sample.velocity = static_cast<double>(measure.speed_rpm);
    sample.fields = measure.fields;

    buffer.samples.append(sample);

    // Trim to history size
    while (buffer.samples.size() > m_historySize) {
        buffer.samples.removeFirst();
    }

    m_changedMotors.insert(motorIndex);

    locker.unlock();
    emit dataUpdated(motorIndex);
}

QVector<QPointF> TelemetryDataStore::getSeries(int motorIndex, Metric metric) const
{
    QMutexLocker locker(&m_mutex);

    QVector<QPointF> points;
    auto it = m_buffers.constFind(motorIndex);
    if (it == m_buffers.constEnd()) {
        return points;
    }

    const MotorBuffer& buffer = it.value();
    points.reserve(buffer.samples.size());

    for (const Sample& s : buffer.samples) {
        double y = 0.0;
        switch (metric) {
        case Metric::Current:
            y = s.current;
            break;
        case Metric::ECD:
            y = s.ecd;
            break;
        case Metric::Velocity:
            y = s.velocity;
            break;
        }
        points.append(QPointF(static_cast<double>(s.sampleIndex), y));
    }

    return points;
}

QVector<QPointF> TelemetryDataStore::getSeries(int motorIndex, const QString& fieldId) const
{
    QMutexLocker locker(&m_mutex);

    QVector<QPointF> points;
    auto it = m_buffers.constFind(motorIndex);
    if (it == m_buffers.constEnd()) {
        return points;
    }

    const MotorBuffer& buffer = it.value();
    points.reserve(buffer.samples.size());

    for (const Sample& s : buffer.samples) {
        double y = 0.0;
        // Check legacy fields first
        if (fieldId == QStringLiteral("current")) {
            y = s.current;
        } else if (fieldId == QStringLiteral("ecd")) {
            y = s.ecd;
        } else if (fieldId == QStringLiteral("speed")) {
            y = s.velocity;
        } else {
            // Fall back to dynamic fields
            y = s.fields.value(fieldId, 0.0);
        }
        points.append(QPointF(static_cast<double>(s.sampleIndex), y));
    }

    return points;
}

QSet<int> TelemetryDataStore::consumeChangedMotors()
{
    QMutexLocker locker(&m_mutex);
    QSet<int> changed = m_changedMotors;
    m_changedMotors.clear();
    return changed;
}

void TelemetryDataStore::clear()
{
    QMutexLocker locker(&m_mutex);
    m_buffers.clear();
    m_changedMotors.clear();
}
