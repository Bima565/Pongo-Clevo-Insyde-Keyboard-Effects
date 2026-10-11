#include "stdafx.h"
#include "RgbZoneTestAnimation.h"
#include "ColourFactory.h"

RgbZoneTestAnimation::RgbZoneTestAnimation()
{
    ColourFactory factory{};
    const Colour red   = factory.Create(255, 0,   0);
    const Colour green = factory.Create(0,   255, 0);
    const Colour blue  = factory.Create(0,   0,   255);

    const uint32_t holdMs = 700;
    const uint32_t gapMs  = 150;

    // Single-zone friendly: drive ALL zone only, cycling colours.
    AddState(red,   holdMs, gapMs);
    AddState(green, holdMs, gapMs);
    AddState(blue,  holdMs, gapMs);
}

void RgbZoneTestAnimation::AddState(const Colour& colour, uint32_t holdMs, uint32_t gapMs)
{
    m_frames.AddFrame(Zone::ALL, colour, holdMs);
    m_frames.AddFrame(Zone::ALL, Colour{ 0, 0, 0 }, gapMs);
}

void RgbZoneTestAnimation::AddFrame(const Frame& frame) { m_frames.AddFrame(frame); }

std::optional<Frame> RgbZoneTestAnimation::GetFrame(uint32_t idx)
{
    return m_frames.GetFrame(idx);
}

uint32_t RgbZoneTestAnimation::Size() const
{
    return m_frames.Size();
}
