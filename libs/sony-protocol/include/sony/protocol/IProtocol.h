#pragma once

#include "SemanticTypes.h"
#include "SonyError.h"
#include <array>
#include <string>

namespace sony::protocol {

enum class ProtocolGeneration {
    V1,
    V2
};

class IProtocol {
public:
    virtual ~IProtocol() = default;

    [[nodiscard]] virtual ProtocolGeneration generation() const noexcept = 0;

    virtual void initDevice() = 0;

    virtual BatteryState getBattery() = 0;

    virtual NoiseControlState getNoiseControl() = 0;
    virtual void setNoiseControl(const NoiseControlState& state) = 0;

    virtual EqualizerState getEqualizer() = 0;
    virtual void setEqualizerPreset(int preset) = 0;
    virtual void setEqualizerCustom(int clearBass, const std::array<int, 5>& bands) = 0;

    virtual bool getDsee() = 0;
    virtual void setDsee(bool enabled) = 0;

    virtual std::string getFirmwareVersion() = 0;
    virtual std::string getCodec() = 0;

    virtual int getAutoPowerOff() = 0;
    virtual void setAutoPowerOff(int index) = 0;

    virtual bool getSpeakToChat() = 0;
    virtual void setSpeakToChat(bool enabled) = 0;

    virtual bool getAdaptiveVolume() = 0;
    virtual void setAdaptiveVolume(bool enabled) = 0;

    // Optional settings only some generations implement. The defaults report
    // Unsupported so callers can treat them like any other missing feature.
    virtual SpeakToChatConfig getSpeakToChatConfig() { throw SonyException(SonyErrorCode::Unsupported, "Speak-to-Chat tuning is not supported"); }
    virtual void setSpeakToChatConfig(const SpeakToChatConfig& /*config*/) { throw SonyException(SonyErrorCode::Unsupported, "Speak-to-Chat tuning is not supported"); }
    virtual bool getPauseWhenTakenOff() { throw SonyException(SonyErrorCode::Unsupported, "Pause when taken off is not supported"); }
    virtual void setPauseWhenTakenOff(bool /*enabled*/) { throw SonyException(SonyErrorCode::Unsupported, "Pause when taken off is not supported"); }
};

} // namespace sony::protocol
