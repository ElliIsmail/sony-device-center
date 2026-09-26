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
constexpr int kChartDays = 7;
constexpr int kHeartbeatMs = 5 * 60 * 1000;
// Two samples further apart than this were not one continuous session.
constexpr qint64 kMaxGapSeconds = 12 * 60;
// Need at least this much measured drain before trusting the usage rate.
constexpr int kMinDropForUsage = 10;
constexpr double kMinHoursForUsage = 1.0;

qint64 now() { return QDateTime::currentSecsSinceEpoch(); }

using Mode = BatteryMonitor::Mode;

Mode modeFrom(const QString& mode) {
    if (mode == "cancelling") return Mode::Cancelling;
    if (mode == "ambient") return Mode::Ambient;
    if (mode == "off") return Mode::Off;
    return Mode::Unknown;
}

// CSV field for a mode; empty when unknown, which is also how older lines read.
const char* modeField(Mode mode) {
    switch (mode) {
    case Mode::Cancelling: return "nc";
    case Mode::Ambient: return "amb";
    case Mode::Off: return "off";
    case Mode::Unknown: break;
    }
    return "";
}

Mode modeFromField(const QString& field) {
    if (field == "nc") return Mode::Cancelling;
    if (field == "amb") return Mode::Ambient;
    if (field == "off") return Mode::Off;
    return Mode::Unknown;
}

} // namespace

