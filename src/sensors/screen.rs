use crate::hardware::Colour;
use windows::Win32::Foundation::COLORREF;
use windows::Win32::Graphics::Gdi::{GetDC, GetPixel, ReleaseDC, CLR_INVALID};
use windows::Win32::UI::WindowsAndMessaging::{GetSystemMetrics, SM_CXSCREEN, SM_CYSCREEN};

pub struct ScreenSensor;

impl ScreenSensor {
    pub fn sample_average() -> Colour {
        unsafe {
            let hdc = GetDC(None);
            if hdc.is_invalid() {
                return Colour::BLACK;
            }

            let screen_w = GetSystemMetrics(SM_CXSCREEN);
            let screen_h = GetSystemMetrics(SM_CYSCREEN);
            if screen_w <= 0 || screen_h <= 0 {
                let _ = ReleaseDC(None, hdc);
                return Colour::BLACK;
            }

            let cx = screen_w / 2;
            let cy = screen_h / 2;
            let step_x = (screen_w / 20).max(10);
            let step_y = (screen_h / 20).max(10);

            let mut sum_r: u64 = 0;
            let mut sum_g: u64 = 0;
            let mut sum_b: u64 = 0;
            let mut count: usize = 0;

            let mut sum_r_white: u64 = 0;
            let mut sum_g_white: u64 = 0;
            let mut sum_b_white: u64 = 0;
            let mut count_white: usize = 0;

            for dy in -1..=1 {
                for dx in -1..=1 {
                    let x = (cx + dx * step_x).clamp(0, screen_w - 1);
                    let y = (cy + dy * step_y).clamp(0, screen_h - 1);

                    let px = GetPixel(hdc, x, y);
                    if px == COLORREF(CLR_INVALID) {
                        continue;
                    }

                    // COLORREF is 0x00bbggrr
                    let r = (px.0 & 0xFF) as u8;
                    let g = ((px.0 >> 8) & 0xFF) as u8;
                    let b = ((px.0 >> 16) & 0xFF) as u8;

                    let max_c = r.max(g).max(b);
                    let min_c = r.min(g).min(b);
                    let is_whiteish = min_c > 230 && (max_c - min_c) <= 15;

                    if is_whiteish {
                        sum_r_white += r as u64;
                        sum_g_white += g as u64;
                        sum_b_white += b as u64;
                        count_white += 1;
                    } else {
                        sum_r += r as u64;
                        sum_g += g as u64;
                        sum_b += b as u64;
                        count += 1;
                    }
                }
            }

            let _ = ReleaseDC(None, hdc);

            let white_weight = 0.5f64;
            let color_weight = 0.95f64;

            let weighted_count =
                color_weight * (count as f64) + white_weight * (count_white as f64);

            if weighted_count > 0.0 {
                let r = (color_weight * (sum_r as f64) + white_weight * (sum_r_white as f64))
                    / weighted_count;
                let g = (color_weight * (sum_g as f64) + white_weight * (sum_g_white as f64))
                    / weighted_count;
                let b = (color_weight * (sum_b as f64) + white_weight * (sum_b_white as f64))
                    / weighted_count;

                Colour::new(
                    r.round().clamp(0.0, 255.0) as u8,
                    g.round().clamp(0.0, 255.0) as u8,
                    b.round().clamp(0.0, 255.0) as u8,
                )
            } else {
                Colour::BLACK
            }
        }
    }
}
