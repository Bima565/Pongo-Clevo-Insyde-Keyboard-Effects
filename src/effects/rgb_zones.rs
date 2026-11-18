use super::{Effect, Frame};
use crate::hardware::Colour;

pub struct RgbZonesEffect {
    frames: Vec<Frame>,
    current_idx: usize,
}

impl RgbZonesEffect {
    pub fn new() -> Self {
        let hold_ms = 700;
        let gap_ms = 150;

        let frames = vec![
            Frame::new(Colour::RED, hold_ms),
            Frame::new(Colour::BLACK, gap_ms),
            Frame::new(Colour::GREEN, hold_ms),
            Frame::new(Colour::BLACK, gap_ms),
            Frame::new(Colour::BLUE, hold_ms),
            Frame::new(Colour::BLACK, gap_ms),
        ];

        Self {
            frames,
            current_idx: 0,
        }
    }
}

impl Effect for RgbZonesEffect {
    fn name(&self) -> &'static str {
        "RGB Zone Test"
    }

    fn description(&self) -> &'static str {
        "Diagnostic test cycling hardware color zones."
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
