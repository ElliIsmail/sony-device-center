#pragma once

#include <QAbstractNativeEventFilter>
#include <QHash>
#include <QObject>
#include <QString>
#include <QVariantList>

class QWindow;

namespace sony::devicecenter {

class DeviceCenterController;

// System-wide keyboard shortcuts, configurable from the Automation page and
// saved in the settings. Windows only (RegisterHotKey); elsewhere `available`
// is false and nothing registers.
class HotkeyManager : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    /** [{action, title, shortcut, error}] for the settings list. */
    Q_PROPERTY(QVariantList bindings READ bindings NOTIFY bindingsChanged)

public:
    HotkeyManager(DeviceCenterController* controller, QObject* parent = nullptr);
    ~HotkeyManager() override;

    void setWindow(QWindow* window) { _window = window; }

    [[nodiscard]] static bool available();
    [[nodiscard]] QVariantList bindings() const;

    /**
     * Binds `action` to a key press captured in QML (Qt::Key + modifiers).
     * Returns an error message, or an empty string on success.
     */
    Q_INVOKABLE QString assign(const QString& action, int key, int modifiers);
    Q_INVOKABLE void clear(const QString& action);
    Q_INVOKABLE void resetDefaults();
    /** Releases every shortcut while one is being recorded. */
    Q_INVOKABLE void setSuspended(bool suspended);

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

signals:
    void bindingsChanged();

private:
    struct Binding {
        QString action;
        QString title;
        QString shortcut;  // QKeySequence portable text, e.g. "Ctrl+Alt+N"
        QString error;
        int id{0};
        bool registered{false};
    };

    void _registerAll();
    void _unregisterAll();
    bool _register(Binding& binding);
    void _trigger(const QString& action);
    void _save() const;

    DeviceCenterController* _controller;
    QWindow* _window{nullptr};
    QList<Binding> _bindings;
    bool _suspended{false};
};

} // namespace sony::devicecenter
