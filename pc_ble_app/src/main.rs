#![windows_subsystem = "windows"]
use std::net::UdpSocket;
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;
use btleplug::api::{Central, Manager as _, Peripheral, ScanFilter};
use btleplug::platform::Manager;
use std::os::windows::process::CommandExt;
use bytemuck::{Pod, Zeroable};
use eframe::egui;
use std::fs;
use serde_json;
use zip::ZipArchive;
use std::io::Cursor;
use std::path::{PathBuf, Path};
use std::process::Command;
use std::io::BufRead;
use tokio::runtime::Runtime;
use tokio::time::timeout;
use uuid::Uuid;

const DEVICE_NAME: &str = "Nano 33 Head Tracker";
const SERVICE_UUID: &str = "19b10000-e8f2-537e-4f6c-d104768a1214";
const CHAR_UUID: &str = "19b10001-e8f2-537e-4f6c-d104768a1214";
const UDP_IP: &str = "127.0.0.1";
const UDP_PORT: u16 = 4242;
const CONNECTION_TIMEOUT: Duration = Duration::from_secs(5);
const RECONNECT_DELAY: Duration = Duration::from_secs(2);

#[repr(C, packed)]
#[derive(Copy, Clone, Pod, Zeroable)]
struct HatireData {
    header1: u16,
    header2: u16,
    yaw: f32,
    pitch: f32,
    roll: f32,
    accel_x: f32,
    accel_y: f32,
    accel_z: f32,
    footer: u16,
}

struct AppState {
    log: Arc<Mutex<Vec<String>>>,
    status: Arc<Mutex<String>>,
    udp_socket: UdpSocket,
    should_run: Arc<Mutex<bool>>,
}

impl AppState {
    fn new() -> Self {
        let udp_socket = UdpSocket::bind("0.0.0.0:0").expect("Failed to bind UDP socket");
        udp_socket.connect((UDP_IP, UDP_PORT)).expect("Failed to connect to OpenTrack");
        Self {
            log: Arc::new(Mutex::new(Vec::new())),
            status: Arc::new(Mutex::new("Off".to_string())),
            udp_socket,
            should_run: Arc::new(Mutex::new(false)),
        }
    }

    fn log_message(&self, message: &str) {
        let mut log = self.log.lock().unwrap();
        log.push(message.to_string());
        println!("{}", message); // Also print to console for debugging
    }

    fn update_status(&self, new_status: &str) {
        let mut status = self.status.lock().unwrap();
        *status = new_status.to_string();
    }

    fn set_should_run(&self, should_run: bool) {
        let mut run_state = self.should_run.lock().unwrap();
        *run_state = should_run;
    }

    fn should_run(&self) -> bool {
        *self.should_run.lock().unwrap()
    }

    fn send_udp(&self, data: &[u8]) -> Result<(), std::io::Error> {
        self.udp_socket.send(data)
            .map(|_| ())
    }

    pub fn clear_log(&self) {
        let mut log = self.log.lock().unwrap();
        log.clear();
    }
}

