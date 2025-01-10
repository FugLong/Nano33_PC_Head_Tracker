use crate::app_state::AppState;
use std::sync::Arc;
use eframe::egui;
use crate::flashing::flash_arduino;
use crate::utilities::{
                        is_usb_connection,
                        is_ble_connection,
};

pub fn start_ui(app_state: Arc<AppState>) {
    eframe::run_native(
        "Nano 33 Head Tracker",
        eframe::NativeOptions::default(),
        Box::new(|_| Box::new(MyApp::new(app_state))),
    )
    .unwrap();
}

struct MyApp {
    app_state: Arc<AppState>,
}

impl MyApp {
    fn new(app_state: Arc<AppState>) -> Self {
        Self { 
            app_state,
        }
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
                        self.app_state.update_ble_status(new_status);
                    }
                } else {
                    if status != "Off" {
                        self.app_state.set_should_run(false);
                        self.app_state.log_message("Stopping BLE loop due to USB connection...");
                        self.app_state.update_ble_status("Off");
                    }
                    ui.add_sized([120.0, 30.0], egui::Button::new(button_label).sense(egui::Sense::hover()));
                }

                // Flash Arduino Button
                if !is_usb {
                    ui.add_sized(
                        [120.0, 30.0],
                        egui::Button::new("Flash Arduino").sense(egui::Sense::hover())
                    );
                } else {
                    let is_flashing = self.app_state.is_flashing();
                    let button = egui::Button::new("Flash Arduino")
                        .sense(if is_flashing {
                            egui::Sense::hover() // Button only hovers when disabled
                        } else {
                            egui::Sense::click() // Button fully interactive when enabled
                        });
                
                    if ui.add_sized([120.0, 30.0], button).clicked() {
                        self.app_state.clear_log();
                        flash_arduino(&self.app_state);
                    }
                }

                // Notification bubble
                if let Some(message) = &*self.app_state.notification_message.lock().unwrap() {
                    ui.add_space(2.0); // Add space between the buttons and the notification
                    ui.colored_label(
                        egui::Color32::from_rgb(160, 160, 160), // Subtle gray color
                        egui::RichText::new(message).size(12.0)
                    );
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