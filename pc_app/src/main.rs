mod app_state;
mod ble_udp_loop;
mod flashing;
mod ui;
mod utilities;

use crate::app_state::AppState;
use crate::ble_udp_loop::run_ble_loop;

use std::sync::Arc;
use std::thread;
use std::time::Duration;
use tokio::runtime::Runtime;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Initialize shared application state
    let app_state = Arc::new(AppState::new());

    // Check for updates
    app_state.update_notification();

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

    // Start the UI
    ui::start_ui(app_state);

    Ok(())
}
