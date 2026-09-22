#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace sony::protocol {

struct BatteryState {
    std::optional<int> main;
    std::optional<int> left;
    std::optional<int> right;
    std::optional<int> caseBattery;
    bool charging{false};
};

enum class NoiseControlMode {
    Off,
    NoiseCancelling,
    Ambient
};

struct NoiseControlState {
    NoiseControlMode mode{NoiseControlMode::Off};
    int ambientLevel{0};
    bool focusOnVoice{false};
};

// Speak-to-Chat tuning. Codes are the device's own byte values.
struct SpeakToChatConfig {
    int sensitivity{0};   // 0 auto, 1 high, 2 low
    bool voiceFocus{false};
    int timeout{1};       // 0 short, 1 standard, 2 long, 3 never (stays in ambient until resumed)
};

struct EqualizerState {
    int preset{0};
    int clearBass{0};
    std::array<int, 5> bands{0, 0, 0, 0, 0};
};

} // namespace sony::protocol
