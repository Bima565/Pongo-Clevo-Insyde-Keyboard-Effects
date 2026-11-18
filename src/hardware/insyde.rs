use std::env;
use std::path::PathBuf;
use windows::core::{s, w, PCWSTR};
use windows::Win32::Foundation::{FreeLibrary, HMODULE};
use windows::Win32::System::LibraryLoader::{GetProcAddress, LoadLibraryW};
use windows::Win32::UI::WindowsAndMessaging::{MessageBoxW, MB_ICONERROR, MB_OK};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct Colour {
    pub r: u8,
    pub g: u8,
    pub b: u8,
}

impl Colour {
    pub const fn new(r: u8, g: u8, b: u8) -> Self {
        Self { r, g, b }
    }

    pub const BLACK: Self = Self::new(0, 0, 0);
    pub const WHITE: Self = Self::new(255, 255, 255);
    pub const RED: Self = Self::new(255, 0, 0);
    pub const GREEN: Self = Self::new(0, 255, 0);
    pub const BLUE: Self = Self::new(0, 0, 255);
}

type FnSetDCHUData = unsafe extern "system" fn(command: u32, buffer: *const u8, length: u32) -> u32;
type FnWriteAppSettings =
    unsafe extern "system" fn(page: u32, offset: u32, length: u32, buffer: *const u8) -> u32;

pub struct InsydeCommunicator {
    module: HMODULE,
    set_dchu_data: FnSetDCHUData,
    write_app_settings: FnWriteAppSettings,
    last_colour: Option<Colour>,
}

unsafe impl Send for InsydeCommunicator {}
unsafe impl Sync for InsydeCommunicator {}

impl InsydeCommunicator {
    pub fn new() -> Result<Self, String> {
        let dll_name = "InsydeDCHU.dll";

        let mut target_paths = Vec::new();

        // 1. Next to running executable and parent directories (covers target/release and project root)
        if let Ok(exe_path) = env::current_exe() {
            if let Some(parent) = exe_path.parent() {
                target_paths.push(parent.join(dll_name));
                if let Some(p2) = parent.parent() {
                    target_paths.push(p2.join(dll_name));
                    if let Some(p3) = p2.parent() {
                        target_paths.push(p3.join(dll_name));
                    }
                }
            }
        }

        // 2. Current working directory
        if let Ok(cwd) = env::current_dir() {
            target_paths.push(cwd.join(dll_name));
            if let Some(p) = cwd.parent() {
                target_paths.push(p.join(dll_name));
            }
        }

        // 3. Fallback standard DLL search
        target_paths.push(PathBuf::from(dll_name));

        let mut loaded_module = None;
        let mut tried_paths = Vec::new();

        for path in target_paths {
            if path.exists() || path.as_os_str() == dll_name {
                let path_str = path.to_string_lossy();
                tried_paths.push(path_str.to_string());
                let wide: Vec<u16> = path_str.encode_utf16().chain(std::iter::once(0)).collect();
                unsafe {
                    if let Ok(h) = LoadLibraryW(PCWSTR::from_raw(wide.as_ptr())) {
                        if !h.is_invalid() {
                            loaded_module = Some(h);
                            break;
                        }
                    }
                }
            }
        }

        let module = loaded_module.ok_or_else(|| {
            format!(
                "Gagal memuat {}.\nPastikan file InsydeDCHU.dll berada di folder aplikasi.\nJalur yang dicoba: {:?}",
                dll_name, tried_paths
            )
        })?;

        unsafe {
            let proc_set = GetProcAddress(module, s!("SetDCHU_Data"))
                .ok_or_else(|| "Fungsi SetDCHU_Data tidak ditemukan di InsydeDCHU.dll".to_string())?;
            let proc_write = GetProcAddress(module, s!("WriteAppSettings"))
                .ok_or_else(|| "Fungsi WriteAppSettings tidak ditemukan di InsydeDCHU.dll".to_string())?;

            let set_dchu_data: FnSetDCHUData = std::mem::transmute(proc_set);
            let write_app_settings: FnWriteAppSettings = std::mem::transmute(proc_write);

            Ok(Self {
                module,
                set_dchu_data,
                write_app_settings,
                last_colour: None,
            })
        }
    }

    pub fn set_colour(&mut self, colour: Colour) -> bool {
        let dchu_data = [colour.g, colour.r, colour.b, 0xF0];
        let raw_colour = [colour.r, colour.g, colour.b];
        let mode: u8 = 8;

        unsafe {
            (self.set_dchu_data)(103, dchu_data.as_ptr(), dchu_data.len() as u32);
            (self.write_app_settings)(2, 81, raw_colour.len() as u32, raw_colour.as_ptr());
            (self.write_app_settings)(2, 32, 1, &mode);
        }

        self.last_colour = Some(colour);
        true
    }
}

impl Drop for InsydeCommunicator {
    fn drop(&mut self) {
        if !self.module.is_invalid() {
            unsafe {
                let _ = FreeLibrary(self.module);
            }
        }
    }
}

pub enum KeyboardBackend {
    Hardware(InsydeCommunicator),
    Mock { last_colour: Option<Colour> },
}

impl KeyboardBackend {
    pub fn auto() -> Self {
        match InsydeCommunicator::new() {
            Ok(comm) => {
                println!("[Hardware] InsydeDCHU.dll terdeteksi & aktif.");
                Self::Hardware(comm)
            }
            Err(err) => {
                eprintln!("[Error Hardware] {}", err);
                // Show GUI alert so user knows why it didn't light up if DLL is missing
                unsafe {
                    let wide_err: Vec<u16> = err.encode_utf16().chain(std::iter::once(0)).collect();
                    let _ = MessageBoxW(
                        None,
                        PCWSTR::from_raw(wide_err.as_ptr()),
                        w!("Pongo Keyboard Effects - Hardware Driver"),
                        MB_ICONERROR | MB_OK,
                    );
                }
                Self::Mock { last_colour: None }
            }
        }
    }

    pub fn set_colour(&mut self, colour: Colour) -> bool {
        match self {
            Self::Hardware(h) => h.set_colour(colour),
            Self::Mock { last_colour } => {
                *last_colour = Some(colour);
                true
            }
        }
    }
}
