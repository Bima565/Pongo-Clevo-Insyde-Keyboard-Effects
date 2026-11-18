use windows::core::Result;
use windows::Win32::Media::Audio::Endpoints::IAudioMeterInformation;
use windows::Win32::Media::Audio::{eMultimedia, eRender, IMMDeviceEnumerator, MMDeviceEnumerator};
use windows::Win32::System::Com::{
    CoCreateInstance, CoInitializeEx, CoUninitialize, CLSCTX_ALL, COINIT_MULTITHREADED,
};

pub struct AudioSensor {
    com_initialized: bool,
    meter: Option<IAudioMeterInformation>,
    enumerator: Option<IMMDeviceEnumerator>,
}

unsafe impl Send for AudioSensor {}

impl AudioSensor {
    pub fn new() -> Self {
        let hr = unsafe { CoInitializeEx(None, COINIT_MULTITHREADED) };
        let com_initialized = hr.is_ok();

        let mut sensor = Self {
            com_initialized,
            meter: None,
            enumerator: None,
        };

        let _ = sensor.init_meter();
        sensor
    }

    fn init_meter(&mut self) -> Result<()> {
        unsafe {
            let enumerator: IMMDeviceEnumerator =
                CoCreateInstance(&MMDeviceEnumerator, None, CLSCTX_ALL)?;
            let device = enumerator.GetDefaultAudioEndpoint(eRender, eMultimedia)?;
            let meter: IAudioMeterInformation = device.Activate(CLSCTX_ALL, None)?;

            self.meter = Some(meter);
            self.enumerator = Some(enumerator);
            Ok(())
        }
    }

    pub fn get_peak_level(&mut self) -> f32 {
        if self.meter.is_none() {
            let _ = self.init_meter();
        }

        if let Some(ref meter) = self.meter {
            unsafe {
                if let Ok(peak) = meter.GetPeakValue() {
                    return peak.clamp(0.0, 1.0);
                }
            }
        }

        // Try refreshing device if endpoint changed
        let _ = self.init_meter();
        0.0
    }
}

impl Drop for AudioSensor {
    fn drop(&mut self) {
        self.meter = None;
        self.enumerator = None;
        if self.com_initialized {
            unsafe {
                CoUninitialize();
            }
        }
    }
}