BatteryMonitor::BatteryMonitor(DeviceCenterController* controller, QObject* parent)
    : QObject(parent), _controller(controller) {
    QSettings settings("SonyBridge", "SonyDeviceCenter");
    _alertsEnabled = settings.value("batteryAlerts", true).toBool();
    _firstAlert = settings.value("batteryAlertFirst", 20).toInt();
    _secondAlert = settings.value("batteryAlertSecond", 10).toInt();
    _fullChargeAlert = settings.value("batteryAlertFull", true).toBool();
    if (_secondAlert >= _firstAlert) { _firstAlert = 20; _secondAlert = 10; }

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    _path = dir + "/battery-history.csv";
    _load();

    _heartbeat.setInterval(kHeartbeatMs);
    connect(&_heartbeat, &QTimer::timeout, this, [this] {
        if (_controller->isConnected() && _lastLevel >= 0) _record(_lastLevel, _lastCharging, _lastMode, true);
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

void BatteryMonitor::setAlertLevels(int first, int second) {
    first = std::clamp(first, 6, 95);
    second = std::clamp(second, 5, first - 1);
    if (first == _firstAlert && second == _secondAlert) return;
    _firstAlert = first;
    _secondAlert = second;
    // Re-arm, so a new threshold above the current level waits for the next drop.
    _alertedFirst = _lastLevel >= 0 && _lastLevel <= first;
    _alertedSecond = _lastLevel >= 0 && _lastLevel <= second;
    QSettings settings("SonyBridge", "SonyDeviceCenter");
    settings.setValue("batteryAlertFirst", first);
    settings.setValue("batteryAlertSecond", second);
    emit alertLevelsChanged();
}

void BatteryMonitor::setFullChargeAlert(bool enabled) {
    if (_fullChargeAlert == enabled) return;
    _fullChargeAlert = enabled;
    QSettings settings("SonyBridge", "SonyDeviceCenter");
    settings.setValue("batteryAlertFull", enabled);
    emit fullChargeAlertChanged();
}

void BatteryMonitor::_onState() {
    const bool connected = _controller->isConnected();
    const int level = _controller->batteryLevel();
    const bool charging = _controller->isCharging();
    const Mode mode = modeFrom(_controller->noiseControlMode());

    if (!connected || level < 0) {
        if (_wasConnected) _record(-1, false, Mode::Unknown, true);  // disconnect marker
        _wasConnected = false;
        return;
    }
    _wasConnected = true;
    if (level != _lastLevel || charging != _lastCharging) _checkAlerts(level, charging);
    // Mode changes are recorded too, so time can be split by noise control.
    _record(level, charging, mode);
    _updateEstimate();  // the rated fallback depends on the noise control mode
}

void BatteryMonitor::_record(int level, bool charging, Mode mode, bool force) {
    if (!force && level == _lastLevel && charging == _lastCharging && mode == _lastMode) return;
    if (level >= 0) {
        _lastLevel = level;
        _lastCharging = charging;
        _lastMode = mode;
    } else {
        _lastLevel = -1;
    }
    const Sample sample{now(), level, charging, mode};
    _samples.push_back(sample);
    _append(sample);
    emit historyChanged();
    _updateEstimate();
}

void BatteryMonitor::_checkAlerts(int level, bool charging) {
    // Re-arm on charging or once the level is comfortably back above a threshold.
    if (charging || level > _firstAlert + 5) _alertedFirst = false;
    if (charging || level > _secondAlert + 5) _alertedSecond = false;
    if (!charging) _alertedFull = false;

    if (!_alertsEnabled) return;
    const QString device = _controller->deviceName();
    if (!charging && level <= _secondAlert && !_alertedSecond) {
        _alertedSecond = _alertedFirst = true;
        emit notify(device + " battery low", QString("%1% left. Charge soon.").arg(level));
    } else if (!charging && level <= _firstAlert && !_alertedFirst) {
        _alertedFirst = true;
        emit notify(device + " battery low", QString("%1% left.").arg(level));
    } else if (charging && level >= 100 && !_alertedFull && _fullChargeAlert) {
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

QVector<BatteryMonitor::DayUsage> BatteryMonitor::usageByDay(const QVector<Sample>& samples,
                                                             qint64 nowSecs, int days) {
    // Local midnights, oldest first; the last entry is today.
    QVector<DayUsage> out;
    const QDate today = QDateTime::fromSecsSinceEpoch(nowSecs).date();
    for (int i = days - 1; i >= 0; --i)
        out.push_back({QDateTime(today.addDays(-i), QTime(0, 0)).toSecsSinceEpoch(), 0, 0, 0, 0, 0, false});

    const auto dayOf = [&](qint64 t) -> DayUsage* {
        for (qsizetype i = out.size() - 1; i >= 0; --i)
            if (t >= out[i].dayStart) return &out[i];
        return nullptr;
    };
    for (qsizetype i = 0; i < samples.size(); ++i) {
        const Sample& a = samples[i];
        if (a.level < 0) continue;
        DayUsage* day = dayOf(a.t);
        if (!day) continue;
        if (a.charging) day->charged = true;
        // Same rule as the estimate: only short steps between samples were connected time.
        if (i + 1 >= samples.size() || a.charging || samples[i + 1].t - a.t > kMaxGapSeconds) continue;
        // The mode holds until the next sample, like the level.
        const double hours = (samples[i + 1].t - a.t) / 3600.0;
        day->hours += hours;
        switch (a.mode) {
        case Mode::Cancelling: day->cancelling += hours; break;
        case Mode::Ambient: day->ambient += hours; break;
        case Mode::Off: day->off += hours; break;
        case Mode::Unknown: day->unknown += hours; break;
        }
    }
    return out;
}

QVariantList BatteryMonitor::dailyUsage() const {
    QVariantList out;
    for (const DayUsage& d : usageByDay(_samples, now(), kChartDays))
        out.append(QVariantMap{{"day", d.dayStart * 1000}, {"hours", d.hours},
                               {"cancelling", d.cancelling}, {"ambient", d.ambient},
                               {"off", d.off}, {"unknown", d.unknown}, {"charged", d.charged}});
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
        if (parts.size() != 3 && parts.size() != 4) continue;
        const Sample s{parts[0].toLongLong(), parts[1].toInt(), parts[2] == "1",
                       parts.size() == 4 ? modeFromField(parts[3]) : Mode::Unknown};
        if (s.t < keepFrom) { pruned = true; continue; }
        _samples.push_back(s);
    }
    file.close();
    // The app wasn't running since the last sample, so the headset's state is unknown.
    if (!_samples.isEmpty() && _samples.back().level >= 0) _samples.push_back({now(), -1, false});
    if (pruned) {
        if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            QTextStream out(&file);
            for (const Sample& s : _samples) out << s.t << ',' << s.level << ',' << (s.charging ? 1 : 0) << ',' << modeField(s.mode) << '\n';
        }
    }
}

void BatteryMonitor::_append(const Sample& sample) {
    QFile file(_path);
    if (!file.open(QIODevice::Append | QIODevice::Text)) return;
    QTextStream out(&file);
    out << sample.t << ',' << sample.level << ',' << (sample.charging ? 1 : 0) << ',' << modeField(sample.mode) << '\n';
}

} // namespace sony::devicecenter
