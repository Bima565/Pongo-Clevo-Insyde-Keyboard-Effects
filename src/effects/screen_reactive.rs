use super::{Effect, Frame};
use crate::sensors::screen::ScreenSensor;

pub struct ScreenReactiveEffect {
    frame_duration_ms: u64,
}

impl ScreenReactiveEffect {
    pub fn new() -> Self {
        Self {
            frame_duration_ms: 60,
        }
    }
}

impl Effect for ScreenReactiveEffect {
    fn name(&self) -> &'static str {
        "Screen Reactive"
    }

    fn description(&self) -> &'static str {
        "Ambilight ambient sync matching on-screen colors."
    }

    fn next_frame(&mut self) -> Option<Frame> {
        let colour = ScreenSensor::sample_average();
        Some(Frame::new(colour, self.frame_duration_ms))
    }
}
