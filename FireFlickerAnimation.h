// Single-zone friendly fire flicker effect: warm orange base with brightness jitter
// and occasional white spark. Uses only Zone::ALL so it works on single-zone keyboards.
#pragma once

#include "IAnimation.h"
#include "FrameCollection.h"
#include "Colour.h"

class FireFlickerAnimation : public IAnimation
{
public:
    FireFlickerAnimation();
    ~FireFlickerAnimation() override = default;

    std::optional<Frame> GetFrame(uint32_t idx) override;
    bool IsSupportedKB(KeyboardType kbType) const override { return kbType != KeyboardType::NONE; }
    uint32_t Size() const override;

private:
    void AddFrame(const Frame& frame) override;
    void AddFlickerFrame(const Colour& base, double brightness, uint32_t ms);

    FrameCollection m_frames{};
};
