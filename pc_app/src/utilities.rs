use std::fs;
use std::path::{Path, PathBuf};
use tokio::time::Duration;
use bytemuck::{Pod, Zeroable};
use crate::app_state::AppState;

pub const DEVICE_NAME: &str = "Nano 33 Head Tracker";
pub const SERVICE_UUID: &str = "19b10000-e8f2-537e-4f6c-d104768a1214";
pub const CHAR_UUID: &str = "19b10001-e8f2-537e-4f6c-d104768a1214";
pub const UDP_IP: &str = "127.0.0.1";
pub const UDP_PORT: u16 = 4242;
pub const CONNECTION_TIMEOUT: Duration = Duration::from_secs(5);
pub const RECONNECT_DELAY: Duration = Duration::from_secs(2);

#[repr(C, packed)]
#[derive(Copy, Clone, Pod, Zeroable)]
pub struct HatireData {
    pub header1: u16,
    pub header2: u16,
    pub yaw: f32,
    pub pitch: f32,
    pub roll: f32,
    pub accel_x: f32,
    pub accel_y: f32,
    pub accel_z: f32,
    pub footer: u16,
}

pub fn is_usb_connection() -> bool {
    use serialport::{available_ports, SerialPortType};
    if let Ok(ports) = available_ports() {
        for port in ports {
            if let SerialPortType::UsbPort(_) = port.port_type {
                // Check specific product/vendor IDs if necessary
                return true;
            }
        }
    }
    false
}

pub fn is_ble_connection(app_state: &AppState) -> bool {
    let status = app_state.status.lock().unwrap().clone();
    status == "Connected"
}

pub fn find_project_dir(base_dir: &Path) -> Option<PathBuf> {
    if let Ok(entries) = fs::read_dir(base_dir) {
        for entry in entries {
            if let Ok(entry) = entry {
                let path = entry.path();
                if path.is_dir() {
                    let name = path.file_name()?.to_string_lossy();
                    if name.starts_with("Nano33") && path.join("platformio.ini").exists() {
                        return Some(path);
                    }
                }
            }
        }
    }
    None
}

pub fn save_local_version(version: &str) -> Result<(), String> {
    let version_file = get_project_directory().join("PlatformIO_AppVersion.txt");
    fs::write(version_file, version).map_err(|e| format!("Failed to write version file: {}", e))
}

pub fn read_local_version() -> Option<String> {
    let version_file = get_project_directory().join("PlatformIO_AppVersion.txt");
    fs::read_to_string(version_file).ok()
}

pub fn fetch_latest_commit(repo_url: &str) -> Result<String, String> {
    let api_url = format!("https://api.github.com/repos/{}/commits/V2_Update", repo_url);
    let client = reqwest::blocking::Client::new();
    let response = client
        .get(&api_url)
        .header("User-Agent", "rust-app")
        .send()
        .map_err(|e| format!("Failed to fetch commit info: {}", e))?;

    if !response.status().is_success() {
        return Err(format!("GitHub API request failed: HTTP {}", response.status()));
    }

    let json: serde_json::Value = response.json().map_err(|e| format!("Failed to parse JSON: {}", e))?;
    json["sha"]
        .as_str()
        .map(String::from)
        .ok_or("Commit hash not found in response".to_string())
}

pub fn get_app_directory() -> PathBuf {
    std::env::current_exe()
        .map(|exe_path| exe_path.parent().unwrap().to_path_buf())
        .unwrap_or_else(|_| PathBuf::from("."))
}

pub fn get_project_directory() -> PathBuf {
    get_app_directory().join("platformio_project")
}

pub fn parse_data(data: &[u8]) -> HatireData {
    // Ensure the data is at least 30 bytes
    if data.len() < 30 {
        panic!("Notification data size mismatch: expected 30 bytes, got {}", data.len());
    }

    // Extract the first 30 bytes
    let meaningful_data = &data[..30];

    // Convert bytes to HatireData
    HatireData::from_bytes(meaningful_data)
}

impl HatireData {
    fn from_bytes(bytes: &[u8]) -> Self {
        assert_eq!(bytes.len(), 30, "SizeMismatch: expected 30 bytes");
        *bytemuck::from_bytes(bytes)
    }
}