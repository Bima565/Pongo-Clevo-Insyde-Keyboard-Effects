use pongo_effects::effects::{create_effect, ALL_EFFECTS};
use pongo_effects::hardware::{Colour, KeyboardBackend};
use pongo_effects::{run_blocking, EffectRunner};

use std::env;
use std::io::{self, BufRead, Write};
use std::sync::atomic::AtomicBool;
use std::sync::Arc;

use windows::core::BOOL;
use windows::Win32::System::Console::SetConsoleCtrlHandler;
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
    println!("Penggunaan:");
    println!("  CppKeyboardColour.exe                  Menjalankan mode interaktif langsung");
    println!("  CppKeyboardColour.exe theme <namaEfek> Menjalankan efek animasi tertentu");
    println!("  CppKeyboardColour.exe color <R> <G> <B> Mengatur warna solid statis (0-255)");
    println!("  CppKeyboardColour.exe off              Mematikan lampu keyboard");
    println!("  CppKeyboardColour.exe list             Menampilkan daftar semua efek");
    println!("  CppKeyboardColour.exe help             Menampilkan bantuan ini\n");
    println!("Daftar tema efek:");
    for effect in ALL_EFFECTS {
        println!("  - {:<18} : {}", effect.id, effect.description);
    }
}

fn interactive_mode() {
    println!("============================================================");
    println!("      PONGO CLEVO KEYBOARD EFFECTS (RUST EDITION)");
    println!("============================================================");

    let default_theme = "breathe";

    let mut runner: Option<EffectRunner> = None;
    if let Some(effect) = create_effect(default_theme) {
        let backend = KeyboardBackend::auto();
        runner = Some(EffectRunner::start(effect, backend));
        println!(">> Lampu keyboard otomatis AKTIF dengan efek: [Breathe]");
    }

    println!("\nPilih efek (ketik angka 1-8 lalu tekan Enter):");
    println!("  [1] Breathe             [5] Music Reactive (WASAPI Audio)");
    println!("  [2] New Colour Breathe  [6] RGB Zone Test");
    println!("  [3] Colour Transform    [7] Typing Boost (Reaktif Ketikan)");
    println!("  [4] Strobe Transform    [8] Screen Reactive (Ambilight)");
    println!("  [0] Matikan Lampu       [q] Keluar");
    println!("------------------------------------------------------------");

    let stdin = io::stdin();
    let mut reader = stdin.lock();
    let mut input = String::new();

    loop {
        print!("Pilih efek [0-8, q]: ");
        let _ = io::stdout().flush();
        input.clear();

        if reader.read_line(&mut input).is_err() {
            break;
        }

        let cmd = input.trim();
        if cmd.eq_ignore_ascii_case("q") || cmd.eq_ignore_ascii_case("exit") {
            println!("Menutup aplikasi...");
            if let Some(mut r) = runner.take() {
                r.stop();
            }
            break;
        }

        let target_theme = match cmd {
            "1" => Some("breathe"),
            "2" => Some("newcolourbreathe"),
            "3" => Some("colourtransform"),
            "4" => Some("strobe"),
            "5" => Some("musicadaptive"),
            "6" => Some("rgbzones"),
            "7" => Some("fireflicker"),
            "8" => Some("screenreactive"),
            "0" => {
                if let Some(mut r) = runner.take() {
                    r.stop();
                }
                let mut backend = KeyboardBackend::auto();
                backend.set_colour(Colour::BLACK);
                println!(">> Lampu keyboard dimatikan.");
                continue;
            }
            _ => None,
        };

        if let Some(theme) = target_theme {
            if let Some(mut old_runner) = runner.take() {
                old_runner.stop();
            }

            if let Some(effect) = create_effect(theme) {
                let name = effect.name();
                let backend = KeyboardBackend::auto();
                runner = Some(EffectRunner::start(effect, backend));
                println!(">> Berhasil switch ke efek: [{}] ({})", name, theme);
            }
        } else {
            println!("Pilihan tidak valid. Masukkan angka 1-8, 0, atau q.");
        }
    }
}

fn main() {
    set_process_priority();

    let args: Vec<String> = env::args().collect();
    if args.len() < 2 {
        // Double-clicked or run without CLI args: run interactive mode immediately!
        interactive_mode();
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
        unsafe extern "system" fn handler(_: u32) -> BOOL {
            std::process::exit(0);
        }
        let _ = SetConsoleCtrlHandler(Some(handler), true);
    }
    Ok(())
}
