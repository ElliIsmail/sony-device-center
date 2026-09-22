#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

namespace sony::devicecenter {

class DeviceCenterController;

// Turns Speak-to-Chat off while another app is using the microphone (a call
// in Teams, Zoom, Discord...) so your own voice doesn't flip the headset to
// ambient, then restores it afterwards. Only undoes what it changed: if
// Speak-to-Chat was already off, or you switch it back on mid-call, it leaves
// it alone.
//
// Windows only: it reads the per-app microphone usage that Windows records
// under CapabilityAccessManager\ConsentStore\microphone (the same data behind
// the "app is using your microphone" indicator). Elsewhere `available` is false.
class CallMonitor : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool inCall READ inCall NOTIFY callChanged)
    Q_PROPERTY(QString callApps READ callApps NOTIFY callChanged)
    Q_PROPERTY(bool pausedSpeakToChat READ pausedSpeakToChat NOTIFY callChanged)

public:
    explicit CallMonitor(DeviceCenterController* controller, QObject* parent = nullptr);

    [[nodiscard]] static bool available();
    [[nodiscard]] bool enabled() const { return _enabled; }
    Q_INVOKABLE void setEnabled(bool enabled);
    [[nodiscard]] bool inCall() const { return !_apps.isEmpty(); }
    /** Friendly names of the apps using the microphone, comma separated. */
    [[nodiscard]] QString callApps() const { return _apps.join(", "); }
    [[nodiscard]] bool pausedSpeakToChat() const { return _paused; }

    /** Apps currently using the microphone, by friendly name. */
    static QStringList appsUsingMicrophone();

signals:
    void enabledChanged();
    void callChanged();

private:
    void _poll();
    void _apply();

    DeviceCenterController* _controller;
    QTimer _timer;
    bool _enabled{true};
    QStringList _apps;
    bool _paused{false};         // we switched Speak-to-Chat off and owe a restore
    bool _actedThisCall{false};  // act once per call, so a manual re-enable sticks
    QElapsedTimer _sinceCommand; // our own command's state change is not a user override
};

} // namespace sony::devicecenter
