#pragma once

#include "IAnimation.h"
#include "Colour.h"
#include "ScopedComPtr.h"

#include <mmdeviceapi.h>
#include <endpointvolume.h>

class MusicReactiveColourTransformAnimation : public IAnimation
{
public:
    MusicReactiveColourTransformAnimation();
    ~MusicReactiveColourTransformAnimation() override;

    std::optional<Frame> GetFrame(uint32_t idx) override;
    bool IsSupportedKB(KeyboardType kbType) const override;
    uint32_t Size() const override;

private:
    void AddFrame(const Frame& frame) override;

    bool InitialiseAudioMeter();
    float QueryLevel();
    Colour HueToRgb(float hue, float brightness) const;

    ScopedComPtr<IMMDeviceEnumerator> m_deviceEnumerator{};
    ScopedComPtr<IMMDevice> m_defaultDevice{};
    ScopedComPtr<IAudioMeterInformation> m_audioMeter{};

    std::wstring m_defaultDeviceId{};

    bool m_comInitialised{ false };
    bool m_audioReady{ false };
    float m_hue{ 0.0f };
};
