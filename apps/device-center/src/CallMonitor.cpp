#include "CallMonitor.h"
#include "DeviceCenterController.h"

#include <QFileInfo>
#include <QSettings>

#include <iterator>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace sony::devicecenter {

namespace {

constexpr int kPollMs = 2000;
constexpr qint64 kCommandSettleMs = 5000;

#ifdef Q_OS_WIN
constexpr wchar_t kMicrophoneKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\microphone";

// An app is using the microphone right now when it has started and not stopped.
bool inUse(HKEY app) {
    ULONGLONG start = 0, stop = 0;
    DWORD size = sizeof(start);
    if (RegQueryValueExW(app, L"LastUsedTimeStart", nullptr, nullptr, reinterpret_cast<BYTE*>(&start), &size) != ERROR_SUCCESS)
        return false;
    size = sizeof(stop);
    if (RegQueryValueExW(app, L"LastUsedTimeStop", nullptr, nullptr, reinterpret_cast<BYTE*>(&stop), &size) != ERROR_SUCCESS)
        return false;
    return start != 0 && stop == 0;
}

// Subkeys are store-app package families ("MSTeams_8wekyb3d8bbwe") or, under
// NonPackaged, executable paths with '#' for '\' ("C:#...#Discord.exe").
QString friendlyName(const QString& key, bool packaged) {
    if (packaged) {
        QString name = key.section('_', 0, 0);
        return name.section('.', -1);  // "Microsoft.SkypeApp" -> "SkypeApp"
    }
    return QFileInfo(QString(key).replace('#', '/')).completeBaseName();
}

void collect(HKEY parent, bool packaged, QStringList& out) {
    wchar_t name[512];
    for (DWORD i = 0;; ++i) {
        DWORD len = static_cast<DWORD>(std::size(name));
        if (RegEnumKeyExW(parent, i, name, &len, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
        const QString key = QString::fromWCharArray(name, static_cast<int>(len));
        if (packaged && key == "NonPackaged") continue;
        HKEY app;
        if (RegOpenKeyExW(parent, name, 0, KEY_READ, &app) != ERROR_SUCCESS) continue;
        if (inUse(app)) {
            const QString friendly = friendlyName(key, packaged);
            if (!friendly.isEmpty() && !out.contains(friendly)) out << friendly;
        }
        RegCloseKey(app);
    }
}
#endif

} // namespace

CallMonitor::CallMonitor(DeviceCenterController* controller, QObject* parent)
    : QObject(parent), _controller(controller) {
    QSettings settings("SonyBridge", "SonyDeviceCenter");
    _enabled = settings.value("callAutoSwitch", true).toBool();
    if (!available()) return;
    _timer.setInterval(kPollMs);
    connect(&_timer, &QTimer::timeout, this, &CallMonitor::_poll);
    _timer.start();
    // Headphones connecting mid-call, or a command finishing, may unblock a pending change.
    connect(_controller, &DeviceCenterController::stateChanged, this, &CallMonitor::_apply);
    _poll();
}

bool CallMonitor::available() {
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

void CallMonitor::setEnabled(bool enabled) {
    if (_enabled == enabled) return;
    _enabled = enabled;
    QSettings settings("SonyBridge", "SonyDeviceCenter");
    settings.setValue("callAutoSwitch", enabled);
    emit enabledChanged();
    _apply();
}

QStringList CallMonitor::appsUsingMicrophone() {
    QStringList apps;
#ifdef Q_OS_WIN
    HKEY root;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kMicrophoneKey, 0, KEY_READ, &root) != ERROR_SUCCESS) return apps;
    collect(root, true, apps);
    HKEY nonPackaged;
    if (RegOpenKeyExW(root, L"NonPackaged", 0, KEY_READ, &nonPackaged) == ERROR_SUCCESS) {
        collect(nonPackaged, false, apps);
        RegCloseKey(nonPackaged);
    }
    RegCloseKey(root);
#endif
    return apps;
}

void CallMonitor::_poll() {
    const QStringList apps = appsUsingMicrophone();
    if (apps == _apps) return;
    const bool started = _apps.isEmpty() && !apps.isEmpty();
    const bool ended = !_apps.isEmpty() && apps.isEmpty();
    _apps = apps;
    if (started || ended) _actedThisCall = false;
    emit callChanged();
    _apply();
}

void CallMonitor::_apply() {
    if (!_controller->isConnected() || !_controller->hasSpeakToChat() || _controller->busy()) return;

    const bool settling = _sinceCommand.isValid() && _sinceCommand.elapsed() < kCommandSettleMs;

    if (inCall() && _enabled) {
        if (!_actedThisCall && _controller->speakToChat()) {
            _actedThisCall = _paused = true;
            _sinceCommand.start();
            _controller->setSpeakToChat(false);
            emit callChanged();
        } else if (_paused && !settling && _controller->speakToChat()) {
            // Switched back on by hand mid-call: leave it, and don't restore later.
            _paused = false;
            emit callChanged();
        }
        return;
    }

    // Call over, or the feature was switched off mid-call: put back what we changed.
    if (_paused) {
        _paused = false;
        _sinceCommand.start();
        if (!_controller->speakToChat()) _controller->setSpeakToChat(true);
        emit callChanged();
    }
}

} // namespace sony::devicecenter
