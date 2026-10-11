#include "stdafx.h"
#include <cmath>
#include <algorithm>
#include <vector>
#include "ColourFactory.h"
#include "MathConstants.h"
#include "StrobeTransformAnimation.h"

namespace
{
    // Timing configuration for each strobe pulse (milliseconds and step counts)
    constexpr uint32_t kFadeInSteps   = 5;   // Micro-steps during fade-in
    constexpr uint32_t kFadeInStepMs  = 20;  // 5 * 20ms = 100ms fade-in
    constexpr uint32_t kPeakTimeMs    = 35;  // 35ms peak hold
    constexpr uint32_t kFadeOutSteps  = 6;   // Micro-steps during fade-out
    constexpr uint32_t kFadeOutStepMs = 20;  // 6 * 20ms = 120ms fade-out
    constexpr uint32_t kOffTimeMs     = 50;  // 50ms pause/gap between pulses

    constexpr uint32_t kPulsesPerSegment = 6; // Number of pulses when transforming between each color
}

StrobeTransformAnimation::StrobeTransformAnimation()
{
    ColourFactory factory{};

    // Vibrant spectrum palette anchors
    const std::vector<Colour> palette =
    {
        factory.Create(255, 20, 30),   // Vivid Red
        factory.Create(255, 110, 0),   // Warm Orange
        factory.Create(255, 210, 0),   // Bright Gold / Yellow
        factory.Create(0, 240, 70),    // Neon Emerald Green
        factory.Create(0, 210, 255),   // Aqua / Cyan
        factory.Create(25, 90, 255),   // Royal Electric Blue
        factory.Create(150, 30, 255),  // Neon Violet / Purple
        factory.Create(255, 20, 160)   // Magenta / Hot Pink
    };

    for (size_t i = 0; i + 1 < palette.size(); ++i)
    {
        GenerateStrobeTransform(palette[i], palette[i + 1], kPulsesPerSegment);
    }

    // Close the loop back to the first color for seamless continuous playback
    GenerateStrobeTransform(palette.back(), palette.front(), kPulsesPerSegment);
}

void StrobeTransformAnimation::AddFrame(const Frame& frame)
{
    m_frames.AddFrame(frame);
}

std::optional<Frame> StrobeTransformAnimation::GetFrame(uint32_t idx)
{
    return m_frames.GetFrame(idx);
}

bool StrobeTransformAnimation::IsSupportedKB(KeyboardType kbType) const
{
    return kbType != KeyboardType::NONE;
}

uint32_t StrobeTransformAnimation::Size() const
{
    return m_frames.Size();
}

void StrobeTransformAnimation::AddPulse(const Colour& peakColour)
{
    const auto applyBrightness = [](const Colour& base, double factor) -> Colour {
        Colour c{};
        for (size_t ch = 0; ch < base.size(); ++ch)
        {
            const auto val = static_cast<int>(std::round(static_cast<double>(base[ch]) * factor));
            c[ch] = static_cast<uint8_t>(std::clamp(val, 0, 255));
        }
        return c;
    };

    // 1. Smooth Fade-In (Attack) using sine-squared curve
    for (uint32_t step = 1; step <= kFadeInSteps; ++step)
    {
        const double progress = static_cast<double>(step) / static_cast<double>(kFadeInSteps);
        const double factor = std::sin(progress * (static_cast<double>(MATH_PI) / 2.0));
        const double brightness = factor * factor; // smooth ease-in

        m_frames.AddFrame(Zone::ALL, applyBrightness(peakColour, brightness), kFadeInStepMs);
    }

    // 2. Peak Hold (Full Brightness)
    m_frames.AddFrame(Zone::ALL, peakColour, kPeakTimeMs);

    // 3. Smooth Fade-Out (Decay) using cosine-squared decay
    for (uint32_t step = 1; step <= kFadeOutSteps; ++step)
    {
        const double progress = static_cast<double>(step) / static_cast<double>(kFadeOutSteps);
        const double factor = std::cos(progress * (static_cast<double>(MATH_PI) / 2.0));
        const double brightness = factor * factor; // smooth decay down to ~0

        m_frames.AddFrame(Zone::ALL, applyBrightness(peakColour, brightness), kFadeOutStepMs);
    }

    // 4. Brief Rest/Off Gap (crisp strobe separation)
    if (kOffTimeMs > 0)
    {
        m_frames.AddFrame(Zone::ALL, Colour{ 0, 0, 0 }, kOffTimeMs);
    }
}

void StrobeTransformAnimation::GenerateStrobeTransform(const Colour& startColour, const Colour& endColour, uint32_t pulseCount)
{
    if (pulseCount == 0) return;

    for (uint32_t p = 0; p < pulseCount; ++p)
    {
        const double t = static_cast<double>(p) / static_cast<double>(pulseCount);
        Colour pulseColour{};

        for (size_t ch = 0; ch < startColour.size(); ++ch)
        {
            const double val = static_cast<double>(startColour[ch]) +
                t * (static_cast<double>(endColour[ch]) - static_cast<double>(startColour[ch]));
            pulseColour[ch] = static_cast<uint8_t>(std::clamp(std::round(val), 0.0, 255.0));
        }

        AddPulse(pulseColour);
    }
}
