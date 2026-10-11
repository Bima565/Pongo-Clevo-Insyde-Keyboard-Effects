// Colour transform with dynamic brightness scaling and typing speed reactive acceleration.
#pragma once

#include "IAnimation.h"
#include "Colour.h"
#include "KeyboardType.h"

class ColourTransformBoostAnimation : public IAnimation
{
public:
	ColourTransformBoostAnimation();
	~ColourTransformBoostAnimation() override = default;

	std::optional<Frame> GetFrame(uint32_t idx) override;
	bool IsSupportedKB(KeyboardType kbType) const override { return kbType != KeyboardType::NONE; }
	uint32_t Size() const override;

private:
	void AddFrame(const Frame& frame) override;
	Colour HueToRgb(float hue, float brightness) const;

	float m_hue{ 0.0f };
	float m_smoothedSpeedBoost{ 0.0f };
	float m_smoothedIntensity{ 0.0f };
};
