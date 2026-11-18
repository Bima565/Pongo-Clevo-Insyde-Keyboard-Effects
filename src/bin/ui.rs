#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use pongo_effects::effects::{create_effect, ALL_EFFECTS};
use pongo_effects::hardware::{Colour, KeyboardBackend};
use pongo_effects::EffectRunner;

use std::env;
use std::mem::size_of;
use std::sync::Mutex;

use windows::core::{w, BOOL, PCWSTR};
use windows::Win32::Foundation::{COLORREF, HINSTANCE, HWND, LPARAM, LRESULT, RECT, WPARAM};
use windows::Win32::Graphics::Dwm::{DwmSetWindowAttribute, DWMWA_USE_IMMERSIVE_DARK_MODE};
use windows::Win32::Graphics::Gdi::{
    BeginPaint, CreateFontW, CreatePen, CreateSolidBrush, DeleteObject, DrawTextW, EndPaint,
    FillRect, InvalidateRect, RoundRect, SelectObject, SetBkMode, SetTextColor, CLEARTYPE_QUALITY,
    CLIP_DEFAULT_PRECIS, DEFAULT_CHARSET, DRAW_TEXT_FORMAT, DT_CENTER, DT_LEFT, DT_NOPREFIX,
    DT_SINGLELINE, DT_VCENTER, HDC, HFONT, OUT_DEFAULT_PRECIS, PAINTSTRUCT, PS_SOLID, TRANSPARENT,
};
use windows::Win32::System::LibraryLoader::GetModuleHandleW;
use windows::Win32::System::Registry::{
    RegCloseKey, RegCreateKeyExW, RegDeleteValueW, RegOpenKeyExW, RegQueryValueExW, RegSetValueExW,
    HKEY, HKEY_CURRENT_USER, KEY_READ, KEY_WRITE, REG_DWORD, REG_OPTION_NON_VOLATILE, REG_SZ,
};
use windows::Win32::UI::Shell::{
    Shell_NotifyIconW, NIF_ICON, NIF_MESSAGE, NIF_TIP, NIM_ADD, NIM_DELETE, NOTIFYICONDATAW,
};
use windows::Win32::UI::WindowsAndMessaging::{
    CreateWindowExW, DefWindowProcW, DispatchMessageW, GetClientRect, GetMessageW, LoadCursorW,
    LoadIconW, PostQuitMessage, RegisterClassExW, ShowWindow, TranslateMessage, CS_HREDRAW,
    CS_VREDRAW, CW_USEDEFAULT, IDC_ARROW, IDI_APPLICATION, SW_HIDE, SW_RESTORE, SW_SHOW,
    WINDOW_EX_STYLE, WM_APP, WM_CREATE, WM_DESTROY, WM_LBUTTONDOWN, WM_LBUTTONUP, WM_MOUSEMOVE,
    WM_PAINT, WM_SIZE, WNDCLASSEXW, WS_CAPTION, WS_MINIMIZEBOX, WS_OVERLAPPED, WS_SYSMENU,
};

const WM_TRAYICON: u32 = WM_APP + 1;

// Color Palette (Dark Slate)
const CLR_WINDOW_BG: COLORREF = COLORREF(0x1A1412); // #12141A (BGR format)
const CLR_HEADER_BG: COLORREF = COLORREF(0x261C18); // #181C26
const CLR_CARD_BG: COLORREF = COLORREF(0x271D19);   // #191D27
const CLR_CARD_HOVER: COLORREF = COLORREF(0x3A2A23);// #232A3A
const CLR_CARD_SEL: COLORREF = COLORREF(0x4E301E);  // #1E304E
const CLR_CARD_BORDER: COLORREF = COLORREF(0x3A2C26);// #262C3A
const CLR_CARD_SEL_BORDER: COLORREF = COLORREF(0xF6823B); // #3B82F6
const CLR_TEXT_PRIMARY: COLORREF = COLORREF(0xF6F4F3);
const CLR_TEXT_SECONDARY: COLORREF = COLORREF(0xAFA39C);
const CLR_STATUS_GREEN: COLORREF = COLORREF(0x81B910);

// Buttons
const CLR_BTN_APPLY: COLORREF = COLORREF(0xEB6325);
const CLR_BTN_STOP: COLORREF = COLORREF(0x2626DC);

