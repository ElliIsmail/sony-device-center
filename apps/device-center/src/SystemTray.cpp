#include "SystemTray.h"
#include "BatteryMonitor.h"
#include "DeviceCenterController.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QFont>
#include <QFontMetricsF>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QSystemTrayIcon>
#include <QWindow>

namespace sony::devicecenter {

namespace {

// Tray icon showing the battery percentage as a coloured badge. Drawn per
// size so the digits stay crisp instead of being downscaled from one bitmap.
QIcon batteryIcon(int level, bool charging) {
    const QColor fill = charging    ? QColor("#3B82F6")
                      : level <= 20 ? QColor("#FF5A5F")
                      : level <= 50 ? QColor("#F2A73B")
                                    : QColor("#2DD4A7");
    const QString text = QString::number(level);

    QIcon icon;
    for (int size : {16, 20, 24, 32, 48, 64}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::TextAntialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        p.drawRoundedRect(QRectF(0, 0, size, size), size * 0.22, size * 0.22);

        // Largest bold font whose text fits the badge with a small margin.
        QFont font("Segoe UI");
        font.setBold(true);
        const qreal room = size * (text.size() > 2 ? 0.96 : 0.86);
        qreal px = size;
        for (; px > 4; px -= 0.5) {
            font.setPixelSize(static_cast<int>(px));
            const QFontMetricsF fm(font);
            if (fm.horizontalAdvance(text) <= room && fm.capHeight() <= size * 0.62) break;
        }
        p.setFont(font);
        p.setPen(QColor("#0A0B0F"));
        p.drawText(QRectF(0, 0, size, size), Qt::AlignCenter, text);
        icon.addPixmap(pixmap);
    }
    return icon;
}

} // namespace

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
    _speakToChat = _menu->addAction(QString());
    _speakToChat->setCheckable(true);
    _menu->addSeparator();
    _quit = _menu->addAction(QString());

    connect(_show, &QAction::triggered, this, &SystemTray::showWindow);
    connect(_anc, &QAction::triggered, _controller, [this] { _controller->setAnc(true); });
    connect(_ambient, &QAction::triggered, _controller,
            [this] { _controller->setAmbient(_controller->ambientLevel(), _controller->focusOnVoice()); });
    connect(_off, &QAction::triggered, _controller, &DeviceCenterController::setNoiseControlOff);
    connect(_speakToChat, &QAction::triggered, _controller, &DeviceCenterController::setSpeakToChat);
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

void SystemTray::showMessage(const QString& title, const QString& message) {
    _icon->showMessage(title, message, QSystemTrayIcon::Information, 8000);
}

void SystemTray::setBatteryMonitor(BatteryMonitor* battery) {
    _battery = battery;
    connect(_battery, &BatteryMonitor::estimateChanged, this, &SystemTray::_refresh);
    _refresh();
}

void SystemTray::_retranslate() {
    _show->setText(_controller->t("tray_show"));
    _anc->setText(_controller->t("noise_cancelling"));
    _ambient->setText(_controller->t("ambient_sound"));
    _off->setText(_controller->t("noise_control_off"));
    _speakToChat->setText(_controller->t("speak_to_chat"));
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
    _speakToChat->setVisible(_controller->hasSpeakToChat());
    _speakToChat->setEnabled(connected);
    _speakToChat->setChecked(_controller->speakToChat());

    const int level = connected ? _controller->batteryLevel() : -1;
    const bool charging = _controller->isCharging();
    // Only repaint when what the badge shows changes; this runs on every snapshot.
    if (level != _shownLevel || charging != _shownCharging) {
        _shownLevel = level;
        _shownCharging = charging;
        _icon->setIcon(level >= 0 ? batteryIcon(level, charging) : QApplication::windowIcon());
    }

    QString tip = "Sony Device Center";
    if (connected) {
        tip += " — " + _controller->deviceName();
        if (level >= 0) {
            tip += QString(" · %1%").arg(level);
            if (charging) tip += " · " + _controller->t("charging");
            else if (_battery && _battery->hoursLeft() >= 0)
                tip += QString(" · ~%1 h left").arg(qRound(_battery->hoursLeft()));
        }
    } else {
        tip += " — " + _controller->t("disconnected");
    }
    _icon->setToolTip(tip);
}

} // namespace sony::devicecenter
