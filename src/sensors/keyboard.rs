use std::sync::atomic::{AtomicU32, Ordering};
use std::sync::{Mutex, Once};
use std::thread;
use windows::Win32::Foundation::{LPARAM, LRESULT, WPARAM};
use windows::Win32::System::SystemInformation::GetTickCount64;
use windows::Win32::UI::WindowsAndMessaging::{
    CallNextHookEx, DispatchMessageW, GetMessageW, PostThreadMessageW, SetWindowsHookExW,
    TranslateMessage, UnhookWindowsHookEx, KBDLLHOOKSTRUCT, MSG, WH_KEYBOARD_LL,
    WM_KEYDOWN, WM_KEYUP, WM_QUIT, WM_SYSKEYDOWN, WM_SYSKEYUP,
};

const HISTORY_SIZE: usize = 64;
const WINDOW_MS: u64 = 1500;

struct KeyboardState {
    key_state: [bool; 256],
    pressed_count: i32,
    last_key_down: u64,
    last_key_up: u64,
    history: [u64; HISTORY_SIZE],
    history_idx: usize,
}

static STATE: Mutex<KeyboardState> = Mutex::new(KeyboardState {
    key_state: [false; 256],
    pressed_count: 0,
    last_key_down: 0,
    last_key_up: 0,
    history: [0; HISTORY_SIZE],
    history_idx: 0,
});

static IMPULSE_BITS: AtomicU32 = AtomicU32::new(0);
static HOOK_THREAD_ID: AtomicU32 = AtomicU32::new(0);
static START_ONCE: Once = Once::new();

unsafe extern "system" fn low_level_keyboard_proc(
    n_code: i32,
    w_param: WPARAM,
    l_param: LPARAM,
) -> LRESULT {
    if n_code >= 0 {
        let msg = w_param.0 as u32;
        let kbd = &*(l_param.0 as *const KBDLLHOOKSTRUCT);
        let vk = (kbd.vkCode & 0xFF) as usize;
        let now = GetTickCount64();

        if msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN {
            if let Ok(mut state) = STATE.lock() {
                let is_repeat = state.key_state[vk];
                if !is_repeat {
                    state.key_state[vk] = true;
                    state.pressed_count += 1;

                    let idx = state.history_idx % HISTORY_SIZE;
                    state.history[idx] = now;
                    state.history_idx = state.history_idx.wrapping_add(1);

                    // Add impulse for new key press
                    add_impulse(0.35);
                } else {
                    // Holding key repeat
                    add_impulse(0.08);
                }
                state.last_key_down = now;
            }
        } else if msg == WM_KEYUP || msg == WM_SYSKEYUP {
            if let Ok(mut state) = STATE.lock() {
                if state.key_state[vk] {
                    state.key_state[vk] = false;
                    if state.pressed_count > 0 {
                        state.pressed_count -= 1;
                    }
                }
                state.last_key_up = now;
            }
        }
    }

    CallNextHookEx(None, n_code, w_param, l_param)
}

fn add_impulse(amount: f32) {
    loop {
        let current_bits = IMPULSE_BITS.load(Ordering::Relaxed);
        let current_val = f32::from_bits(current_bits);
        let new_val = (current_val + amount).min(1.0);
        if IMPULSE_BITS
            .compare_exchange_weak(
                current_bits,
                new_val.to_bits(),
                Ordering::SeqCst,
                Ordering::Relaxed,
            )
            .is_ok()
        {
            break;
        }
    }
}

pub fn start() {
    START_ONCE.call_once(|| {
        thread::spawn(|| unsafe {
            let thread_id = windows::Win32::System::Threading::GetCurrentThreadId();
            HOOK_THREAD_ID.store(thread_id, Ordering::SeqCst);

            let hook = SetWindowsHookExW(
                WH_KEYBOARD_LL,
                Some(low_level_keyboard_proc),
                None,
                0,
            );

            if let Ok(hook) = hook {
                let mut msg = MSG::default();
                while GetMessageW(&mut msg, None, 0, 0).into() {
                    let _ = TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                }

                let _ = UnhookWindowsHookEx(hook);
            }
        });
    });
}

pub fn stop() {
    let thread_id = HOOK_THREAD_ID.load(Ordering::SeqCst);
    if thread_id != 0 {
        unsafe {
            let _ = PostThreadMessageW(thread_id, WM_QUIT, WPARAM(0), LPARAM(0));
        }
    }
}

pub fn consume_keystroke_impulse() -> f32 {
    let bits = IMPULSE_BITS.swap(0.0f32.to_bits(), Ordering::SeqCst);
    f32::from_bits(bits)
}

pub fn get_typing_kps() -> f32 {
    let now = unsafe { GetTickCount64() };
    let state = match STATE.lock() {
        Ok(s) => s,
        Err(_) => return 0.0,
    };

    if state.last_key_down == 0 || now.saturating_sub(state.last_key_down) > 2000 {
        return 0.0;
    }

    let mut weighted_count = 0.0f32;
    let mut recent_count = 0;

    for &t in &state.history {
        if t > 0 && now >= t {
            let age = now - t;
            if age <= WINDOW_MS {
                recent_count += 1;
                let recency = 1.0 - (age as f32 / WINDOW_MS as f32) * 0.45;
                weighted_count += recency;
            }
        }
    }

    if recent_count == 0 {
        return 0.0;
    }

    (weighted_count * 1000.0) / (WINDOW_MS as f32)
}

pub fn get_typing_intensity() -> f32 {
    (get_typing_kps() / 10.0).clamp(0.0, 1.0)
}

pub fn get_typing_speed_boost() -> f32 {
    let kps = get_typing_kps();
    if kps <= 0.05 {
        return 0.0;
    }
    ((kps * 1.15) + (kps * kps * 0.09)).min(32.0)
}
