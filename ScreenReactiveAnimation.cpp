#include "stdafx.h"
#include <vector>
#include <numeric>
#include "ScreenReactiveAnimation.h"

ScreenReactiveAnimation::ScreenReactiveAnimation(uint32_t frameTimeMs)
	: m_frameTimeMs(frameTimeMs)
{
}

std::optional<Frame> ScreenReactiveAnimation::GetFrame(uint32_t idx)
{
	UNREFERENCED_PARAMETER(idx);
	return MakeFrame();
}

Frame ScreenReactiveAnimation::MakeFrame() const
{
	const auto colour = SampleScreenAverage();
	return Frame(Zone::ALL, colour, m_frameTimeMs);
}

Colour ScreenReactiveAnimation::SampleScreenAverage() const
{
	Colour result{ 0, 0, 0 };

	HDC hScreen = GetDC(nullptr);
	if (!hScreen)
	{
		return result;
	}

	// Sample a 3x3 grid around screen center. GetPixel is relatively expensive,
	// so keeping the sample count small matters for always-on background use.
	const int screenW = GetSystemMetrics(SM_CXSCREEN);
	const int screenH = GetSystemMetrics(SM_CYSCREEN);
	if (screenW <= 0 || screenH <= 0)
	{
		ReleaseDC(nullptr, hScreen);
		return result;
	}

	const int cx = screenW / 2;
	const int cy = screenH / 2;
	const int stepX = std::max(screenW / 20, 10); // spread samples but avoid tiny step
	const int stepY = std::max(screenH / 20, 10);

	uint64_t sumR = 0, sumG = 0, sumB = 0;
	uint64_t sumRWhite = 0, sumGWhite = 0, sumBWhite = 0;
	int countWhite = 0;
	int count = 0;

	for (int dy = -1; dy <= 1; ++dy)
	{
		for (int dx = -1; dx <= 1; ++dx)
		{
			int x = cx + dx * stepX;
			int y = cy + dy * stepY;
			x = std::clamp(x, 0, screenW - 1);
			y = std::clamp(y, 0, screenH - 1);
			const COLORREF px = GetPixel(hScreen, x, y);
			if (px == CLR_INVALID)
			{
				continue; // skip failed samples (e.g., full-screen exclusive surface)
			}
			const uint8_t r = GetRValue(px);
			const uint8_t g = GetGValue(px);
			const uint8_t b = GetBValue(px);

			const uint8_t maxc = std::max({ r, g, b });
			const uint8_t minc = std::min({ r, g, b });
			const bool isWhiteish = (minc > 230) && (maxc - minc <= 15);

			if (isWhiteish)
			{
				sumRWhite += r; sumGWhite += g; sumBWhite += b;
				++countWhite;
			}
			else
			{
				sumR += r; sumG += g; sumB += b;
				++count;
			}
		}
	}

	ReleaseDC(nullptr, hScreen);

	constexpr double kWhiteWeight = 0.5;
	constexpr double kColorWeight = 0.95;

	const double weightedCount =
		kColorWeight * static_cast<double>(count) +
		kWhiteWeight * static_cast<double>(countWhite);

	if (weightedCount > 0.0)
	{
		const double r = (kColorWeight * static_cast<double>(sumR) +
			kWhiteWeight * static_cast<double>(sumRWhite)) / weightedCount;
		const double g = (kColorWeight * static_cast<double>(sumG) +
			kWhiteWeight * static_cast<double>(sumGWhite)) / weightedCount;
		const double b = (kColorWeight * static_cast<double>(sumB) +
			kWhiteWeight * static_cast<double>(sumBWhite)) / weightedCount;

		const auto clampToByte = [](long v) -> uint8_t {
			if (v < 0) return 0;
			if (v > 255) return 255;
			return static_cast<uint8_t>(v);
		};

		result[INDEX_COLOUR_RED] = clampToByte(std::lround(r));
		result[INDEX_COLOUR_GREEN] = clampToByte(std::lround(g));
		result[INDEX_COLOUR_BLUE] = clampToByte(std::lround(b));
	}

	return result;
}
