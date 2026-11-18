use super::{hue_to_rgb, Effect, Frame};
use crate::sensors::audio::AudioSensor;

pub struct MusicReactiveEffect {
    sensor: AudioSensor,
    hue: f32,
}

impl MusicReactiveEffect {
    pub fn new() -> Self {
        Self {
            sensor: AudioSensor::new(),
            hue: 0.0,
        }
    }
}

impl Effect for MusicReactiveEffect {
    fn name(&self) -> &'static str {
        "Music Adaptive Transform"
    }

    fn description(&self) -> &'static str {
        "Real-time audio visualizer reacting to PC music."
    }

    fn next_frame(&mut self) -> Option<Frame> {
        const FRAME_DURATION_MS: u64 = 75;
        const MIN_BRIGHTNESS: f32 = 0.15;
        const MAX_BRIGHTNESS: f32 = 1.0;
        const BASE_HUE_DRIFT: f32 = 0.0125;

        let level = self.sensor.get_peak_level();

        self.hue = (self.hue + BASE_HUE_DRIFT + (level * 0.08)).rem_euclid(1.0);

        let brightness = (MIN_BRIGHTNESS + level * (MAX_BRIGHTNESS - MIN_BRIGHTNESS))
            .clamp(MIN_BRIGHTNESS, MAX_BRIGHTNESS);

        let colour = hue_to_rgb(self.hue, brightness);
        Some(Frame::new(colour, FRAME_DURATION_MS))
    }
}
