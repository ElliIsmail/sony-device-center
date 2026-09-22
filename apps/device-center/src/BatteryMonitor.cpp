#include "BatteryMonitor.h"
#include "DeviceCenterController.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <QVariantMap>

#include <algorithm>

namespace sony::devicecenter {

namespace {

constexpr qint64 kKeepSeconds = 14 * 24 * 3600;
constexpr qint64 kChartSeconds = 48 * 3600;
constexpr int kHeartbeatMs = 5 * 60 * 1000;
// Two samples further apart than this were not one continuous session.
constexpr qint64 kMaxGapSeconds = 12 * 60;
// Need at least this much measured drain before trusting the usage rate.
constexpr int kMinDropForUsage = 10;
constexpr double kMinHoursForUsage = 1.0;

qint64 now() { return QDateTime::currentSecsSinceEpoch(); }

} // namespace

BatteryMonitor::BatteryMonitor(DeviceCenterController* controller, QObject* parent)
    : QObject(parent), _controller(controller) {
    QSettings settings("SonyBridge", "SonyDeviceCenter");
    _alertsEnabled = settings.value("batteryAlerts", true).toBool();

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    _path = dir + "/battery-history.csv";
    _load();

    _heartbeat.setInterval(kHeartbeatMs);
    connect(&_heartbeat, &QTimer::timeout, this, [this] {
        if (_controller->isConnected() && _lastLevel >= 0) _record(_lastLevel, _lastCharging, true);
    });
    _heartbeat.start();

    connect(_controller, &DeviceCenterController::stateChanged, this, &BatteryMonitor::_onState);
    _updateEstimate();
}

void BatteryMonitor::setAlertsEnabled(bool enabled) {
    if (_alertsEnabled == enabled) return;
    _alertsEnabled = enabled;
    QSettings settings("SonyBridge", "SonyDeviceCenter");
    settings.setValue("batteryAlerts", enabled);
    emit alertsEnabledChanged();
}

void BatteryMonitor::_onState() {
    const bool connected = _controller->isConnected();
    const int level = _controller->batteryLevel();
    const bool charging = _controller->isCharging();

    if (!connected || level < 0) {
        if (_wasConnected) _record(-1, false, true);  // disconnect marker
        _wasConnected = false;
        return;
    }
    _wasConnected = true;
    if (level != _lastLevel || charging != _lastCharging) {
        _checkAlerts(level, charging);
        _record(level, charging);
    }
    _updateEstimate();  // the rated fallback depends on the noise control mode
}

void BatteryMonitor::_record(int level, bool charging, bool force) {
    if (!force && level == _lastLevel && charging == _lastCharging) return;
    if (level >= 0) {
        _lastLevel = level;
        _lastCharging = charging;
    } else {
        _lastLevel = -1;
    }
    const Sample sample{now(), level, charging};
    _samples.push_back(sample);
    _append(sample);
    emit historyChanged();
    _updateEstimate();
}

void BatteryMonitor::_checkAlerts(int level, bool charging) {
    // Re-arm on charging or once the level is comfortably back above a threshold.
    if (charging || level > 25) _alerted20 = false;
    if (charging || level > 15) _alerted10 = false;
    if (!charging) _alertedFull = false;

    if (!_alertsEnabled) return;
    const QString device = _controller->deviceName();
    if (!charging && level <= 10 && !_alerted10) {
        _alerted10 = _alerted20 = true;
        emit notify(device + " battery low", QString("%1% left. Charge soon.").arg(level));
    } else if (!charging && level <= 20 && !_alerted20) {
        _alerted20 = true;
        emit notify(device + " battery low", QString("%1% left.").arg(level));
    } else if (charging && level >= 100 && !_alertedFull) {
        _alertedFull = true;
        emit notify(device + " fully charged", "You can unplug your headphones.");
    }
}

double BatteryMonitor::estimateFromUsage(const QVector<Sample>& samples, int level) {
    // Walk back through the current discharge (to the last charge) and add up
    // connected time and the level it cost. Gaps (headset off, out of range)
    // count neither time nor drain.
    double hours = 0;
    int drop = 0;
    for (qsizetype i = samples.size() - 1; i > 0; --i) {
        const Sample& b = samples[i];
        const Sample& a = samples[i - 1];
        if (a.charging || b.charging) break;
        if (a.level < 0 || b.level < 0) continue;
        if (b.t - a.t > kMaxGapSeconds) continue;
        hours += (b.t - a.t) / 3600.0;
        drop += std::max(0, a.level - b.level);
    }
    if (drop < kMinDropForUsage || hours < kMinHoursForUsage) return -1;
    const double percentPerHour = drop / hours;
    return level / percentPerHour;
}

double BatteryMonitor::ratedHours() const {
    // Sony's rated battery life with noise cancelling on / off.
    const QString name = _controller->deviceName().toUpper();
    const bool nc = _controller->noiseControlMode() == "cancelling";
    if (name.contains("WH-1000XM4") || name.contains("WH-1000XM3")) return nc ? 30 : 38;
    if (name.contains("WH-1000XM5") || name.contains("WH-1000XM6")) return nc ? 30 : 40;
    if (name.startsWith("WF-")) return nc ? 8 : 12;
    return 30;
}

void BatteryMonitor::_updateEstimate() {
    double hours = -1;
    QString source;
    if (_controller->isConnected() && _lastLevel >= 0 && !_lastCharging) {
        hours = estimateFromUsage(_samples, _lastLevel);
        source = "usage";
        if (hours < 0) {
            hours = ratedHours() * _lastLevel / 100.0;
            source = "rated";
        }
    }
    if (hours != _hoursLeft || source != _estimateSource) {
        _hoursLeft = hours;
        _estimateSource = source;
        emit estimateChanged();
    }
}

QVariantList BatteryMonitor::history() const {
    QVariantList out;
    const qint64 from = now() - kChartSeconds;
    for (const Sample& s : _samples) {
        if (s.t < from) continue;
        out.append(QVariantMap{{"t", s.t * 1000}, {"level", s.level}, {"charging", s.charging}});
    }
    return out;
}

void BatteryMonitor::_load() {
    QFile file(_path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    const qint64 keepFrom = now() - kKeepSeconds;
    QTextStream in(&file);
    bool pruned = false;
    while (!in.atEnd()) {
        const QStringList parts = in.readLine().split(',');
        if (parts.size() != 3) continue;
        const Sample s{parts[0].toLongLong(), parts[1].toInt(), parts[2] == "1"};
        if (s.t < keepFrom) { pruned = true; continue; }
        _samples.push_back(s);
    }
    file.close();
    // The app wasn't running since the last sample, so the headset's state is unknown.
    if (!_samples.isEmpty() && _samples.back().level >= 0) _samples.push_back({now(), -1, false});
    if (pruned) {
        if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            QTextStream out(&file);
            for (const Sample& s : _samples) out << s.t << ',' << s.level << ',' << (s.charging ? 1 : 0) << '\n';
        }
    }
}

void BatteryMonitor::_append(const Sample& sample) {
    QFile file(_path);
    if (!file.open(QIODevice::Append | QIODevice::Text)) return;
    QTextStream out(&file);
    out << sample.t << ',' << sample.level << ',' << (sample.charging ? 1 : 0) << '\n';
}

} // namespace sony::devicecenter
