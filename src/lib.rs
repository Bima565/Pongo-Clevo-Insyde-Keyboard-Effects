pub mod effects;
pub mod hardware;
pub mod sensors;

use effects::Effect;
use hardware::KeyboardBackend;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::thread;

pub struct EffectRunner {
    stop_signal: Arc<AtomicBool>,
    handle: Option<thread::JoinHandle<()>>,
}

impl EffectRunner {
    pub fn start(mut effect: Box<dyn Effect>, mut backend: KeyboardBackend) -> Self {
        let stop_signal = Arc::new(AtomicBool::new(false));
        let stop_clone = Arc::clone(&stop_signal);

        let handle = thread::spawn(move || {
            while !stop_clone.load(Ordering::Relaxed) {
                if let Some(frame) = effect.next_frame() {
                    backend.set_colour(frame.colour);
                    thread::sleep(frame.duration);
                } else {
                    break;
                }
            }
        });

        Self {
            stop_signal,
            handle: Some(handle),
        }
    }

    pub fn stop(&mut self) {
        self.stop_signal.store(true, Ordering::SeqCst);
        if let Some(handle) = self.handle.take() {
            let _ = handle.join();
        }
    }

    pub fn is_running(&self) -> bool {
        !self.stop_signal.load(Ordering::Relaxed)
    }
}

impl Drop for EffectRunner {
    fn drop(&mut self) {
        self.stop();
    }
}

pub fn run_blocking(
    mut effect: Box<dyn Effect>,
    mut backend: KeyboardBackend,
    stop_signal: Arc<AtomicBool>,
) {
    while !stop_signal.load(Ordering::Relaxed) {
        if let Some(frame) = effect.next_frame() {
            backend.set_colour(frame.colour);
            thread::sleep(frame.duration);
        } else {
            break;
        }
    }
}
