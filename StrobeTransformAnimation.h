#pragma once

#include "IAnimation.h"
#include "FrameCollection.h"
#include "Colour.h"

class StrobeTransformAnimation : public IAnimation
{
public:
    StrobeTransformAnimation();
    ~StrobeTransformAnimation() override = default;

    std::optional<Frame> GetFrame(uint32_t idx) override;
    bool IsSupportedKB(KeyboardType kbType) const override;
    uint32_t Size() const override;

private:
    void AddFrame(const Frame& frame) override;

    FrameCollection m_frames{};

    void GenerateStrobeTransform(const Colour& startColour, const Colour& endColour, uint32_t pulseCount);
    void AddPulse(const Colour& peakColour);
};