struct AppState {
    selected_idx: usize,
    running_idx: Option<usize>,
    hover_idx: Option<usize>,
    run_at_startup: bool,
    runner: Option<EffectRunner>,
}

static STATE: Mutex<Option<AppState>> = Mutex::new(None);

const REG_KEY_SETTINGS: PCWSTR = w!("Software\\KeyboardEffects\\Settings");
const REG_KEY_RUN: PCWSTR = w!("Software\\Microsoft\\Windows\\CurrentVersion\\Run");
const APP_NAME: PCWSTR = w!("PongoKeyboardEffects");

fn load_settings() -> (usize, bool) {
    let mut selected_idx = 0;
    let mut run_at_startup = false;

    unsafe {
        let mut hkey = HKEY::default();
        if RegOpenKeyExW(HKEY_CURRENT_USER, REG_KEY_SETTINGS, Some(0), KEY_READ, &mut hkey).is_ok() {
            let mut buf = [0u16; 64];
            let mut size = (buf.len() * 2) as u32;
            if RegQueryValueExW(
                hkey,
                w!("LastTheme"),
                None,
                None,
                Some(buf.as_mut_ptr() as *mut u8),
                Some(&mut size),
            )
            .is_ok()
            {
                let theme = String::from_utf16_lossy(&buf[..(size as usize / 2).saturating_sub(1)]);
                for (i, eff) in ALL_EFFECTS.iter().enumerate() {
                    if eff.id.eq_ignore_ascii_case(&theme) {
                        selected_idx = i;
                        break;
                    }
                }
            }

            let mut run_val: u32 = 0;
            let mut val_size = 4u32;
            if RegQueryValueExW(
                hkey,
                w!("RunAtStartup"),
                None,
                None,
                Some(&mut run_val as *mut u32 as *mut u8),
                Some(&mut val_size),
            )
            .is_ok()
            {
                run_at_startup = run_val != 0;
            }

            let _ = RegCloseKey(hkey);
        }
    }

    (selected_idx, run_at_startup)
}

fn save_settings(theme_id: &str, run_at_startup: bool) {
    unsafe {
        let mut hkey = HKEY::default();
        if RegCreateKeyExW(
            HKEY_CURRENT_USER,
            REG_KEY_SETTINGS,
            Some(0),
            None,
            REG_OPTION_NON_VOLATILE,
            KEY_WRITE,
            None,
            &mut hkey,
            None,
        )
        .is_ok()
        {
            let wide_theme: Vec<u16> = theme_id.encode_utf16().chain(std::iter::once(0)).collect();
            let _ = RegSetValueExW(
                hkey,
                w!("LastTheme"),
                Some(0),
                REG_SZ,
                Some(std::slice::from_raw_parts(
                    wide_theme.as_ptr() as *const u8,
                    wide_theme.len() * 2,
                )),
            );

            let val: u32 = if run_at_startup { 1 } else { 0 };
            let _ = RegSetValueExW(
                hkey,
                w!("RunAtStartup"),
                Some(0),
                REG_DWORD,
                Some(std::slice::from_raw_parts(
                    &val as *const u32 as *const u8,
                    4,
                )),
            );

            let _ = RegCloseKey(hkey);
        }

        // Windows Startup Run registry
        if let Ok(exe_path) = env::current_exe() {
            let mut run_hkey = HKEY::default();
            if RegOpenKeyExW(HKEY_CURRENT_USER, REG_KEY_RUN, Some(0), KEY_WRITE, &mut run_hkey).is_ok() {
                if run_at_startup {
                    let path_str = exe_path.to_string_lossy();
                    let wide_path: Vec<u16> =
                        path_str.encode_utf16().chain(std::iter::once(0)).collect();
                    let _ = RegSetValueExW(
                        run_hkey,
                        APP_NAME,
                        Some(0),
                        REG_SZ,
                        Some(std::slice::from_raw_parts(
                            wide_path.as_ptr() as *const u8,
                            wide_path.len() * 2,
                        )),
                    );
                } else {
                    let _ = RegDeleteValueW(run_hkey, APP_NAME);
                }
                let _ = RegCloseKey(run_hkey);
            }
        }
    }
}

