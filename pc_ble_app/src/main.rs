#![windows_subsystem = "windows"]
use std::net::UdpSocket;
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;
use btleplug::api::{Central, Manager as _, Peripheral, ScanFilter};
use btleplug::platform::Manager;
use bytemuck::{Pod, Zeroable};
use eframe::egui;
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
                    self.app_state.log_message("Flashing Arduino...");
                    flash_arduino();
                }
            });

            ui.separator();

            // Log Section
            egui::ScrollArea::vertical()
                .auto_shrink([false; 2])
                .stick_to_bottom(true) // Automatically stick to the bottom when new logs are added
                .show(ui, |ui| {
                    let log = self.app_state.log.lock().unwrap();

                    for entry in log.iter() {
                        ui.label(entry);
                    }
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

fn flash_arduino() {
    use std::process::Command;

    // Example command to run PlatformIO CLI for flashing
    let output = Command::new("pio")
        .arg("run")
        .arg("--target")
        .arg("upload")
        .output();

    match output {
        Ok(output) => {
            if output.status.success() {
                println!("Arduino flashed successfully");
            } else {
                eprintln!("Failed to flash Arduino: {:?}", output.stderr);
            }
        }
        Err(e) => {
            eprintln!("Error running PlatformIO CLI: {}", e);
        }
    }
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