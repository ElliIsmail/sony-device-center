#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>
#include <QSystemTrayIcon>
#include <QWindow>
#include <memory>

#include "DeviceCenterController.h"
#include "LocalApi.h"
#include "SystemTray.h"

int main(int argc, char *argv[]) {
    // QApplication rather than QGuiApplication: the tray icon's context menu
    // is a QMenu.
    QApplication app(argc, argv);

    // Main.qml customises background/handle/indicator on its controls. The
    // native "Windows" and "macOS" styles Qt picks by default there refuse
    // that (one warning per control, and native-looking widgets), so pin the
    // one style that honours customisation on every platform.
    QQuickStyle::setStyle("Basic");
    app.setApplicationName("Sony Device Center");
    app.setOrganizationName("SonyBridge");
    app.setApplicationVersion(SONY_DEVICE_CENTER_VERSION);

    // Wayland and the GNOME/KDE shells match a window to its .desktop entry by
    // this name; without it the taskbar falls back to a generic placeholder
    // even though the window icon below is set.
    QGuiApplication::setDesktopFileName("sony-device-center");

    // The raster form is used deliberately: QIcon can only read the SVG brand
    // asset when Qt's qsvg image plugin is deployed alongside the binary.
    app.setWindowIcon(QIcon(":/resources/brand/app-icon.png"));

    sony::devicecenter::DeviceCenterController controller;
    // Local HTTP API for the Stream Deck plugin; follows the Settings switch.
    sony::devicecenter::LocalApi localApi(&controller);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("controller", &controller);

    const QUrl url(QStringLiteral("qrc:/qml/Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);

    engine.load(url);

    std::unique_ptr<sony::devicecenter::SystemTray> tray;
    if (QSystemTrayIcon::isSystemTrayAvailable() && !engine.rootObjects().isEmpty()) {
        if (auto *window = qobject_cast<QWindow *>(engine.rootObjects().constFirst()))
            tray = std::make_unique<sony::devicecenter::SystemTray>(&controller, window);
    }

    return app.exec();
}