fn create_app_font(size: i32, weight: i32) -> HFONT {
    unsafe {
        CreateFontW(
            size,
            0,
            0,
            0,
            weight,
            0,
            0,
            0,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            0,
            w!("Segoe UI"),
        )
    }
}

fn draw_rounded_rect(hdc: HDC, rc: &RECT, radius: i32, fill: COLORREF, border: COLORREF) {
    unsafe {
        let pen = CreatePen(PS_SOLID, 1, border);
        let brush = CreateSolidBrush(fill);
        let old_pen = SelectObject(hdc, pen.into());
        let old_brush = SelectObject(hdc, brush.into());

        let _ = RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);

        let _ = SelectObject(hdc, old_pen);
        let _ = SelectObject(hdc, old_brush);
        let _ = DeleteObject(pen.into());
        let _ = DeleteObject(brush.into());
    }
}

fn draw_text_helper(hdc: HDC, text: &str, rc: &mut RECT, format: DRAW_TEXT_FORMAT) {
    let mut wide: Vec<u16> = text.encode_utf16().collect();
    unsafe {
        let _ = DrawTextW(hdc, &mut wide, rc, format);
    }
}

// Card geometry layout
const LIST_TOP: i32 = 78;
const CARD_HEIGHT: i32 = 48;
const CARD_GAP: i32 = 6;
const CARD_MARGIN_X: i32 = 18;

fn get_card_rect(idx: usize, width: i32) -> RECT {
    let top = LIST_TOP + (idx as i32) * (CARD_HEIGHT + CARD_GAP);
    RECT {
        left: CARD_MARGIN_X,
        top,
        right: width - CARD_MARGIN_X,
        bottom: top + CARD_HEIGHT,
    }
}

// Button layout
fn get_btn_apply_rect(width: i32) -> RECT {
    RECT {
        left: 18,
        top: 520,
        right: (width / 2) - 6,
        bottom: 560,
    }
}

fn get_btn_stop_rect(width: i32) -> RECT {
    RECT {
        left: (width / 2) + 6,
        top: 520,
        right: width - 18,
        bottom: 560,
    }
}

fn get_startup_checkbox_rect(width: i32) -> RECT {
    RECT {
        left: 20,
        top: 572,
        right: width - 20,
        bottom: 594,
    }
}

