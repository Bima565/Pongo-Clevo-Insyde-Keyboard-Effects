use super::{Effect, Frame};
use crate::hardware::Colour;

pub struct StrobeEffect {
    frames: Vec<Frame>,
    current_idx: usize,
}

impl StrobeEffect {
    pub fn new() -> Self {
        const FADE_IN_STEPS: usize = 5;
        const FADE_IN_MS: u64 = 20;
        const PEAK_MS: u64 = 35;
        const FADE_OUT_STEPS: usize = 6;
        const FADE_OUT_MS: u64 = 20;
        const OFF_MS: u64 = 50;
        const PULSES_PER_SEGMENT: usize = 6;

        let palette = [
            Colour::new(255, 20, 30),   // Vivid Red
            Colour::new(255, 110, 0),   // Warm Orange
            Colour::new(255, 210, 0),   // Bright Gold
            Colour::new(0, 240, 70),    // Neon Green
            Colour::new(0, 210, 255),   // Aqua / Cyan
            Colour::new(25, 90, 255),   // Royal Blue
            Colour::new(150, 30, 255),  // Neon Violet
            Colour::new(255, 20, 160),  // Magenta
        ];

        let mut frames = Vec::new();

        for i in 0..palette.len() {
            let start = palette[i];
            let end = palette[(i + 1) % palette.len()];

            for p in 0..PULSES_PER_SEGMENT {
                let progress = p as f32 / PULSES_PER_SEGMENT as f32;
                let current = Colour::new(
                    (start.r as f32 + (end.r as f32 - start.r as f32) * progress) as u8,
                    (start.g as f32 + (end.g as f32 - start.g as f32) * progress) as u8,
                    (start.b as f32 + (end.b as f32 - start.b as f32) * progress) as u8,
                );

                // Fade-in
                for s in 0..FADE_IN_STEPS {
                    let factor = (s + 1) as f32 / FADE_IN_STEPS as f32;
                    let c = Colour::new(
                        ((current.r as f32) * factor) as u8,
                        ((current.g as f32) * factor) as u8,
                        ((current.b as f32) * factor) as u8,
                    );
                    frames.push(Frame::new(c, FADE_IN_MS));
                }

                // Peak
                frames.push(Frame::new(current, PEAK_MS));

                // Fade-out
                for s in 0..FADE_OUT_STEPS {
                    let factor = 1.0 - (s + 1) as f32 / FADE_OUT_STEPS as f32;
                    let c = Colour::new(
                        ((current.r as f32) * factor) as u8,
                        ((current.g as f32) * factor) as u8,
                        ((current.b as f32) * factor) as u8,
                    );
                    frames.push(Frame::new(c, FADE_OUT_MS));
                }

                // Off gap
                frames.push(Frame::new(Colour::BLACK, OFF_MS));
            }
        }

        Self {
            frames,
            current_idx: 0,
        }
    }
}

impl Effect for StrobeEffect {
    fn name(&self) -> &'static str {
        "Strobe Transform"
    }

    fn description(&self) -> &'static str {
        "Dynamic strobe with smooth fade-in & fade-out."
    }

    fn next_frame(&mut self) -> Option<Frame> {
        if self.frames.is_empty() {
            return None;
        }
        let frame = self.frames[self.current_idx];
        self.current_idx = (self.current_idx + 1) % self.frames.len();
        Some(frame)
    }
}
