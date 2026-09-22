#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVector>

namespace sony::devicecenter {

class DeviceCenterController;

// Battery history, time-left estimate and low-battery notifications.
//
// While connected it records a sample on every level/charging change plus a
// heartbeat every few minutes, so connected time can be told apart from time
// the headset spent switched off. History is kept for two weeks in
// <AppData>/battery-history.csv ("epochSeconds,level,charging").
class BatteryMonitor : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
    Q_PROPERTY(double hoursLeft READ hoursLeft NOTIFY estimateChanged)
    Q_PROPERTY(QString estimateSource READ estimateSource NOTIFY estimateChanged)
    /** Sony's rated hours for the connected model in its current mode (full charge). */
    Q_PROPERTY(double ratedHours READ ratedHours NOTIFY estimateChanged)
    Q_PROPERTY(bool alertsEnabled READ alertsEnabled WRITE setAlertsEnabled NOTIFY alertsEnabledChanged)
    /** Two alert thresholds in percent; the second is always below the first. */
    Q_PROPERTY(int firstAlertLevel READ firstAlertLevel NOTIFY alertLevelsChanged)
    Q_PROPERTY(int secondAlertLevel READ secondAlertLevel NOTIFY alertLevelsChanged)

public:
    explicit BatteryMonitor(DeviceCenterController* controller, QObject* parent = nullptr);

    struct Sample {
        qint64 t;      // seconds since epoch
        int level;     // 0-100, -1 marks a disconnect
        bool charging;
    };

    /** Last 48 hours as [{t: ms, level, charging}], for the chart. */
    [[nodiscard]] QVariantList history() const;
    /** Estimated hours of use left, or -1 when unknown. */
    [[nodiscard]] double hoursLeft() const { return _hoursLeft; }
    /** "usage" when measured from your own discharge, "rated" when from the spec sheet. */
    [[nodiscard]] QString estimateSource() const { return _estimateSource; }
    [[nodiscard]] bool alertsEnabled() const { return _alertsEnabled; }
    [[nodiscard]] double ratedHours() const;
    Q_INVOKABLE void setAlertsEnabled(bool enabled);
    [[nodiscard]] int firstAlertLevel() const { return _firstAlert; }
    [[nodiscard]] int secondAlertLevel() const { return _secondAlert; }
    /** Sets both levels; they are clamped to 5-95 and kept at least 5 apart. */
    Q_INVOKABLE void setAlertLevels(int first, int second);

    // Exposed for tests: estimate from samples, given the current level.
    static double estimateFromUsage(const QVector<Sample>& samples, int level);

signals:
    void historyChanged();
    void estimateChanged();
    void alertsEnabledChanged();
    void alertLevelsChanged();
    /** A notification the tray should show. */
    void notify(const QString& title, const QString& message);

private:
    void _onState();
    void _record(int level, bool charging, bool force = false);
    void _checkAlerts(int level, bool charging);
    void _updateEstimate();
    void _load();
    void _append(const Sample& sample);

    DeviceCenterController* _controller;
    QVector<Sample> _samples;
    QString _path;
    QTimer _heartbeat;
    double _hoursLeft{-1};
    QString _estimateSource;
    bool _alertsEnabled{true};
    bool _wasConnected{false};
    int _lastLevel{-1};
    bool _lastCharging{false};
    int _firstAlert{20};
    int _secondAlert{10};
    // Alert latches, re-armed after charging or a rise above the threshold.
    bool _alertedFirst{false};
    bool _alertedSecond{false};
    bool _alertedFull{false};
};

} // namespace sony::devicecenter
