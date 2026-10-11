// Keyboard keypress listener providing brightness boost, key-hold detection, and typing speed reactivity.
#pragma once

#include "stdafx.h"

namespace KeypressBoost
{
	// Start a background thread that installs a low-level keyboard hook.
	// boostHoldMs: grace duration in ms after key release to hold peak boost.
	// baseFactor: brightness multiplier when idle.
	// boostFactor: brightness multiplier during typing / key hold.
	void Start(uint32_t boostHoldMs = 300, double baseFactor = 0.25, double boostFactor = 1.0);

	// Stop the hook thread if needed.
	void Stop();

	// Get current brightness factor based on key hold state, recent keystrokes, and smooth decay.
	double GetFactor();

	// Returns true if any keyboard key is currently physically pressed / held down.
	bool IsKeyHeld();

	// Get typing intensity in range [0.0 .. 1.0] representing keystroke tempo / typing speed.
	float GetTypingIntensity();

	// Get estimated typing speed in Keys Per Second (KPS).
	float GetTypingKps();

	// Get dynamic speed boost factor for animations (e.g. 0.0f at idle, scaling up smoothly with typing speed).
	float GetTypingSpeedBoost();

	// Consume accumulated keystroke impulse (resets back to 0).
	float ConsumeKeystrokeImpulse();
}
