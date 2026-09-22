#include "HotkeyManager.h"
#include "DeviceCenterController.h"

#include <QCoreApplication>
#include <QKeySequence>
#include <QSettings>
#include <QVariantMap>
#include <QWindow>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace sony::devicecenter {

namespace {

constexpr int kFirstId = 0x5D00;

struct ActionSpec {
    const char* action;
    const char* title;
    const char* defaultShortcut;
};

constexpr ActionSpec kActions[] = {
    {"cycleNoiseControl", "Cycle noise control", "Ctrl+Alt+N"},
    {"toggleSpeakToChat", "Toggle Speak-to-Chat", "Ctrl+Alt+S"},
    {"noiseCancelling", "Noise Cancelling", ""},
    {"ambient", "Ambient Sound", ""},
    {"noiseOff", "Noise control off", ""},
    {"showWindow", "Show / hide window", ""},
};

#ifdef Q_OS_WIN
// Qt key -> Windows virtual key for the keys a shortcut can reasonably use.
UINT virtualKey(int key) {
    if ((key >= Qt::Key_A && key <= Qt::Key_Z) || (key >= Qt::Key_0 && key <= Qt::Key_9)) return static_cast<UINT>(key);
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24) return VK_F1 + static_cast<UINT>(key - Qt::Key_F1);
    switch (key) {
    case Qt::Key_Space: return VK_SPACE;
    case Qt::Key_Left: return VK_LEFT;
    case Qt::Key_Right: return VK_RIGHT;
    case Qt::Key_Up: return VK_UP;
    case Qt::Key_Down: return VK_DOWN;
    case Qt::Key_Home: return VK_HOME;
    case Qt::Key_End: return VK_END;
    case Qt::Key_PageUp: return VK_PRIOR;
    case Qt::Key_PageDown: return VK_NEXT;
    case Qt::Key_Insert: return VK_INSERT;
    case Qt::Key_Delete: return VK_DELETE;
    case Qt::Key_Pause: return VK_PAUSE;
    case Qt::Key_Comma: return VK_OEM_COMMA;
    case Qt::Key_Period: return VK_OEM_PERIOD;
    case Qt::Key_Minus: return VK_OEM_MINUS;
    case Qt::Key_Plus: case Qt::Key_Equal: return VK_OEM_PLUS;
    case Qt::Key_MediaPlay: case Qt::Key_MediaTogglePlayPause: return VK_MEDIA_PLAY_PAUSE;
    default: return 0;
    }
}

bool toNative(const QKeySequence& sequence, UINT& mods, UINT& vk) {
    if (sequence.isEmpty()) return false;
    const QKeyCombination combo = sequence[0];
    const auto qtMods = combo.keyboardModifiers();
    mods = MOD_NOREPEAT;
    if (qtMods & Qt::ControlModifier) mods |= MOD_CONTROL;
    if (qtMods & Qt::AltModifier) mods |= MOD_ALT;
    if (qtMods & Qt::ShiftModifier) mods |= MOD_SHIFT;
    if (qtMods & Qt::MetaModifier) mods |= MOD_WIN;
    vk = virtualKey(combo.key());
    return vk != 0;
}
#endif

bool isModifierKey(int key) {
    return key == Qt::Key_Control || key == Qt::Key_Alt || key == Qt::Key_Shift || key == Qt::Key_Meta
        || key == Qt::Key_AltGr || key == Qt::Key_unknown;
}

} // namespace

HotkeyManager::HotkeyManager(DeviceCenterController* controller, QObject* parent)
    : QObject(parent), _controller(controller) {
    QSettings settings("SonyBridge", "SonyDeviceCenter");
    int id = kFirstId;
    for (const auto& spec : kActions) {
        const QString key = QStringLiteral("shortcuts/") + spec.action;
        _bindings.append(Binding{
            .action = spec.action,
            .title = spec.title,
            .shortcut = settings.value(key, QString(spec.defaultShortcut)).toString(),
            .id = id++,
        });
    }
    if (!available()) return;
    QCoreApplication::instance()->installNativeEventFilter(this);
    _registerAll();
}

HotkeyManager::~HotkeyManager() {
    _unregisterAll();
    if (QCoreApplication::instance()) QCoreApplication::instance()->removeNativeEventFilter(this);
}

