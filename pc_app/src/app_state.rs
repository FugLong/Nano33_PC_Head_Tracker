use std::net::UdpSocket;
use std::sync::{Arc, Mutex};
use crate::utilities::{UDP_IP, UDP_PORT, fetch_latest_commit, read_local_version};

pub struct AppState {
    pub log: Arc<Mutex<Vec<String>>>,
    pub status: Arc<Mutex<String>>,
    pub udp_socket: UdpSocket,
    pub should_run: Arc<Mutex<bool>>,
    pub latest_script_version: Option<String>,
    pub notification_message: Arc<Mutex<Option<String>>>,
    pub is_flashing: Arc<Mutex<bool>>,
}

impl AppState {
    pub fn new() -> Self {
        let udp_socket = UdpSocket::bind("0.0.0.0:0").expect("Failed to bind UDP socket");
        udp_socket.connect((UDP_IP, UDP_PORT)).expect("Failed to connect to OpenTrack");
        let latest_script_version = fetch_latest_commit("FugLong/Nano33_PC_Head_Tracker").ok();

        // Initialize notification_message
        let notification_message = Arc::new(Mutex::new(None));

        Self {
            log: Arc::new(Mutex::new(Vec::new())),
            status: Arc::new(Mutex::new("Off".to_string())),
            udp_socket,
            should_run: Arc::new(Mutex::new(false)),
            latest_script_version,
            notification_message,
            is_flashing: Arc::new(Mutex::new(false)),
        }
    }

    // Method to set flashing state
    pub fn set_flashing(&self, is_flashing: bool) {
        let mut state = self.is_flashing.lock().unwrap();
        *state = is_flashing;
    }

    // Method to check flashing state
    pub fn is_flashing(&self) -> bool {
        *self.is_flashing.lock().unwrap()
    }

    // Method to check for updates
    pub fn update_notification(&self) {
        let latest_version = self.latest_script_version.as_ref();
        let local_version = read_local_version();
        
        let mut notification = self.notification_message.lock().unwrap();
        
        if let Some(latest_version) = latest_version {
            if local_version.as_deref() != Some(latest_version) {
                // Notify if versions differ or if local version is missing
                *notification = Some("New Update, Flash Recommended".to_string());
            } else {
                // Clear notification if versions match
                *notification = None;
            }
        } else {
            // Handle case where fetching latest version fails
            *notification = Some("Unable to check for updates".to_string());
        }
    }

    pub fn log_message(&self, message: &str) {
        let mut log = self.log.lock().unwrap();
        log.push(message.to_string());
        println!("{}", message); // Also print to console for debugging
    }

    pub fn update_ble_status(&self, new_status: &str) {
        let mut status = self.status.lock().unwrap();
        *status = new_status.to_string();
    }

    pub fn set_should_run(&self, should_run: bool) {
        let mut run_state = self.should_run.lock().unwrap();
        *run_state = should_run;
    }

    pub fn should_run(&self) -> bool {
        *self.should_run.lock().unwrap()
    }

    pub fn send_udp(&self, data: &[u8]) -> Result<(), std::io::Error> {
        self.udp_socket.send(data)
            .map(|_| ())
    }

    pub fn clear_log(&self) {
        let mut log = self.log.lock().unwrap();
        log.clear();
    }
}