async fn handle_ble_connection(
    peripheral: &impl Peripheral,
    state: &Arc<AppState>,
) -> Result<(), Box<dyn std::error::Error>> {
    state.log_message("Attempting to connect to device...");
    
    match timeout(CONNECTION_TIMEOUT, peripheral.connect()).await {
        Ok(connect_result) => {
            connect_result?;
        }
        Err(_) => {
            state.log_message("Connection attempt timed out");
            return Err("Connection timeout".into());
        }
    }

    if peripheral.is_connected().await? {
        state.update_status("Connected");
        state.log_message("Device connected. Discovering services...");

        peripheral.discover_services().await?;
        let services = peripheral.services();
        let service_uuid = Uuid::parse_str(SERVICE_UUID)?;

        let service = services.iter()
            .find(|s| s.uuid == service_uuid)
            .ok_or("Service not found")?;

        let char_uuid = Uuid::parse_str(CHAR_UUID)?;
        let characteristic = service.characteristics.iter()
            .find(|c| c.uuid == char_uuid)
            .ok_or("Characteristic not found")?;

        use futures::stream::StreamExt;
        peripheral.subscribe(characteristic).await?;
        let mut notifications = peripheral.notifications().await?;
        
        state.log_message("Successfully connected and receiving data. Sending to OpenTrack...");
        while state.should_run() {
            match timeout(Duration::from_secs(1), notifications.next()).await {
                Ok(Some(notification)) => {
                    let data = &notification.value;
                    if data.len() == 32 {
                        let hatire_data = parse_data(data);

                        // Axis adjustment (mislabeled on the BLE device)
                        let yaw = hatire_data.yaw as f64;
                        let roll = hatire_data.roll as f64;
                        let pitch = hatire_data.pitch as f64;

                        // Construct OpenTrack-compatible data packet
                        let opentrack_data = [
                            0.0,    // X position (unused)
                            0.0,    // Y position (unused)
                            0.0,    // Z position (unused)
                            yaw,    // Yaw (rotation around vertical axis)
                            roll,   // Pitch (rotation around lateral axis)
                            pitch,  // Roll (rotation around longitudinal axis)
                        ];

                        // Convert to byte array and send via UDP
                        let bytes = unsafe {
                            std::slice::from_raw_parts(
                                opentrack_data.as_ptr() as *const u8,
                                std::mem::size_of_val(&opentrack_data),
                            )
                        };

                        if let Err(e) = state.send_udp(bytes) {
                            state.log_message(&format!("UDP send error: {:?}", e));
                        }
                    }
                }
                Ok(None) => {
                    state.log_message("Device disconnected");
                    break;
                }
                Err(_) => {
                    if !peripheral.is_connected().await? {
                        state.log_message("Connection lost");
                        break;
                    }
                }
            }
        }
    }
    
    // Cleanup on exit
    if peripheral.is_connected().await? {
        let _ = peripheral.disconnect().await;
        state.log_message("Device disconnected");
    }
    Ok(())
}

async fn run_ble_loop(state: Arc<AppState>) {
    let manager = Manager::new().await.unwrap();
    let adapters = manager.adapters().await.unwrap();
    
    if adapters.is_empty() {
        state.log_message("No BLE adapters found.");
        return;
    }
    
    let central = adapters.into_iter().next().unwrap();
    state.log_message(&format!("Using adapter: {:?}", central.adapter_info().await.unwrap()));

    while state.should_run() {
        state.update_status("Scanning...");
        state.log_message("Scanning for head tracker device...");
        
        // Create a scan filter that looks for our device name
        let filter = ScanFilter {
            services: vec![Uuid::parse_str(SERVICE_UUID).unwrap()],
            ..ScanFilter::default()
        };
        
        match central.start_scan(filter).await {
            Ok(_) => {
                thread::sleep(Duration::from_secs(2));
                
                if let Ok(peripherals) = central.peripherals().await {
                    for peripheral in peripherals {
                        if !state.should_run() {
                            break;
                        }

                        if let Ok(Some(properties)) = peripheral.properties().await {
                            if let Some(local_name) = &properties.local_name {
                                if local_name == DEVICE_NAME {
                                    state.log_message(&format!("Found device: {}", local_name));
                                    
                                    if let Err(e) = handle_ble_connection(&peripheral, &state).await {
                                        state.log_message(&format!("Connection error: {:?}", e));
                                    }
                                    
                                    if state.should_run() {
                                        state.log_message("Connection lost. Attempting to reconnect...");
                                    }
                                    // Wait before attempting to reconnect
                                    thread::sleep(RECONNECT_DELAY);
                                }
                            }
                        }
                    }
                }
            }
            Err(e) => {
                state.log_message(&format!("Scan error: {:?}", e));
                thread::sleep(RECONNECT_DELAY);
            }
        }
    }
    
    state.update_status("Off");
    state.log_message("Tracking stopped");
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let app_state = Arc::new(AppState::new());

    // BLE thread
    let state_clone = app_state.clone();
    thread::spawn(move || {
        let rt = Runtime::new().unwrap();
        rt.block_on(async move {
            loop {
                if state_clone.should_run() {
                    run_ble_loop(state_clone.clone()).await;
                }
                thread::sleep(Duration::from_secs(1));
            }
        });
    });

    // UI thread
    eframe::run_native(
        "Nano 33 Head Tracker",
        eframe::NativeOptions::default(),
        Box::new(|_| Box::new(MyApp::new(app_state))),
    )
    .unwrap();

    Ok(())
}

