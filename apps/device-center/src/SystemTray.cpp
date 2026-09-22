#include "SystemTray.h"
#include "DeviceCenterController.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QWindow>

namespace sony::devicecenter {

SystemTray::SystemTray(DeviceCenterController* controller, QWindow* window, QObject* parent)
    : QObject(parent), _controller(controller), _window(window) {
    _menu = new QMenu();
    _show = _menu->addAction(QString());
    _menu->addSeparator();

    auto* modes = new QActionGroup(this);
    for (auto** action : {&_anc, &_ambient, &_off}) {
        *action = _menu->addAction(QString());
        (*action)->setCheckable(true);
        modes->addAction(*action);
    }
    _menu->addSeparator();
    _quit = _menu->addAction(QString());

    connect(_show, &QAction::triggered, this, &SystemTray::showWindow);
    connect(_anc, &QAction::triggered, _controller, [this] { _controller->setAnc(true); });
    connect(_ambient, &QAction::triggered, _controller,
            [this] { _controller->setAmbient(_controller->ambientLevel(), _controller->focusOnVoice()); });
    connect(_off, &QAction::triggered, _controller, &DeviceCenterController::setNoiseControlOff);
    connect(_quit, &QAction::triggered, qApp, &QCoreApplication::quit);

    _icon = new QSystemTrayIcon(QApplication::windowIcon(), this);
    _icon->setContextMenu(_menu);
    connect(_icon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) showWindow();
    });

    connect(_controller, &DeviceCenterController::stateChanged, this, &SystemTray::_refresh);
    connect(_controller, &DeviceCenterController::capabilitiesChanged, this, &SystemTray::_refresh);
    connect(_controller, &DeviceCenterController::languageChanged, this, &SystemTray::_retranslate);

    _retranslate();
    _icon->show();
}

SystemTray::~SystemTray() {
    delete _menu;
}

void SystemTray::showWindow() {
    if (!_window) return;
    _window->showNormal();
    _window->raise();
    _window->requestActivate();
}

void SystemTray::_retranslate() {
    _show->setText(_controller->t("tray_show"));
    _anc->setText(_controller->t("noise_cancelling"));
    _ambient->setText(_controller->t("ambient_sound"));
    _off->setText(_controller->t("noise_control_off"));
    _quit->setText(_controller->t("tray_quit"));
    _refresh();
}

void SystemTray::_refresh() {
    const bool connected = _controller->isConnected();
    const QString mode = _controller->noiseControlMode();
    _anc->setEnabled(connected && _controller->hasAnc());
    _ambient->setEnabled(connected && _controller->hasAmbient());
    _off->setEnabled(connected && (_controller->hasAnc() || _controller->hasAmbient()));
    _anc->setChecked(mode == "cancelling");
    _ambient->setChecked(mode == "ambient");
    _off->setChecked(mode == "off");

    QString tip = "Sony Device Center";
    if (connected) {
        tip += " — " + _controller->deviceName();
        if (_controller->batteryLevel() >= 0) tip += QString(" · %1%").arg(_controller->batteryLevel());
    } else {
        tip += " — " + _controller->t("disconnected");
    }
    _icon->setToolTip(tip);
}

} // namespace sony::devicecenter
