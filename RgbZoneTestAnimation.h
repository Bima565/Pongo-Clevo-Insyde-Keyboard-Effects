// Simple diagnostic animation: cycles Red/Green/Blue. For single-zone boards it lights ALL;
// for multi-zone boards we keep the same ALL-based frames so it still shows clearly.
#pragma once

#include "IAnimation.h"
#include "FrameCollection.h"
#include "Colour.h"

class RgbZoneTestAnimation : public IAnimation
{
public:
    RgbZoneTestAnimation();
    ~RgbZoneTestAnimation() override = default;

    std::optional<Frame> GetFrame(uint32_t idx) override;
    bool IsSupportedKB(KeyboardType kbType) const override { return kbType != KeyboardType::NONE; }
    uint32_t Size() const override;

private:
	void AddFrame(const Frame& frame) override;

	void AddState(const Colour& colour, uint32_t holdMs, uint32_t gapMs);

	FrameCollection m_frames{};
};
