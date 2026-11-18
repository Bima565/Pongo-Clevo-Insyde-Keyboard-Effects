use pongo_effects::effects::{create_effect, ALL_EFFECTS};
use pongo_effects::hardware::{Colour, KeyboardBackend};
use pongo_effects::run_blocking;
use std::env;
use std::sync::atomic::AtomicBool;
use std::sync::Arc;
use windows::Win32::System::Threading::{
    GetCurrentProcess, SetPriorityClass, BELOW_NORMAL_PRIORITY_CLASS,
};

fn set_process_priority() {
    unsafe {
        let _ = SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
    }
}

fn print_help() {
    println!("Pongo Clevo Keyboard Effects (Rust Edition v2.0)");
    println!("Usage:");
    println!("  CppKeyboardColour.exe theme <themeName>  Menjalankan efek animasi");
    println!("  CppKeyboardColour.exe color <R> <G> <B>  Mengatur warna solid statis (0-255)");
    println!("  CppKeyboardColour.exe off                Mematikan lampu keyboard");
    println!("  CppKeyboardColour.exe list               Menampilkan daftar semua efek");
    println!("  CppKeyboardColour.exe help               Menampilkan bantuan ini\n");
    println!("Daftar tema efek:");
    for effect in ALL_EFFECTS {
        println!("  - {:<18} : {}", effect.id, effect.description);
    }
}

fn main() {
    set_process_priority();

    let args: Vec<String> = env::args().collect();
    if args.len() < 2 {
        print_help();
        return;
    }

    let mut backend = KeyboardBackend::auto();

    match args[1].to_lowercase().as_str() {
        "theme" => {
            if args.len() < 3 {
                eprintln!("Error: Parameter tema tidak ditemukan.");
                println!("Contoh: CppKeyboardColour.exe theme breathe");
                return;
            }

            let theme_name = &args[2];
            let effect = match create_effect(theme_name) {
                Some(eff) => eff,
                None => {
                    eprintln!("Error: Efek '{}' tidak dikenali.", theme_name);
                    println!("Gunakan 'CppKeyboardColour.exe list' untuk melihat daftar efek.");
                    return;
                }
            };

            println!("Menjalankan efek: {} ({})", effect.name(), theme_name);
            println!("Tekan Ctrl+C untuk berhenti.");

            let stop_signal = Arc::new(AtomicBool::new(false));
            let stop_clone = Arc::clone(&stop_signal);

            // Handle Ctrl+C gracefully
            let _ = ctrlc_handler(stop_clone);

            run_blocking(effect, backend, stop_signal);
        }
        "off" => {
            println!("Mematikan backlight keyboard...");
            backend.set_colour(Colour::BLACK);
            println!("Selesai.");
        }
        "color" | "colour" => {
            if args.len() < 5 {
                eprintln!("Format salah. Gunakan: CppKeyboardColour.exe color <R> <G> <B>");
                return;
            }
            let r: u8 = args[2].parse().unwrap_or(0);
            let g: u8 = args[3].parse().unwrap_or(0);
            let b: u8 = args[4].parse().unwrap_or(0);
            let c = Colour::new(r, g, b);
            println!("Mengatur warna keyboard ke RGB({}, {}, {})...", r, g, b);
            backend.set_colour(c);
            println!("Selesai.");
        }
        "list" => {
            println!("Daftar Efek Pencahayaan Tersedia:");
            for (idx, eff) in ALL_EFFECTS.iter().enumerate() {
                println!("{}. {:<18} -> {}", idx + 1, eff.id, eff.description);
            }
        }
        "help" | "-h" | "--help" => {
            print_help();
        }
        unknown => {
            // Check if user passed the effect name directly without "theme" prefix
            if let Some(effect) = create_effect(unknown) {
                println!("Menjalankan efek: {} ({})", effect.name(), unknown);
                println!("Tekan Ctrl+C untuk berhenti.");

                let stop_signal = Arc::new(AtomicBool::new(false));
                let stop_clone = Arc::clone(&stop_signal);
                let _ = ctrlc_handler(stop_clone);

                run_blocking(effect, backend, stop_signal);
            } else {
                eprintln!("Perintah tidak dikenal: {}", unknown);
                print_help();
            }
        }
    }
}

fn ctrlc_handler(_stop: Arc<AtomicBool>) -> Result<(), ()> {
    unsafe {
        use windows::core::BOOL;
        use windows::Win32::System::Console::SetConsoleCtrlHandler;
        unsafe extern "system" fn handler(_: u32) -> BOOL {
            std::process::exit(0);
        }
        let _ = SetConsoleCtrlHandler(Some(handler), true);
    }
    Ok(())
}
