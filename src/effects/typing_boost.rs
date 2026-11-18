use super::{hue_to_rgb, Effect, Frame};
use crate::sensors::keyboard;

pub struct TypingBoostEffect {
    hue: f32,
    smoothed_speed_boost: f32,
    smoothed_intensity: f32,
}

impl TypingBoostEffect {
    pub fn new() -> Self {
        keyboard::start();
        Self {
            hue: 0.0,
            smoothed_speed_boost: 0.0,
            smoothed_intensity: 0.0,
        }
    }
}

impl Effect for TypingBoostEffect {
    fn name(&self) -> &'static str {
        "Colour Transform Boost"
    }

    fn description(&self) -> &'static str {
        "Typing speed reactive: cycles colors faster the quicker you type!"
    }

    fn next_frame(&mut self) -> Option<Frame> {
        const FRAME_DURATION_MS: u64 = 30;
        const BASE_HUE_SPEED: f32 = 0.0038;
        const IMPULSE_HUE_NUDGE: f32 = 0.016;

        let target_speed_boost = keyboard::get_typing_speed_boost();
        let target_intensity = keyboard::get_typing_intensity();

        // Smooth speed boost
        if target_speed_boost > self.smoothed_speed_boost {
            self.smoothed_speed_boost += (target_speed_boost - self.smoothed_speed_boost) * 0.35;
        } else {
            self.smoothed_speed_boost += (target_speed_boost - self.smoothed_speed_boost) * 0.065;
        }

        // Smooth intensity
        if target_intensity > self.smoothed_intensity {
            self.smoothed_intensity += (target_intensity - self.smoothed_intensity) * 0.40;
        } else {
            self.smoothed_intensity += (target_intensity - self.smoothed_intensity) * 0.08;
        }
        self.smoothed_intensity = self.smoothed_intensity.clamp(0.0, 1.0);

        let impulse = keyboard::consume_keystroke_impulse();

        let speed_multiplier = 1.0 + self.smoothed_speed_boost;
        let hue_advance = (BASE_HUE_SPEED * speed_multiplier) + (impulse * IMPULSE_HUE_NUDGE);

        self.hue = (self.hue + hue_advance).rem_euclid(1.0);

        let brightness = 0.45 + self.smoothed_intensity * 0.55;
        let colour = hue_to_rgb(self.hue, brightness);

        Some(Frame::new(colour, FRAME_DURATION_MS))
    }
}
