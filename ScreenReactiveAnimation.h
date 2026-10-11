// ScreenReactiveAnimation: samples the current screen colour and applies it to the whole keyboard.
// Single-zone friendly (uses Zone::ALL). Sampling happens each GetFrame call.
#pragma once

#include "IAnimation.h"
#include "Frame.h"
#include "KeyboardType.h"

class ScreenReactiveAnimation : public IAnimation
{
public:
	ScreenReactiveAnimation(uint32_t frameTimeMs = 100);
	~ScreenReactiveAnimation() override = default;

	std::optional<Frame> GetFrame(uint32_t idx) override;
	bool IsSupportedKB(KeyboardType kbType) const override { return kbType != KeyboardType::NONE; }
	uint32_t Size() const override { return 1; } // single dynamic frame, looped externally

private:
	void AddFrame(const Frame&) override {} // unused
	Frame MakeFrame() const;
	Colour SampleScreenAverage() const;

	uint32_t m_frameTimeMs;
};
