#include "stdafx.h"
#include <atomic>
#include <thread>
#include <mutex>
#include <cmath>
#include <algorithm>
#include "KeypressBoost.h"

namespace
{
	std::once_flag g_startedFlag;
	HHOOK g_hHook = nullptr;
	DWORD g_hookThreadId = 0;

	std::atomic<uint32_t> g_boostHoldMs{ 300 };
	std::atomic<double> g_baseFactor{ 0.25 };
	std::atomic<double> g_boostFactor{ 1.0 };

	std::mutex g_stateMutex;
	bool g_keyState[256]{ false };
	int g_pressedKeyCount{ 0 };
	ULONGLONG g_lastKeyDownTime{ 0 };
	ULONGLONG g_lastKeyUpTime{ 0 };

	// Keystroke history for typing speed / intensity calculation
	constexpr size_t kHistorySize = 64;
	ULONGLONG g_keypressHistory[kHistorySize]{ 0 };
	size_t g_historyIndex{ 0 };

	// Keystroke impulse accumulator
	std::atomic<float> g_keystrokeImpulse{ 0.0f };

	LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
	{
		if (nCode == HC_ACTION)
		{
			const auto* pKbd = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
			if (pKbd)
			{
				const DWORD vk = pKbd->vkCode & 0xFF;
				const ULONGLONG now = GetTickCount64();

				if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)
				{
					std::lock_guard<std::mutex> lock(g_stateMutex);

					const bool isRepeat = g_keyState[vk];
					if (!isRepeat)
					{
						g_keyState[vk] = true;
						g_pressedKeyCount++;

						// Record new keystroke timestamp for typing velocity
						g_keypressHistory[g_historyIndex % kHistorySize] = now;
						g_historyIndex++;

						// Add an instant snappy impulse for each distinct key hit
						const float currImpulse = g_keystrokeImpulse.load();
						g_keystrokeImpulse.store((std::min)(currImpulse + 0.35f, 1.0f));
					}
					else
					{
						// Held key repeating (e.g. holding key in games or typing)
						const float currImpulse = g_keystrokeImpulse.load();
						g_keystrokeImpulse.store((std::min)(currImpulse + 0.08f, 1.0f));
					}

					g_lastKeyDownTime = now;
				}
				else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP)
				{
					std::lock_guard<std::mutex> lock(g_stateMutex);

					if (g_keyState[vk])
					{
						g_keyState[vk] = false;
						if (g_pressedKeyCount > 0)
						{
							g_pressedKeyCount--;
						}
					}

					g_lastKeyUpTime = now;
				}
			}
		}

		return CallNextHookEx(g_hHook, nCode, wParam, lParam);
	}

	void HookThread()
	{
		g_hookThreadId = GetCurrentThreadId();
		g_hHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandleW(nullptr), 0);

		MSG msg;
		while (GetMessageW(&msg, nullptr, 0, 0) > 0)
		{
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}

		if (g_hHook)
		{
			UnhookWindowsHookEx(g_hHook);
			g_hHook = nullptr;
		}
	}
}

namespace KeypressBoost
{
	void Start(uint32_t boostHoldMs, double baseFactor, double boostFactor)
	{
		std::call_once(g_startedFlag, [boostHoldMs, baseFactor, boostFactor]() {
			g_boostHoldMs.store(boostHoldMs);
			g_baseFactor.store(baseFactor);
			g_boostFactor.store(boostFactor);
			std::thread(HookThread).detach();
		});
	}

	void Stop()
	{
		if (g_hookThreadId != 0)
		{
			PostThreadMessageW(g_hookThreadId, WM_QUIT, 0, 0);
		}
	}

	bool IsKeyHeld()
	{
		std::lock_guard<std::mutex> lock(g_stateMutex);
		return g_pressedKeyCount > 0;
	}

