#include "stdafx.h"
#include "MusicReactiveColourTransformAnimation.h"

#include <cmath>

#pragma comment(lib, "Mmdevapi.lib")

namespace
{
    constexpr uint32_t kFrameDurationMs = 75; // ~13 FPS keeps audio mode light while staying responsive
    constexpr float kMinBrightness = 0.15f;
    constexpr float kMaxBrightness = 1.0f;
    constexpr float kHueDriftPerFrame = 0.0125f; // base hue drift when quiet
}

MusicReactiveColourTransformAnimation::MusicReactiveColourTransformAnimation()
{
    const auto hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    m_comInitialised = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;

    m_audioReady = InitialiseAudioMeter();
}

MusicReactiveColourTransformAnimation::~MusicReactiveColourTransformAnimation()
{
    if (m_comInitialised)
    {
        CoUninitialize();
    }
}

std::optional<Frame> MusicReactiveColourTransformAnimation::GetFrame(uint32_t /*idx*/)
{
    const float level = QueryLevel();

    // Accelerate hue shift when music gets louder.
    m_hue += kHueDriftPerFrame + (level * 0.08f);
    if (m_hue > 1.0f)
    {
        m_hue -= 1.0f;
    }

    const float brightness = std::clamp(kMinBrightness + level * (kMaxBrightness - kMinBrightness),
                                        kMinBrightness, kMaxBrightness);
    const auto colour = HueToRgb(m_hue, brightness);

    return Frame(Zone::ALL, colour, kFrameDurationMs);
}

bool MusicReactiveColourTransformAnimation::IsSupportedKB(KeyboardType kbType) const
{
    return kbType != KeyboardType::NONE;
}

uint32_t MusicReactiveColourTransformAnimation::Size() const
{
    // Single dynamic frame; PlayAnimation loops forever, requesting fresh values each cycle.
    return 1;
}

void MusicReactiveColourTransformAnimation::AddFrame(const Frame& /*frame*/)
{
    // No-op. Frames are generated dynamically from audio levels.
}

bool MusicReactiveColourTransformAnimation::InitialiseAudioMeter()
{
    IMMDeviceEnumerator* rawEnumerator = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&rawEnumerator));
    if (FAILED(hr))
    {
        return false;
    }

    m_deviceEnumerator.Reset(rawEnumerator);

    IMMDevice* rawDevice = nullptr;
    hr = m_deviceEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &rawDevice);
    if (FAILED(hr))
    {
        return false;
    }

    m_defaultDevice.Reset(rawDevice);

    LPWSTR deviceId = nullptr;
    if (SUCCEEDED(m_defaultDevice->GetId(&deviceId)))
    {
        m_defaultDeviceId = deviceId;
        CoTaskMemFree(deviceId);
    }

    IAudioMeterInformation* rawMeter = nullptr;
    hr = m_defaultDevice->Activate(__uuidof(IAudioMeterInformation), CLSCTX_ALL, nullptr,
                                   reinterpret_cast<void**>(&rawMeter));
    if (FAILED(hr))
    {
        return false;
    }

    m_audioMeter.Reset(rawMeter);
    return true;
}

float MusicReactiveColourTransformAnimation::QueryLevel()
{
    if (!m_audioReady || !m_deviceEnumerator.IsValid())
    {
        return 0.0f;
    }

    IMMDevice* rawDevice = nullptr;
    if (SUCCEEDED(m_deviceEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &rawDevice)))
    {
        LPWSTR deviceId = nullptr;
        if (SUCCEEDED(rawDevice->GetId(&deviceId)))
        {
            if (m_defaultDeviceId != deviceId)
            {
                m_defaultDevice.Reset(rawDevice);
                m_defaultDeviceId = deviceId;

                IAudioMeterInformation* rawMeter = nullptr;
                if (SUCCEEDED(m_defaultDevice->Activate(__uuidof(IAudioMeterInformation), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&rawMeter))))
                {
                    m_audioMeter.Reset(rawMeter);
                }
                else
                {
                    m_audioMeter.Reset(nullptr);
                }
            }
            else
            {
                rawDevice->Release();
            }
            CoTaskMemFree(deviceId);
        }
        else
        {
            rawDevice->Release();
        }
    }

    if (!m_audioMeter.IsValid())
    {
        return 0.0f;
    }

    float peak = 0.0f;
    const HRESULT hr = m_audioMeter->GetPeakValue(&peak);
    if (FAILED(hr))
    {
        return 0.0f;
    }

    return std::clamp(peak, 0.0f, 1.0f);
}

Colour MusicReactiveColourTransformAnimation::HueToRgb(float hue, float brightness) const
{
    // Hue expected in [0,1).
    const float h = std::fmod(std::max(hue, 0.0f), 1.0f) * 6.0f;
    const int sector = static_cast<int>(h);
    const float fractional = h - static_cast<float>(sector);

    const float p = 0.0f;
    const float q = brightness * (1.0f - fractional);
    const float t = brightness * fractional;

    float r = 0.0f, g = 0.0f, b = 0.0f;

    switch (sector)
    {
    case 0: r = brightness; g = t; b = p; break;
    case 1: r = q; g = brightness; b = p; break;
    case 2: r = p; g = brightness; b = t; break;
    case 3: r = p; g = q; b = brightness; break;
    case 4: r = t; g = p; b = brightness; break;
    default: r = brightness; g = p; b = q; break;
    }

    Colour colour{};
    colour[INDEX_COLOUR_RED] = static_cast<uint8_t>(std::clamp(r * 255.0f, 0.0f, 255.0f));
    colour[INDEX_COLOUR_GREEN] = static_cast<uint8_t>(std::clamp(g * 255.0f, 0.0f, 255.0f));
    colour[INDEX_COLOUR_BLUE] = static_cast<uint8_t>(std::clamp(b * 255.0f, 0.0f, 255.0f));

    return colour;
}