bool HotkeyManager::available() {
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

QVariantList HotkeyManager::bindings() const {
    QVariantList out;
    for (const auto& b : _bindings) {
        out.append(QVariantMap{
            {"action", b.action},
            {"title", b.title},
            {"shortcut", QKeySequence(b.shortcut, QKeySequence::PortableText).toString(QKeySequence::NativeText)},
            {"error", b.error},
        });
    }
    return out;
}

QString HotkeyManager::assign(const QString& action, int key, int modifiers) {
    if (isModifierKey(key)) return {};  // still holding modifiers; wait for the real key
    const auto mods = Qt::KeyboardModifiers(modifiers) & (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier | Qt::MetaModifier);
    const bool functionKey = key >= Qt::Key_F1 && key <= Qt::Key_F24;
    if (!functionKey && !(mods & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)))
        return "Use Ctrl, Alt or Win with the key (or an F-key)";

    const QString shortcut = QKeySequence(QKeyCombination(mods, Qt::Key(key))).toString(QKeySequence::PortableText);
#ifdef Q_OS_WIN
    UINT nativeMods = 0, vk = 0;
    if (!toNative(QKeySequence(shortcut, QKeySequence::PortableText), nativeMods, vk)) return "That key can't be used for a shortcut";
#endif
    for (auto& b : _bindings) {
        if (b.action != action && b.shortcut == shortcut) return "Already used by “" + b.title + "”";
    }
    for (auto& b : _bindings) {
        if (b.action != action) continue;
        _unregisterAll();
        b.shortcut = shortcut;
        _registerAll();
        _save();
        emit bindingsChanged();
        return b.error;
    }
    return "Unknown action";
}

void HotkeyManager::clear(const QString& action) {
    for (auto& b : _bindings) {
        if (b.action != action) continue;
        _unregisterAll();
        b.shortcut.clear();
        b.error.clear();
        _registerAll();
        _save();
        emit bindingsChanged();
    }
}

void HotkeyManager::resetDefaults() {
    _unregisterAll();
    for (int i = 0; i < _bindings.size(); ++i) _bindings[i].shortcut = kActions[i].defaultShortcut;
    _registerAll();
    _save();
    emit bindingsChanged();
}

void HotkeyManager::setSuspended(bool suspended) {
    // Unregister while recording: a registered combination never reaches the
    // window, so re-recording an existing shortcut would otherwise fire it.
    if (suspended == _suspended) return;
    _suspended = suspended;
    if (suspended) {
        _unregisterAll();
    } else {
        _registerAll();
        emit bindingsChanged();  // registration errors surface now
    }
}

void HotkeyManager::_registerAll() {
    if (_suspended) return;
    for (auto& b : _bindings) _register(b);
}

void HotkeyManager::_unregisterAll() {
#ifdef Q_OS_WIN
    for (auto& b : _bindings) {
        if (b.registered) UnregisterHotKey(nullptr, b.id);
        b.registered = false;
    }
#endif
}

bool HotkeyManager::_register(Binding& binding) {
    binding.error.clear();
    if (binding.shortcut.isEmpty()) return false;
#ifdef Q_OS_WIN
    UINT mods = 0, vk = 0;
    if (!toNative(QKeySequence(binding.shortcut, QKeySequence::PortableText), mods, vk)) {
        binding.error = "Unsupported key";
        return false;
    }
    // Thread-level hotkey: WM_HOTKEY arrives on the GUI thread's queue.
    binding.registered = RegisterHotKey(nullptr, binding.id, mods, vk);
    if (!binding.registered) binding.error = "Taken by another app";
    return binding.registered;
#else
    return false;
#endif
}

bool HotkeyManager::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* /*result*/) {
#ifdef Q_OS_WIN
    if (eventType != "windows_generic_MSG" && eventType != "windows_dispatcher_MSG") return false;
    const auto* msg = static_cast<const MSG*>(message);
    if (msg->message != WM_HOTKEY) return false;
    for (const auto& b : _bindings) {
        if (b.registered && static_cast<WPARAM>(b.id) == msg->wParam) {
            if (!_suspended) _trigger(b.action);
            return true;
        }
    }
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
#endif
    return false;
}

void HotkeyManager::_trigger(const QString& action) {
    if (action == "showWindow") {
        if (!_window) return;
        if (_window->isVisible() && _window->isActive()) {
            _window->hide();
        } else {
            _window->showNormal();
            _window->raise();
            _window->requestActivate();
        }
        return;
    }
    if (!_controller->isConnected()) return;
    if (action == "cycleNoiseControl") _controller->cycleNoiseControl();
    else if (action == "toggleSpeakToChat") _controller->toggleSpeakToChat();
    else if (action == "noiseCancelling" && _controller->hasAnc()) _controller->setAnc(true);
    else if (action == "ambient" && _controller->hasAmbient()) _controller->setAmbient(_controller->ambientLevel(), _controller->focusOnVoice());
    else if (action == "noiseOff") _controller->setNoiseControlOff();
}

void HotkeyManager::_save() const {
    QSettings settings("SonyBridge", "SonyDeviceCenter");
    for (const auto& b : _bindings) settings.setValue("shortcuts/" + b.action, b.shortcut);
}

} // namespace sony::devicecenter
