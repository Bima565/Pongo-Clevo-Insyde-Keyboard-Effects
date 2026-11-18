use super::{Effect, Frame};
use crate::hardware::Colour;

pub struct ColourTransformEffect {
    frames: Vec<Frame>,
    current_idx: usize,
}

impl ColourTransformEffect {
    pub fn new() -> Self {
        const STEPS: usize = 40;
        const DURATION_MS: u64 = 40;

        let red = Colour::new(255, 0, 0);
        let green = Colour::new(0, 255, 0);
        let blue = Colour::new(0, 0, 255);

        let mut frames = Vec::with_capacity(STEPS * 3);

        let transitions = [(red, green), (green, blue), (blue, red)];

        for (start, end) in transitions {
            for i in 0..STEPS {
                let t = i as f32 / STEPS as f32;
                let r = (start.r as f32 + (end.r as f32 - start.r as f32) * t)
                    .clamp(0.0, 255.0)
                    .round() as u8;
                let g = (start.g as f32 + (end.g as f32 - start.g as f32) * t)
                    .clamp(0.0, 255.0)
                    .round() as u8;
                let b = (start.b as f32 + (end.b as f32 - start.b as f32) * t)
                    .clamp(0.0, 255.0)
                    .round() as u8;

                frames.push(Frame::new(Colour::new(r, g, b), DURATION_MS));
            }
        }

        Self {
            frames,
            current_idx: 0,
        }
    }
}

impl Effect for ColourTransformEffect {
    fn name(&self) -> &'static str {
        "Colour Transform"
    }

    fn description(&self) -> &'static str {
        "Smooth and continuous RGB color wave."
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