struct MyApp {
    app_state: Arc<AppState>,
}

impl MyApp {
    fn new(app_state: Arc<AppState>) -> Self {
        Self { app_state }
    }
}

impl eframe::App for MyApp {
    fn update(&mut self, ctx: &egui::Context, _: &mut eframe::Frame) {
        egui::CentralPanel::default().show(ctx, |ui| {
            // Horizontal layout for the buttons and indicators
            ui.horizontal(|ui| {
                // Fake LED indicator
                let is_usb = is_usb_connection();
                let is_ble = is_ble_connection(&self.app_state);
                let led_color = if is_usb || is_ble { egui::Color32::GREEN } else { egui::Color32::RED };
                ui.add(egui::Label::new(egui::RichText::new(" ").background_color(led_color).size(28.0)));
                ui.vertical(|ui| {
                    let connection_text = if is_usb || is_ble { "Connected" } else { "Disconnected" };
                    ui.label(egui::RichText::new(connection_text));
                    ui.horizontal(|ui| {
                        let usb_color = if is_usb {
                            egui::Color32::GREEN
                        } else {
                            egui::Color32::GRAY
                        };

                        let ble_color = if is_ble {
                            egui::Color32::from_rgb(135, 206, 250) // Softer, lighter blue
                        } else {
                            egui::Color32::GRAY
                        };

                        ui.label(egui::RichText::new("USB").color(usb_color).size(10.0)); // Adjusted font size
                        ui.add_space(2.0); // Reduced space between USB and BLE
                        ui.label(egui::RichText::new("BLE").color(ble_color).size(10.0));
                    });
                });

                // Start/Stop BLE Button
                let status = self.app_state.status.lock().unwrap().clone();
                let button_label = if status == "Off" { "Start BLE" } else { "Stop BLE" };
                if !is_usb {
                    if ui.add_sized([120.0, 30.0], egui::Button::new(button_label)).clicked() { // Slightly reduced height
                        let new_status = if status == "Off" {
                            self.app_state.clear_log();
                            self.app_state.set_should_run(true);
                            self.app_state.log_message("Starting head tracking...");
                            "Scanning..."
                        } else {
                            self.app_state.set_should_run(false);
                            self.app_state.log_message("Stopping head tracking...");
                            "Off"
                        };
                        self.app_state.update_status(new_status);
                    }
                } else {
                    if status != "Off" {
                        self.app_state.set_should_run(false);
                        self.app_state.log_message("Stopping BLE loop due to USB connection...");
                        self.app_state.update_status("Off");
                    }
                    ui.add_sized([120.0, 30.0], egui::Button::new(button_label).sense(egui::Sense::hover()));
                }

                // Flash Arduino Button
                if !is_usb {
                    ui.add_sized([120.0, 30.0], egui::Button::new("Flash Arduino").sense(egui::Sense::hover()));
                } else if ui.add_sized([120.0, 30.0], egui::Button::new("Flash Arduino")).clicked() {
                    self.app_state.clear_log();
                    let log_clone = self.app_state.log.clone();
                    flash_arduino(log_clone);
                }
            });

            ui.separator();

            ui.label("Log Output:");
            egui::ScrollArea::vertical()
                .auto_shrink([false; 2])
                .stick_to_bottom(true) // Automatically scroll to the bottom when new logs are added
                .show(ui, |ui| {
                    let log = self.app_state.log.lock().unwrap();
                    let log_text = log.join("\n");
            
                    let colored_text = egui::RichText::new(log_text.clone())
                        .monospace()
                        .color(egui::Color32::from_gray(200)); // Adjust the brightness (0-255)
            
                    ui.add(
                        egui::TextEdit::multiline(&mut colored_text.text().to_owned())
                            .font(egui::TextStyle::Monospace) // Use a monospace font
                            .code_editor()                   // Enables text selection and copy-paste
                            .frame(false)                    // Removes the frame for a cleaner look
                            .desired_width(f32::INFINITY)    // Expands to full width
                            .desired_rows(10),               // Sets the number of visible rows
                    );
                });            
        });

        ctx.request_repaint();
    }
}

