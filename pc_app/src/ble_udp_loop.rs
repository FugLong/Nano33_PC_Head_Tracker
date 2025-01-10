use crate::app_state::AppState;
use btleplug::api::{Peripheral, ScanFilter, Central};
use btleplug::api::Manager as ManagerTrait;
use btleplug::platform::Manager;
use tokio::time::{timeout, Duration};
use std::sync::Arc;
use std::thread;
use uuid::Uuid;
use crate::utilities::{
                        SERVICE_UUID,
                        CONNECTION_TIMEOUT,
                        CHAR_UUID,
                        parse_data,
                        DEVICE_NAME,
                        RECONNECT_DELAY,
};

pub async fn handle_ble_connection(
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
        state.update_ble_status("Connected");
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

pub async fn run_ble_loop(state: Arc<AppState>) {
    let manager = Manager::new().await.unwrap();
    let adapters = manager.adapters().await.unwrap();
    
    if adapters.is_empty() {
        state.log_message("No BLE adapters found.");
        return;
    }
    
    let central = adapters.into_iter().next().unwrap();
    state.log_message(&format!("Using adapter: {:?}", central.adapter_info().await.unwrap()));

    while state.should_run() {
        state.update_ble_status("Scanning...");
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
    
    state.update_ble_status("Off");
    state.log_message("Tracking stopped");
}