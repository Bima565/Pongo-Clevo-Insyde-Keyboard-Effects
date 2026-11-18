pub mod breathe;
pub mod colour_transform;
pub mod music_reactive;
pub mod new_breathe;
pub mod rgb_zones;
pub mod screen_reactive;
pub mod strobe;
pub mod typing_boost;

use crate::hardware::Colour;
use std::time::Duration;

#[derive(Debug, Clone, Copy)]
pub struct Frame {
    pub colour: Colour,
    pub duration: Duration,
}

impl Frame {
    pub fn new(colour: Colour, duration_ms: u64) -> Self {
        Self {
            colour,
            duration: Duration::from_millis(duration_ms),
        }
    }
}

pub trait Effect: Send {
    fn name(&self) -> &'static str;
    fn description(&self) -> &'static str;
    fn next_frame(&mut self) -> Option<Frame>;
}

pub fn create_effect(theme: &str) -> Option<Box<dyn Effect>> {
    match theme.to_lowercase().as_str() {
        "breathe" => Some(Box::new(breathe::BreatheEffect::new())),
        "newcolourbreathe" | "newbreathe" => Some(Box::new(new_breathe::NewBreatheEffect::new())),
        "colourtransform" | "rainbow" => {
            Some(Box::new(colour_transform::ColourTransformEffect::new()))
        }
        "strobe" => Some(Box::new(strobe::StrobeEffect::new())),
        "musicadaptive" | "music" | "audio" => {
            Some(Box::new(music_reactive::MusicReactiveEffect::new()))
        }
        "rgbzones" | "rgbtest" | "diagnostic" => {
            Some(Box::new(rgb_zones::RgbZonesEffect::new()))
        }
        "fireflicker" | "colourboost" | "boost" | "typing" => {
            Some(Box::new(typing_boost::TypingBoostEffect::new()))
        }
        "screenreactive" | "ambilight" | "screen" => {
            Some(Box::new(screen_reactive::ScreenReactiveEffect::new()))
        }
        _ => None,
    }
}

pub struct EffectInfo {
    pub id: &'static str,
    pub label: &'static str,
    pub description: &'static str,
}

pub const ALL_EFFECTS: &[EffectInfo] = &[
    EffectInfo {
        id: "breathe",
        label: "1. Breathe",
        description: "Classic breathing pulse through primary RGB colors.",
    },
    EffectInfo {
        id: "newcolourbreathe",
        label: "2. New Colour Breathe",
        description: "Dynamic multi-color breathing cycle.",
    },
    EffectInfo {
        id: "colourtransform",
        label: "3. Colour Transform",
        description: "Smooth and continuous RGB color wave.",
    },
    EffectInfo {
        id: "strobe",
        label: "4. Strobe Transform",
        description: "Dynamic strobe with smooth fade-in & fade-out.",
    },
    EffectInfo {
        id: "musicadaptive",
        label: "5. Music Adaptive Transform",
        description: "Real-time audio visualizer reacting to PC music.",
    },
    EffectInfo {
        id: "rgbzones",
        label: "6. RGB Zone Test (R/G/B)",
        description: "Diagnostic test cycling hardware color zones.",
    },
    EffectInfo {
        id: "fireflicker",
        label: "7. Colour Transform Boost",
        description: "Typing speed reactive: cycles colors faster the quicker you type!",
    },
    EffectInfo {
        id: "screenreactive",
        label: "8. Screen Reactive",
        description: "Ambilight ambient sync matching on-screen colors.",
    },
];

pub fn hue_to_rgb(hue: f32, brightness: f32) -> Colour {
    let h = (hue.rem_euclid(1.0)) * 6.0;
    let sector = h.floor() as i32;
    let fractional = h - sector as f32;

    let p = 0.0f32;
    let q = brightness * (1.0 - fractional);
    let t = brightness * fractional;

    let (r, g, b) = match sector {
        0 => (brightness, t, p),
        1 => (q, brightness, p),
        2 => (p, brightness, t),
        3 => (p, q, brightness),
        4 => (t, p, brightness),
        _ => (brightness, p, q),
    };

    Colour::new(
        (r * 255.0).clamp(0.0, 255.0).round() as u8,
        (g * 255.0).clamp(0.0, 255.0).round() as u8,
        (b * 255.0).clamp(0.0, 255.0).round() as u8,
    )
}