fn is_usb_connection() -> bool {
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

fn is_ble_connection(app_state: &AppState) -> bool {
    let status = app_state.status.lock().unwrap().clone();
    status == "Connected"
}

fn check_existing_python() -> Option<PathBuf> {
    let python_dir = get_app_directory().join("python");
    let python_exe = python_dir.join("python.exe");
    if python_exe.exists() && python_dir.join("Lib").exists() {
        Some(python_exe)
    } else {
        None
    }
}

fn check_existing_pip(python_exe: &Path) -> bool {
    let output = Command::new(python_exe)
        .arg("-m")
        .arg("pip")
        .arg("--version")
        .stdout(std::process::Stdio::null())
        .stderr(std::process::Stdio::null())
        .creation_flags(0x08000000)
        .status();
    output.map_or(false, |status| status.success())
}

fn check_existing_platformio(python_exe: &Path) -> bool {
    let output = Command::new(python_exe)
        .arg("-m")
        .arg("pip")
        .arg("show")
        .arg("platformio")
        .stdout(std::process::Stdio::null())
        .stderr(std::process::Stdio::null())
        .creation_flags(0x08000000)
        .status();
    output.map_or(false, |status| status.success())
}

fn download_and_setup_python(log: Arc<Mutex<Vec<String>>>) -> Result<PathBuf, String> {
    let python_dir = get_app_directory().join("python");
    let python_exe = python_dir.join("python.exe");

    // If Python is already set up, return early
    if python_exe.exists() && python_dir.join("Lib").exists() {
        return Ok(python_exe);
    }

    // Clean up any partial installation
    if python_dir.exists() {
        fs::remove_dir_all(&python_dir).map_err(|e| format!("Failed to clean up Python directory: {}", e))?;
    }

    // Create fresh Python directory
    fs::create_dir_all(&python_dir).map_err(|e| format!("Failed to create Python directory: {}", e))?;

    // Download and extract main Python distribution
    let python_url = "https://www.python.org/ftp/python/3.11.5/python-3.11.5-embed-amd64.zip";
    log.lock().unwrap().push("Downloading Python...".to_string());
    let response = reqwest::blocking::get(python_url).map_err(|e| format!("Failed to download Python: {}", e))?;
    let mut archive = ZipArchive::new(Cursor::new(response.bytes().map_err(|e| e.to_string())?))
        .map_err(|e| format!("Failed to parse Python ZIP archive: {}", e))?;

    // Extract main Python distribution
    for i in 0..archive.len() {
        let mut file = archive.by_index(i).map_err(|e| format!("Failed to extract file from ZIP: {}", e))?;
        let out_path = python_dir.join(file.mangled_name());

        if file.is_dir() {
            fs::create_dir_all(&out_path).map_err(|e| format!("Failed to create directory: {}", e))?;
        } else {
            if let Some(parent) = out_path.parent() {
                fs::create_dir_all(parent).map_err(|e| format!("Failed to create parent directory: {}", e))?;
            }
            let mut outfile = fs::File::create(&out_path).map_err(|e| format!("Failed to create file: {}", e))?;
            std::io::copy(&mut file, &mut outfile).map_err(|e| format!("Failed to write file: {}", e))?;
        }
    }

    // Download and extract standard library
    let stdlib_url = "https://www.python.org/ftp/python/3.11.5/python-3.11.5-amd64.exe";
    let response = reqwest::blocking::get(stdlib_url).map_err(|e| format!("Failed to download stdlib: {}", e))?;
    
    // Save the exe temporarily
    let temp_exe = python_dir.join("python_installer.exe");
    fs::write(&temp_exe, response.bytes().map_err(|e| e.to_string())?)
        .map_err(|e| format!("Failed to save installer: {}", e))?;

    // Extract the exe (it's actually a self-extracting archive)
    let output = Command::new(&temp_exe)
        .args(["/quiet", "/layout", python_dir.to_str().unwrap()])
        .creation_flags(0x08000000)  // CREATE_NO_WINDOW
        .output()
        .map_err(|e| format!("Failed to extract stdlib: {}", e))?;

    if !output.status.success() {
        return Err(format!("Failed to extract stdlib: {:?}", output));
    }

    // Clean up installer
    let _ = fs::remove_file(temp_exe);

    // Create/modify python3XX._pth file
    let pth_file_path = python_dir.join("python311._pth");
    let pth_contents = "python311.zip\n.\nLib/site-packages\nimport site";
    fs::write(&pth_file_path, pth_contents).map_err(|e| format!("Failed to write _pth file: {}", e))?;

    // Create empty Lib/site-packages directory if it doesn't exist
    fs::create_dir_all(python_dir.join("Lib").join("site-packages"))
        .map_err(|e| format!("Failed to create site-packages directory: {}", e))?;

    Ok(python_exe)
}

fn bootstrap_pip(python_exe: &Path, log: Arc<Mutex<Vec<String>>>) -> Result<(), String> {
    let get_pip_url = "https://bootstrap.pypa.io/get-pip.py";
    let get_pip_script = python_exe.parent().unwrap().join("get-pip.py");

    log.lock().unwrap().push("Bootstrapping pip...".to_string());

    // Download get-pip.py
    log.lock().unwrap().push("Downloading get-pip.py...".to_string());
    let response = reqwest::blocking::get(get_pip_url)
        .map_err(|e| format!("Failed to download get-pip.py: {}", e))?;
    
    fs::write(&get_pip_script, response.bytes().map_err(|e| e.to_string())?)
        .map_err(|e| format!("Failed to write get-pip.py: {}", e))?;

    // Run get-pip.py
    log.lock().unwrap().push("Executing get-pip.py...".to_string());
    let output = Command::new(python_exe)
        .arg(&get_pip_script)
        .arg("--no-warn-script-location")
        .stdout(std::process::Stdio::piped())
        .stderr(std::process::Stdio::piped())
        .creation_flags(0x08000000)
        .output()
        .map_err(|e| format!("Failed to execute get-pip.py: {:?}", e))?;

    // Clean up get-pip.py regardless of success
    let _ = fs::remove_file(&get_pip_script);

    if output.status.success() {
        log.lock().unwrap().push("Pip install files cleaned up.".to_string());
        Ok(())
    } else {
        let stderr = String::from_utf8_lossy(&output.stderr);
        log.lock().unwrap().push(format!("Failed to bootstrap pip:\n{}", stderr));
        Err("Failed to bootstrap pip.".to_string())
    }
}

fn install_platformio(log: Arc<Mutex<Vec<String>>>) -> Result<PathBuf, String> {
    let python_exe = download_and_setup_python(log.clone())?;

    if check_existing_platformio(&python_exe) {
        log.lock().unwrap().push("PlatformIO is already installed.".to_string());
        return Ok(python_exe);
    }

    log.lock().unwrap().push("Installing PlatformIO...".to_string());

    let output = Command::new(&python_exe)
        .arg("-m")
        .arg("pip")
        .arg("install")
        .arg("platformio")
        .stdout(std::process::Stdio::piped())
        .stderr(std::process::Stdio::piped())
        .creation_flags(0x08000000)
        .output()
        .map_err(|e| format!("Failed to execute pip: {:?}", e))?;

    if output.status.success() {
        log.lock().unwrap().push("PlatformIO installed successfully.".to_string());
    } else {
        let stderr = String::from_utf8_lossy(&output.stderr);
        log.lock().unwrap().push(format!("PlatformIO installation failed:\n{}", stderr));
        return Err("Failed to install PlatformIO".to_string());
    }

    Ok(python_exe)
}

fn find_project_dir(base_dir: &Path) -> Option<PathBuf> {
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

fn save_local_version(version: &str) -> Result<(), String> {
    let version_file = get_project_directory().join("PlatformIO_AppVersion.txt");
    fs::write(version_file, version).map_err(|e| format!("Failed to write version file: {}", e))
}

fn read_local_version() -> Option<String> {
    let version_file = get_project_directory().join("PlatformIO_AppVersion.txt");
    fs::read_to_string(version_file).ok()
}

fn fetch_latest_commit(repo_url: &str) -> Result<String, String> {
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

fn prepare_platformio_project(log: Arc<Mutex<Vec<String>>>) -> Result<PathBuf, String> {
    let base_project_dir = get_project_directory();
    let repo_url = "FugLong/Nano33_PC_Head_Tracker";
    
    // Compare latest and local versions if local exists
    //Download if necessary
    let latest_version = fetch_latest_commit(repo_url).map_err(|e| {
        log.lock().unwrap().push(format!("Failed to fetch latest project version: {}", e));
        e
    })?;
    
    let local_version = read_local_version();
    
    if local_version.as_deref() == Some(&latest_version) {
        log.lock().unwrap().push("Project is up-to-date.".to_string());
        return find_project_dir(&get_project_directory())
            .ok_or("Failed to locate cached project".to_string());
    }
    
    log.lock().unwrap().push(format!("Updating to version: {}...", latest_version));

    // Clean up any existing partial project
    if base_project_dir.exists() {
        fs::remove_dir_all(&base_project_dir).map_err(|e| format!("Failed to clean up project directory: {}", e))?;
    }

    fs::create_dir_all(&base_project_dir).map_err(|e| format!("Failed to create project directory: {}", e))?;

    let repo_url = "https://github.com/FugLong/Nano33_PC_Head_Tracker/archive/refs/heads/V2_Update.zip";
    log.lock().unwrap().push(format!("Downloading from: {}", repo_url));
    
    let response = reqwest::blocking::get(repo_url).map_err(|e| format!("Failed to download project: {}", e))?;
    
    if !response.status().is_success() {
        return Err(format!("Failed to download project: HTTP {}", response.status()));
    }

    let bytes = response.bytes().map_err(|e| format!("Failed to get response bytes: {}", e))?;
    log.lock().unwrap().push(format!("Downloaded {} bytes", bytes.len()));

    let mut archive = ZipArchive::new(Cursor::new(bytes))
        .map_err(|e| format!("Failed to parse ZIP archive: {}", e))?;

    log.lock().unwrap().push(format!("ZIP archive contains {} files", archive.len()));

    // Extract files
    for i in 0..archive.len() {
        let mut file = archive.by_index(i).map_err(|e| format!("Failed to extract file from ZIP: {}", e))?;
        
        let file_name = file.name().to_string(); // Immutable borrow ends here
        let out_path = base_project_dir.join(&file_name);
        
        //log.lock().unwrap().push(format!("Extracting: {}", file_name));

        if file.is_dir() {
            fs::create_dir_all(&out_path).map_err(|e| format!("Failed to create directory {}: {}", file_name, e))?;
        } else {
            if let Some(parent) = out_path.parent() {
                fs::create_dir_all(parent).map_err(|e| format!("Failed to create parent directory for {}: {}", file_name, e))?;
            }
            let mut outfile = fs::File::create(&out_path)
                .map_err(|e| format!("Failed to create file {}: {}", file_name, e))?;
            std::io::copy(&mut file, &mut outfile)
                .map_err(|e| format!("Failed to write file {}: {}", file_name, e))?;
        }
    }

    // Verify the extraction using `find_project_dir`
    if let Some(project_dir) = find_project_dir(&base_project_dir) {
        log.lock().unwrap().push(format!("Project directory verified successfully: {:?}", project_dir));
        save_local_version(&latest_version).map_err(|e| {
            log.lock().unwrap().push(format!("Failed to save local version: {}", e));
            e
        })?;
        Ok(project_dir)
    } else {
        // Debugging: List contents of the base directory
        log.lock().unwrap().push("Failed to find project directory after extraction. Contents:".to_string());
        if let Ok(entries) = fs::read_dir(&base_project_dir) {
            for entry in entries {
                if let Ok(entry) = entry {
                    log.lock().unwrap().push(format!("  {:?}", entry.path()));
                }
            }
        }
        Err("Failed to find valid project directory after extraction.".to_string())
    }
}

fn flash_arduino(log: Arc<Mutex<Vec<String>>>) {
    let log_clone = log.clone();

    thread::spawn(move || {
        log_clone.lock().unwrap().push("Starting Arduino flashing process...".to_string());

        let python_exe = match check_existing_python() {
            Some(python) => {
                log_clone.lock().unwrap().push("Python already installed.".to_string());
                python
            },
            None => match download_and_setup_python(log.clone()) {
                Ok(python) => {
                    log_clone.lock().unwrap().push("Python installed successfully.".to_string());
                    python
                },
                Err(e) => {
                    log_clone.lock().unwrap().push(format!("Error setting up Python: {}", e));
                    return;
                }
            },
        };

        if !check_existing_pip(&python_exe) {
            if let Err(e) = bootstrap_pip(&python_exe, log_clone.clone()) {
                log_clone.lock().unwrap().push(format!("Error setting up pip: {}", e));
                return;
            } else {
                log_clone.lock().unwrap().push("Pip installed successfully.".to_string());
            }
        } else {
            log_clone.lock().unwrap().push("Pip already installed.".to_string());
        }

        if !check_existing_platformio(&python_exe) {
            if let Err(e) = install_platformio(log_clone.clone()) {
                log_clone.lock().unwrap().push(format!("Error installing PlatformIO: {}", e));
                return;
            } else {
                log_clone.lock().unwrap().push("PlatformIO installed successfully.".to_string());
            }
        } else {
            log_clone.lock().unwrap().push("PlatformIO already installed.".to_string());
        }

        match prepare_platformio_project(log_clone.clone()) {
            Ok(project_dir) => {
                log_clone.lock().unwrap().push("Flashing Arduino...".to_string());

                let output = Command::new(&python_exe)
                    .current_dir(&project_dir)
                    .arg("-m")
                    .arg("platformio")
                    .arg("run")
                    .arg("--target")
                    .arg("upload")
                    .creation_flags(0x08000000) // CREATE_NO_WINDOW
                    .stdout(std::process::Stdio::piped())
                    .stderr(std::process::Stdio::piped())
                    .spawn();

                match output {
                    Ok(mut child) => {
                        if let Some(stdout) = child.stdout.take() {
                            let reader = std::io::BufReader::new(stdout);
                            for line in reader.lines() {
                                if let Ok(line) = line {
                                    if !line.contains("warning:") && !line.contains("-W") {
                                        log_clone.lock().unwrap().push(line.clone());
                                    }
                                }
                            }
                        }

                        if let Ok(status) = child.wait() {
                            if status.success() {
                                log_clone.lock().unwrap().push("Arduino flashed successfully.".to_string());
                            } else {
                                log_clone.lock().unwrap().push("Flash failed. Check log for details.".to_string());
                            }
                        }
                    }
                    Err(e) => {
                        log_clone.lock().unwrap().push(format!(
                            "Failed to execute platformio: {:?}\nPython path: {:?}\nProject dir: {:?}",
                            e, python_exe, project_dir
                        ));
                    }
                }
            }
            Err(e) => {
                log_clone.lock().unwrap().push(format!("Error preparing PlatformIO project: {}", e));
            }
        }
    });
}

fn get_app_directory() -> PathBuf {
    std::env::current_exe()
        .map(|exe_path| exe_path.parent().unwrap().to_path_buf())
        .unwrap_or_else(|_| PathBuf::from("."))
}

fn get_project_directory() -> PathBuf {
    get_app_directory().join("platformio_project")
}

fn parse_data(data: &[u8]) -> HatireData {
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