	double GetFactor()
	{
		const ULONGLONG now = GetTickCount64();
		const double baseF = g_baseFactor.load();
		const double boostF = g_boostFactor.load();

		int heldCount = 0;
		ULONGLONG lastDown = 0;
		ULONGLONG lastUp = 0;

		{
			std::lock_guard<std::mutex> lock(g_stateMutex);
			heldCount = g_pressedKeyCount;
			lastDown = g_lastKeyDownTime;
			lastUp = g_lastKeyUpTime;
		}

		// Safety check: if heldCount > 0 but no key event for 3.5s, verify with GetAsyncKeyState
		if (heldCount > 0 && (now - lastDown > 3500))
		{
			std::lock_guard<std::mutex> lock(g_stateMutex);
			bool anyStillDown = false;
			int verifiedCount = 0;
			for (int vk = 1; vk < 256; ++vk)
			{
				if (g_keyState[vk])
				{
					if ((GetAsyncKeyState(vk) & 0x8000) != 0)
					{
						anyStillDown = true;
						verifiedCount++;
					}
					else
					{
						g_keyState[vk] = false;
					}
				}
			}
			g_pressedKeyCount = verifiedCount;
			heldCount = verifiedCount;
			if (!anyStillDown)
			{
				lastUp = now;
			}
		}

		// 1. While any key is held down -> 100% stable full brightness (no drop during repeat delay!)
		if (heldCount > 0)
		{
			return boostF;
		}

		// 2. Check time since last key event
		const ULONGLONG lastActivity = (std::max)(lastDown, lastUp);
		if (lastActivity == 0)
		{
			return baseF;
		}

		const ULONGLONG elapsed = now - lastActivity;
		const uint32_t holdMs = g_boostHoldMs.load();
		constexpr uint32_t kFadeMs = 450; // Smooth 450ms cosine fade decay

		if (elapsed <= holdMs)
		{
			return boostF;
		}

		const ULONGLONG fadeElapsed = elapsed - holdMs;
		if (fadeElapsed < kFadeMs)
		{
			// Smooth ease-out cosine curve from 1.0 down to 0.0
			const double progress = static_cast<double>(fadeElapsed) / static_cast<double>(kFadeMs);
			const double ease = 0.5 * (1.0 + std::cos(progress * 3.14159265358979323846));
			return baseF + (boostF - baseF) * ease;
		}

		return baseF;
	}

	float GetTypingKps()
	{
		const ULONGLONG now = GetTickCount64();
		constexpr ULONGLONG kTimeWindowMs = 1500; // 1.5s sliding window

		ULONGLONG lastDown = 0;
		int recentCount = 0;
		float weightedCount = 0.0f;

		{
			std::lock_guard<std::mutex> lock(g_stateMutex);
			lastDown = g_lastKeyDownTime;
			for (size_t i = 0; i < kHistorySize; ++i)
			{
				const ULONGLONG t = g_keypressHistory[i];
				if (t > 0 && (now >= t))
				{
					const ULONGLONG age = now - t;
					if (age <= kTimeWindowMs)
					{
						recentCount++;
						// Keystrokes in the most recent interval carry full weight; smooth falloff
						const float recency = 1.0f - (static_cast<float>(age) / static_cast<float>(kTimeWindowMs)) * 0.45f;
						weightedCount += recency;
					}
				}
			}
		}

		if (recentCount == 0 || (now - lastDown > 2000))
		{
			return 0.0f;
		}

		// Convert weighted keystrokes over the time window into Keys Per Second (KPS)
		const float kps = (weightedCount * 1000.0f) / static_cast<float>(kTimeWindowMs);
		return kps;
	}

	float GetTypingIntensity()
	{
		const float kps = GetTypingKps();
		// 0 KPS -> 0.0 intensity
		// 5 KPS (~50-60 WPM) -> 0.5 intensity
		// 10 KPS (~100-120 WPM) -> 1.0 intensity
		return std::clamp(kps / 10.0f, 0.0f, 1.0f);
	}

	float GetTypingSpeedBoost()
	{
		const float kps = GetTypingKps();
		if (kps <= 0.05f)
		{
			return 0.0f;
		}

		// Smooth non-linear acceleration curve:
		// Casual typing (1-3 KPS) -> ~1.2x - 4.2x boost
		// Moderate typing (4-7 KPS) -> ~6.0x - 12.5x boost
		// Fast typing (8-11 KPS) -> ~15.0x - 23.5x boost
		// Extreme burst typing (12-16+ KPS) -> ~26.0x - 32.0x boost
		const float boost = (kps * 1.15f) + (kps * kps * 0.09f);
		return (std::min)(boost, 32.0f);
	}

	float ConsumeKeystrokeImpulse()
	{
		return g_keystrokeImpulse.exchange(0.0f);
	}
}
