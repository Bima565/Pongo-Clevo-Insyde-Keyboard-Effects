use super::{Effect, Frame};
use crate::hardware::Colour;
use std::f32::consts::PI;

pub struct BreatheEffect {
    frames: Vec<Frame>,
    current_idx: usize,
}

impl BreatheEffect {
    pub fn new() -> Self {
        const STEPS: usize = 40;
        const DURATION_MS: u64 = 40;
        let mut frames = Vec::with_capacity(STEPS * 3);

        // Phase 1: Red
        for i in 0..STEPS {
            let val = (180.0 / STEPS as f32) * (i as f32);
            let intensity = (255.0 * (val * (PI / 180.0)).sin()).clamp(0.0, 255.0) as u8;
            frames.push(Frame::new(Colour::new(intensity, 0, 0), DURATION_MS));
        }

        // Phase 2: Green
        for i in 0..STEPS {
            let val = (180.0 / STEPS as f32) * (i as f32);
            let intensity = (255.0 * (val * (PI / 180.0)).sin()).clamp(0.0, 255.0) as u8;
            frames.push(Frame::new(Colour::new(0, intensity, 0), DURATION_MS));
        }

        // Phase 3: Blue
        for i in 0..STEPS {
            let val = (180.0 / STEPS as f32) * (i as f32);
            let intensity = (255.0 * (val * (PI / 180.0)).sin()).clamp(0.0, 255.0) as u8;
            frames.push(Frame::new(Colour::new(0, 0, intensity), DURATION_MS));
        }

        Self {
            frames,
            current_idx: 0,
        }
    }
}

impl Effect for BreatheEffect {
    fn name(&self) -> &'static str {
        "Breathe"
    }

    fn description(&self) -> &'static str {
        "Classic breathing pulse through primary RGB colors."
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