unsafe extern "system" fn wnd_proc(
    hwnd: HWND,
    msg: u32,
    w_param: WPARAM,
    l_param: LPARAM,
) -> LRESULT {
    match msg {
        WM_CREATE => {
            let dark_mode = BOOL::from(true);
            let _ = DwmSetWindowAttribute(
                hwnd,
                DWMWA_USE_IMMERSIVE_DARK_MODE,
                &raw const dark_mode as *const _,
                size_of::<BOOL>() as u32,
            );

            // Add system tray icon
            let mut nid = NOTIFYICONDATAW::default();
            nid.cbSize = size_of::<NOTIFYICONDATAW>() as u32;
            nid.hWnd = hwnd;
            nid.uID = 1;
            nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
            nid.uCallbackMessage = WM_TRAYICON;
            nid.hIcon = LoadIconW(None, IDI_APPLICATION).unwrap_or_default();
            let tip = w!("Pongo Keyboard Effects");
            let tip_slice = tip.as_wide();
            nid.szTip[..tip_slice.len()].copy_from_slice(tip_slice);
            let _ = Shell_NotifyIconW(NIM_ADD, &nid);

            LRESULT(0)
        }
        WM_PAINT => {
            let mut ps = PAINTSTRUCT::default();
            let hdc = BeginPaint(hwnd, &mut ps);

            let mut rc_client = RECT::default();
            let _ = GetClientRect(hwnd, &mut rc_client);
            let width = rc_client.right - rc_client.left;

            // 1. Background
            let bg_brush = CreateSolidBrush(CLR_WINDOW_BG);
            FillRect(hdc, &rc_client, bg_brush);
            let _ = DeleteObject(bg_brush.into());

            SetBkMode(hdc, TRANSPARENT);

            // 2. Header Banner
            let header_brush = CreateSolidBrush(CLR_HEADER_BG);
            let rc_header = RECT {
                left: 0,
                top: 0,
                right: width,
                bottom: 64,
            };
            FillRect(hdc, &rc_header, header_brush);
            let _ = DeleteObject(header_brush.into());

            let font_title = create_app_font(-16, 700);
            let font_sub = create_app_font(-11, 400);
            let font_item_title = create_app_font(-13, 600);
            let font_item_desc = create_app_font(-11, 400);
            let font_btn = create_app_font(-13, 700);

            // Title
            let old_font = SelectObject(hdc, font_title.into());
            SetTextColor(hdc, CLR_TEXT_PRIMARY);
            let mut rc_title = RECT {
                left: 20,
                top: 12,
                right: width - 20,
                bottom: 34,
            };
            draw_text_helper(hdc, "Pongo Keyboard Effects (Rust)", &mut rc_title, DT_LEFT | DT_SINGLELINE);

            // Subtitle
            let _ = SelectObject(hdc, font_sub.into());
            SetTextColor(hdc, CLR_TEXT_SECONDARY);
            let mut rc_sub = RECT {
                left: 20,
                top: 36,
                right: width - 20,
                bottom: 56,
            };
            draw_text_helper(hdc, "Klik efek apa saja untuk langsung menyalakan lampu", &mut rc_sub, DT_LEFT | DT_SINGLELINE);

            // 3. Effect Cards
            let state_lock = STATE.lock().unwrap();
            let state = state_lock.as_ref().unwrap();

            for (i, eff) in ALL_EFFECTS.iter().enumerate() {
                let card_rc = get_card_rect(i, width);
                let is_sel = state.selected_idx == i;
                let is_running = state.running_idx == Some(i);
                let is_hover = state.hover_idx == Some(i);

                let fill = if is_running {
                    CLR_CARD_SEL
                } else if is_sel {
                    CLR_CARD_SEL
                } else if is_hover {
                    CLR_CARD_HOVER
                } else {
                    CLR_CARD_BG
                };

                let border = if is_running || is_sel {
                    CLR_CARD_SEL_BORDER
                } else {
                    CLR_CARD_BORDER
                };

                draw_rounded_rect(hdc, &card_rc, 8, fill, border);

                // Indicator dot (Green if active)
                if is_running {
                    let dot_rc = RECT {
                        left: card_rc.left + 10,
                        top: card_rc.top + 18,
                        right: card_rc.left + 22,
                        bottom: card_rc.top + 30,
                    };
                    draw_rounded_rect(hdc, &dot_rc, 12, CLR_STATUS_GREEN, CLR_STATUS_GREEN);
                }

                // Title
                let text_x = if is_running { card_rc.left + 28 } else { card_rc.left + 14 };
                let _ = SelectObject(hdc, font_item_title.into());
                SetTextColor(hdc, CLR_TEXT_PRIMARY);
                let mut rc_item_title = RECT {
                    left: text_x,
                    top: card_rc.top + 6,
                    right: card_rc.right - 14,
                    bottom: card_rc.top + 24,
                };
                draw_text_helper(hdc, eff.label, &mut rc_item_title, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

                // Desc
                let _ = SelectObject(hdc, font_item_desc.into());
                SetTextColor(hdc, CLR_TEXT_SECONDARY);
                let mut rc_item_desc = RECT {
                    left: text_x,
                    top: card_rc.top + 26,
                    right: card_rc.right - 14,
                    bottom: card_rc.bottom - 4,
                };
                draw_text_helper(hdc, eff.description, &mut rc_item_desc, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
            }

            // 4. Buttons
            let mut rc_apply = get_btn_apply_rect(width);
            let mut rc_stop = get_btn_stop_rect(width);

            draw_rounded_rect(hdc, &rc_apply, 6, CLR_BTN_APPLY, CLR_BTN_APPLY);
            draw_rounded_rect(hdc, &rc_stop, 6, CLR_BTN_STOP, CLR_BTN_STOP);

            let _ = SelectObject(hdc, font_btn.into());
            SetTextColor(hdc, CLR_TEXT_PRIMARY);

            draw_text_helper(hdc, "Apply Effect", &mut rc_apply, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
            draw_text_helper(hdc, "Stop Lighting", &mut rc_stop, DT_SINGLELINE | DT_VCENTER | DT_CENTER);

            // 5. Startup Checkbox
            let mut rc_check = get_startup_checkbox_rect(width);
            let check_symbol = if state.run_at_startup { "[x] " } else { "[ ] " };
            let checkbox_label = format!("{}Run automatically at Windows startup", check_symbol);

            let _ = SelectObject(hdc, font_item_desc.into());
            SetTextColor(hdc, CLR_TEXT_SECONDARY);
            draw_text_helper(hdc, &checkbox_label, &mut rc_check, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            // Cleanup
            let _ = SelectObject(hdc, old_font);
            let _ = DeleteObject(font_title.into());
            let _ = DeleteObject(font_sub.into());
            let _ = DeleteObject(font_item_title.into());
            let _ = DeleteObject(font_item_desc.into());
            let _ = DeleteObject(font_btn.into());

            let _ = EndPaint(hwnd, &ps);
            LRESULT(0)
        }
        WM_LBUTTONDOWN => {
            let x = (l_param.0 & 0xFFFF) as i16 as i32;
            let y = ((l_param.0 >> 16) & 0xFFFF) as i16 as i32;

            let mut rc_client = RECT::default();
            let _ = GetClientRect(hwnd, &mut rc_client);
            let width = rc_client.right - rc_client.left;

            let mut state_lock = STATE.lock().unwrap();
            let state = state_lock.as_mut().unwrap();

            // 1. Click card -> Select AND APPLY IMMEDIATELY
            for i in 0..ALL_EFFECTS.len() {
                let rc = get_card_rect(i, width);
                if x >= rc.left && x <= rc.right && y >= rc.top && y <= rc.bottom {
                    state.selected_idx = i;
                    let theme_id = ALL_EFFECTS[i].id;

                    if let Some(effect) = create_effect(theme_id) {
                        if let Some(mut old_runner) = state.runner.take() {
                            old_runner.stop();
                        }
                        let backend = KeyboardBackend::auto();
                        state.runner = Some(EffectRunner::start(effect, backend));
                        state.running_idx = Some(i);
                        save_settings(theme_id, state.run_at_startup);
                    }

                    let _ = InvalidateRect(Some(hwnd), None, true);
                    return LRESULT(0);
                }
            }

            // 2. Click Apply button -> Apply selected
            let rc_apply = get_btn_apply_rect(width);
            if x >= rc_apply.left && x <= rc_apply.right && y >= rc_apply.top && y <= rc_apply.bottom {
                let selected = state.selected_idx;
                let theme_id = ALL_EFFECTS[selected].id;

                if let Some(effect) = create_effect(theme_id) {
                    if let Some(mut old_runner) = state.runner.take() {
                        old_runner.stop();
                    }
                    let backend = KeyboardBackend::auto();
                    state.runner = Some(EffectRunner::start(effect, backend));
                    state.running_idx = Some(selected);
                    save_settings(theme_id, state.run_at_startup);
                }
                let _ = InvalidateRect(Some(hwnd), None, true);
                return LRESULT(0);
            }

            // 3. Click Stop button -> Stop and turn off
            let rc_stop = get_btn_stop_rect(width);
            if x >= rc_stop.left && x <= rc_stop.right && y >= rc_stop.top && y <= rc_stop.bottom {
                if let Some(mut runner) = state.runner.take() {
                    runner.stop();
                }
                let mut backend = KeyboardBackend::auto();
                backend.set_colour(Colour::BLACK);
                state.running_idx = None;
                let _ = InvalidateRect(Some(hwnd), None, true);
                return LRESULT(0);
            }

            // 4. Click Startup checkbox
            let rc_check = get_startup_checkbox_rect(width);
            if x >= rc_check.left && x <= rc_check.right && y >= rc_check.top && y <= rc_check.bottom {
                state.run_at_startup = !state.run_at_startup;
                let theme_id = ALL_EFFECTS[state.selected_idx].id;
                save_settings(theme_id, state.run_at_startup);
                let _ = InvalidateRect(Some(hwnd), None, true);
                return LRESULT(0);
            }

            LRESULT(0)
        }
        WM_MOUSEMOVE => {
            let x = (l_param.0 & 0xFFFF) as i16 as i32;
            let y = ((l_param.0 >> 16) & 0xFFFF) as i16 as i32;

            let mut rc_client = RECT::default();
            let _ = GetClientRect(hwnd, &mut rc_client);
            let width = rc_client.right - rc_client.left;

            let mut hovered_card = None;
            for i in 0..ALL_EFFECTS.len() {
                let rc = get_card_rect(i, width);
                if x >= rc.left && x <= rc.right && y >= rc.top && y <= rc.bottom {
                    hovered_card = Some(i);
                    break;
                }
            }

            let mut state_lock = STATE.lock().unwrap();
            let state = state_lock.as_mut().unwrap();
            if state.hover_idx != hovered_card {
                state.hover_idx = hovered_card;
                let _ = InvalidateRect(Some(hwnd), None, false);
            }

            LRESULT(0)
        }
        WM_SIZE => {
            if w_param.0 == SIZE_MINIMIZED as usize {
                let _ = ShowWindow(hwnd, SW_HIDE);
            }
            LRESULT(0)
        }
        WM_TRAYICON => {
            if l_param.0 as u32 == WM_LBUTTONUP {
                let _ = ShowWindow(hwnd, SW_RESTORE);
                let _ = ShowWindow(hwnd, SW_SHOW);
            }
            LRESULT(0)
        }
        WM_DESTROY => {
            let mut nid = NOTIFYICONDATAW::default();
            nid.cbSize = size_of::<NOTIFYICONDATAW>() as u32;
            nid.hWnd = hwnd;
            nid.uID = 1;
            let _ = Shell_NotifyIconW(NIM_DELETE, &nid);

            let mut state_lock = STATE.lock().unwrap();
            if let Some(mut state) = state_lock.take() {
                if let Some(mut runner) = state.runner.take() {
                    runner.stop();
                }
            }

            PostQuitMessage(0);
            LRESULT(0)
        }
        _ => DefWindowProcW(hwnd, msg, w_param, l_param),
    }
}

const SIZE_MINIMIZED: u32 = 1;

fn main() {
    let (saved_idx, run_at_startup) = load_settings();

    // Auto-start lighting immediately as soon as GUI opens!
    let theme_id = ALL_EFFECTS[saved_idx].id;
    let initial_runner = create_effect(theme_id).map(|eff| {
        let backend = KeyboardBackend::auto();
        EffectRunner::start(eff, backend)
    });

    let running_idx = if initial_runner.is_some() {
        Some(saved_idx)
    } else {
        None
    };

    *STATE.lock().unwrap() = Some(AppState {
        selected_idx: saved_idx,
        running_idx,
        hover_idx: None,
        run_at_startup,
        runner: initial_runner,
    });

    unsafe {
        let class_name = w!("PongoKeyboardEffectsWindow");
        let instance = GetModuleHandleW(None).unwrap_or_default();

        let wc = WNDCLASSEXW {
            cbSize: size_of::<WNDCLASSEXW>() as u32,
            style: CS_HREDRAW | CS_VREDRAW,
            lpfnWndProc: Some(wnd_proc),
            cbClsExtra: 0,
            cbWndExtra: 0,
            hInstance: instance.into(),
            hIcon: LoadIconW(None, IDI_APPLICATION).unwrap_or_default(),
            hCursor: LoadCursorW(None, IDC_ARROW).unwrap_or_default(),
            hbrBackground: windows::Win32::Graphics::Gdi::HBRUSH::default(),
            lpszMenuName: PCWSTR::null(),
            lpszClassName: class_name,
            hIconSm: windows::Win32::UI::WindowsAndMessaging::HICON::default(),
        };

        let _ = RegisterClassExW(&wc);

        let window_title = w!("Pongo Keyboard Effects");
        let hwnd = CreateWindowExW(
            WINDOW_EX_STYLE(0),
            class_name,
            window_title,
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            460,
            650,
            None,
            None,
            Some(HINSTANCE(instance.0)),
            None,
        );

        if let Ok(hwnd) = hwnd {
            let _ = ShowWindow(hwnd, SW_SHOW);
            let mut msg = windows::Win32::UI::WindowsAndMessaging::MSG::default();
            while GetMessageW(&mut msg, None, 0, 0).into() {
                let _ = TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
    }
}
