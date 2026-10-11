#include "stdafx.h"
#include <random>
#include <chrono>
#include "FireFlickerAnimation.h"
#include "ColourFactory.h"

namespace
{
    constexpr double kMinBrightness = 0.65;   // darkest ember
    constexpr double kMaxBrightness = 1.00;   // base intensity
    constexpr uint32_t kBaseFrameMs = 80;     // default flicker frame length
    constexpr uint32_t kFlashMs = 30;         // spark duration
    constexpr double kSparkChance = 0.12;     // ~12% of frames are bright sparks
}

FireFlickerAnimation::FireFlickerAnimation()
{
    ColourFactory factory{};
    const Colour base = factory.Create(255, 106, 0); // warm orange base (#FF6A00)

    // RNG seeded with steady_clock for variation per run.
    const auto seed = static_cast<uint32_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> brightnessDist(kMinBrightness, kMaxBrightness);
    std::uniform_real_distribution<double> sparkDist(0.0, 1.0);

    // Build a few seconds of flicker; animation loops in Keyboard::PlayAnimation.
    for (int i = 0; i < 160; ++i)
    {
        const bool isSpark = sparkDist(rng) < kSparkChance;
        if (isSpark)
        {
            // Quick white flash.
            m_frames.AddFrame(Zone::ALL, Colour{255, 255, 255}, kFlashMs);
            continue;
        }

        const double b = brightnessDist(rng);
        AddFlickerFrame(base, b, kBaseFrameMs);
    }
}

void FireFlickerAnimation::AddFlickerFrame(const Colour& base, double brightness, uint32_t ms)
{
    Colour c{};
    for (size_t i = 0; i < base.size(); ++i)
    {
        const auto v = static_cast<uint32_t>(std::round(brightness * static_cast<double>(base[i])));
        c[i] = static_cast<uint8_t>(std::clamp(v, 0u, 255u));
    }
    m_frames.AddFrame(Zone::ALL, c, ms);
}

void FireFlickerAnimation::AddFrame(const Frame& frame)
{
    m_frames.AddFrame(frame);
}

std::optional<Frame> FireFlickerAnimation::GetFrame(uint32_t idx)
{
    return m_frames.GetFrame(idx);
}

uint32_t FireFlickerAnimation::Size() const
{
    return m_frames.Size();
}
