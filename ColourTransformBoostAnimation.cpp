#include "stdafx.h"
#include <algorithm>
#include <cmath>
#include "ColourTransformBoostAnimation.h"
#include "KeypressBoost.h"

namespace
{
	constexpr uint32_t kFrameDurationMs = 30;    // ~33 FPS for ultra-smooth responsiveness
	constexpr float kBaseHueSpeed = 0.0038f;     // Base idle hue speed (~7.8s for full rainbow cycle)
	constexpr float kImpulseHueNudge = 0.016f;   // Direct hue step on each keypress impulse
}

ColourTransformBoostAnimation::ColourTransformBoostAnimation()
	: m_hue(0.0f)
	, m_smoothedSpeedBoost(0.0f)
	, m_smoothedIntensity(0.0f)
{
}

void ColourTransformBoostAnimation::AddFrame(const Frame& /*frame*/)
{
	// Dynamic frame generation; no pre-baked static frames needed.
}

std::optional<Frame> ColourTransformBoostAnimation::GetFrame(uint32_t /*idx*/)
{
	const float targetSpeedBoost = KeypressBoost::GetTypingSpeedBoost();
	const float targetIntensity = KeypressBoost::GetTypingIntensity();

	// Smoothly interpolate typing speed boost:
	// Snappy acceleration on keystrokes (~2-3 frames), graceful flywheel inertia deceleration on pause
	if (targetSpeedBoost > m_smoothedSpeedBoost)
	{
		m_smoothedSpeedBoost += (targetSpeedBoost - m_smoothedSpeedBoost) * 0.35f;
	}
	else
	{
		m_smoothedSpeedBoost += (targetSpeedBoost - m_smoothedSpeedBoost) * 0.065f;
	}

	if (targetIntensity > m_smoothedIntensity)
	{
		m_smoothedIntensity += (targetIntensity - m_smoothedIntensity) * 0.40f;
	}
	else
	{
		m_smoothedIntensity += (targetIntensity - m_smoothedIntensity) * 0.08f;
	}
	m_smoothedIntensity = std::clamp(m_smoothedIntensity, 0.0f, 1.0f);

	// Keystroke impulse: provides immediate tactile chromatic push on each distinct key hit
	const float impulse = KeypressBoost::ConsumeKeystrokeImpulse();

	// Dynamic hue advance: Base speed + non-linear typing speed acceleration + instantaneous key tap impulse
	const float speedMultiplier = 1.0f + m_smoothedSpeedBoost;
	const float hueAdvance = (kBaseHueSpeed * speedMultiplier) + (impulse * kImpulseHueNudge);

	m_hue += hueAdvance;
	if (m_hue >= 1.0f)
	{
		m_hue -= std::floor(m_hue);
	}
	else if (m_hue < 0.0f)
	{
		m_hue = 0.0f;
	}

	// Dynamic brightness factor (100% stable while held, smooth cosine fade after release)
	const float brightness = static_cast<float>(KeypressBoost::GetFactor());

	// Convert Hue + Brightness into RGB Colour
	const Colour colour = HueToRgb(m_hue, brightness);

	return Frame(Zone::ALL, colour, kFrameDurationMs);
}

uint32_t ColourTransformBoostAnimation::Size() const
{
	// Single dynamic frame; Keyboard::PlayAnimation loops continuously, querying new frames in real-time
	return 1;
}

Colour ColourTransformBoostAnimation::HueToRgb(float hue, float brightness) const
{
	float h = std::fmod(hue, 1.0f);
	if (h < 0.0f)
	{
		h += 1.0f;
	}
	h *= 6.0f;

	const int sector = static_cast<int>(h);
	const float fractional = h - static_cast<float>(sector);

	const float p = 0.0f;
	const float q = brightness * (1.0f - fractional);
	const float t = brightness * fractional;

	float r = 0.0f, g = 0.0f, b = 0.0f;

	switch (sector % 6)
	{
	case 0: r = brightness; g = t; b = p; break;
	case 1: r = q; g = brightness; b = p; break;
	case 2: r = p; g = brightness; b = t; break;
	case 3: r = p; g = q; b = brightness; break;
	case 4: r = t; g = p; b = brightness; break;
	default: r = brightness; g = p; b = q; break;
	}

	Colour colour{};
	colour[INDEX_COLOUR_RED]   = static_cast<uint8_t>(std::clamp(r * 255.0f, 0.0f, 255.0f));
	colour[INDEX_COLOUR_GREEN] = static_cast<uint8_t>(std::clamp(g * 255.0f, 0.0f, 255.0f));
	colour[INDEX_COLOUR_BLUE]  = static_cast<uint8_t>(std::clamp(b * 255.0f, 0.0f, 255.0f));

	return colour;
}
