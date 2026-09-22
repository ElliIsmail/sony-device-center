#pragma once

#include <QObject>
#include <QPointer>

class QAction;
class QMenu;
class QSystemTrayIcon;
class QWindow;

namespace sony::devicecenter {

class DeviceCenterController;

// Tray icon for the main window: shows the live battery level, restores the
// window on click, mirrors noise control and Speak-to-Chat in its menu, and
// offers the only Quit path once closing the window just hides it
// (DeviceCenterController::minimizeToTray).
class SystemTray : public QObject {
    Q_OBJECT
public:
    SystemTray(DeviceCenterController* controller, QWindow* window, QObject* parent = nullptr);
    ~SystemTray() override;

    void showWindow();

private:
    void _retranslate();
    void _refresh();

    DeviceCenterController* _controller;
    QPointer<QWindow> _window;
    QSystemTrayIcon* _icon{nullptr};
    QMenu* _menu{nullptr};
    QAction* _show{nullptr};
    QAction* _anc{nullptr};
    QAction* _ambient{nullptr};
    QAction* _off{nullptr};
    QAction* _speakToChat{nullptr};
    QAction* _quit{nullptr};
    int _shownLevel{-2};
    bool _shownCharging{false};
};

} // namespace sony::devicecenter
