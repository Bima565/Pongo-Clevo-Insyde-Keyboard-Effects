use super::{Effect, Frame};
use crate::hardware::Colour;
use std::f32::consts::PI;

pub struct NewBreatheEffect {
    frames: Vec<Frame>,
    current_idx: usize,
}

impl NewBreatheEffect {
    pub fn new() -> Self {
        const STEPS: usize = 40;
        const DURATION_MS: u64 = 40;

        let palette = [
            Colour::new(255, 255, 0),   // Yellow
            Colour::new(255, 0, 0),     // Red
            Colour::new(255, 0, 255),   // Purple
            Colour::new(0, 255, 0),     // Green
            Colour::new(255, 255, 255), // White
            Colour::new(0, 0, 255),     // Blue
            Colour::new(255, 120, 120), // Brown / Coral
        ];

        let mut frames = Vec::with_capacity(STEPS * palette.len());

        for target in palette {
            for i in 0..STEPS {
                let val = (180.0 / STEPS as f32) * (i as f32);
                let factor = (val * (PI / 180.0)).sin();
                let r = ((target.r as f32) * factor).clamp(0.0, 255.0) as u8;
                let g = ((target.g as f32) * factor).clamp(0.0, 255.0) as u8;
                let b = ((target.b as f32) * factor).clamp(0.0, 255.0) as u8;

                frames.push(Frame::new(Colour::new(r, g, b), DURATION_MS));
            }
        }

        Self {
            frames,
            current_idx: 0,
        }
    }
}

impl Effect for NewBreatheEffect {
    fn name(&self) -> &'static str {
        "New Colour Breathe"
    }

    fn description(&self) -> &'static str {
        "Dynamic multi-color breathing cycle."
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
