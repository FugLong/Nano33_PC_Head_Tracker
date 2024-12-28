import sys
import asyncio
import socket
from struct import unpack, pack
from bleak import BleakScanner, BleakClient
from PyQt5.QtWidgets import QApplication, QVBoxLayout, QPushButton, QTextEdit, QWidget
from PyQt5.QtCore import QThread, pyqtSignal

# BLE Configuration
DEVICE_NAME = "Nano 33 Head Tracker"
SERVICE_UUID = "19b10000-e8f2-537e-4f6c-d104768a1214"
CHARACTERISTIC_UUID = "19b10001-e8f2-537e-4f6c-d104768a1214"

# UDP Configuration
UDP_IP = "127.0.0.1"  # Localhost
UDP_PORT = 4242      # Default port for OpenTrack

# Hatire structure details
HAT_STRUCT_FORMAT = "<H H 3f 3f H"  # Matches 30 bytes of meaningful data
HAT_STRUCT_SIZE = 30  # Expected meaningful size

# Initialize smoothing values
yaw_smooth = None
pitch_smooth = None
roll_smooth = None

class BLEWorker(QThread):
    log_signal = pyqtSignal(str)
    status_signal = pyqtSignal(str)

    def __init__(self):
        super().__init__()
        self.running = False
        self.stopped = False

    async def discover_device(self):
        """Discover BLE devices and return the address of the target device."""
        self.log_signal.emit("Scanning for BLE devices...")
        while self.running and not self.stopped:
            devices = await BleakScanner.discover()
            for device in devices:
                if device.name == DEVICE_NAME:
                    self.log_signal.emit(f"Found device: {device.name} ({device.address})")
                    return device.address
            self.log_signal.emit(f"No devices found with name '{DEVICE_NAME}'. Retrying...")
            await asyncio.sleep(2)

    async def handle_ble_notifications(self, client):
        """Handle BLE notifications and forward data via UDP."""
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        server_address = (UDP_IP, UDP_PORT)

        def wrap_angle(angle):
            """Ensure angle stays within -180 to 180 degrees."""
            return (angle + 180) % 360 - 180

        async def notification_handler(sender, data):
            """Handle incoming BLE notifications."""
            try:
                global yaw_smooth, pitch_smooth, roll_smooth
                if len(data) != 32:
                    self.log_signal.emit(f"[ERROR] Unexpected data size: {len(data)} bytes (expected 32 bytes).")
                    return

                meaningful_data = data[:HAT_STRUCT_SIZE]
                unpacked_data = unpack(HAT_STRUCT_FORMAT, meaningful_data)
                yaw, roll, pitch = unpacked_data[2:5]

                x = y = z = 0.0
                roll = -roll
                pitch = -pitch

                smoothing_factor = .95
                if yaw_smooth is None:
                    yaw_smooth, pitch_smooth, roll_smooth = yaw, pitch, roll
                else:
                    yaw_diff = wrap_angle(yaw - yaw_smooth)
                    pitch_diff = wrap_angle(pitch - pitch_smooth)
                    roll_diff = wrap_angle(roll - roll_smooth)
                    yaw_smooth = wrap_angle(yaw_smooth + smoothing_factor * yaw_diff)
                    pitch_smooth = wrap_angle(pitch_smooth + smoothing_factor * pitch_diff)
                    roll_smooth = wrap_angle(roll_smooth + smoothing_factor * roll_diff)

                opentrack_data = pack('<6d', x, y, z, yaw_smooth, pitch_smooth, roll_smooth)
                sock.sendto(opentrack_data, server_address)
            except Exception as e:
                self.log_signal.emit(f"[ERROR] Notification handler exception: {e}")

        self.log_signal.emit("Subscribing to BLE notifications...")
        try:
            await client.start_notify(CHARACTERISTIC_UUID, notification_handler)
            self.log_signal.emit("Successfully Receiving Data...")

            while client.is_connected and not self.stopped:
                try:
                    await asyncio.wait_for(client.read_gatt_char(CHARACTERISTIC_UUID), timeout=3)
                except asyncio.TimeoutError:
                    self.log_signal.emit("[ERROR] BLE device unresponsive. Reconnecting...")
                    break
                except asyncio.CancelledError:
                    self.log_signal.emit("[INFO] Notification loop cancelled.")
                    break
                except Exception as e:
                    self.log_signal.emit(f"[ERROR] Exception while reading characteristic: {e}")
                    break
                await asyncio.sleep(1)
        except asyncio.CancelledError:
            self.log_signal.emit("[INFO] Notification handling cancelled.")
        except Exception as e:
            self.log_signal.emit(f"[ERROR] Failed to subscribe to notifications: {e}")
        finally:
            try:
                if client.is_connected:
                    await client.stop_notify(CHARACTERISTIC_UUID)
            except asyncio.CancelledError:
                self.log_signal.emit("[INFO] Notification stop cancelled.")
            except Exception as e:
                if "closed" in str(e).lower():
                    self.log_signal.emit("[INFO] Notification resources already closed.")
                else:
                    self.log_signal.emit(f"[ERROR] Failed to stop notifications cleanly: {e}")
            finally:
                sock.close()
                self.log_signal.emit("Stopped BLE notifications.")

    async def ble_loop(self):
        while self.running and not self.stopped:
            device_address = await self.discover_device()
            try:
                async with BleakClient(device_address) as client:
                    if client.is_connected:
                        self.log_signal.emit(f"Connected to {DEVICE_NAME} at {device_address}.")
                        self.status_signal.emit("connected")
                        await self.handle_ble_notifications(client)
                    else:
                        self.log_signal.emit(f"Failed to connect to {DEVICE_NAME}. Retrying...")
                        self.status_signal.emit("searching")
            except asyncio.CancelledError:
                self.log_signal.emit("[INFO] Connection process cancelled.")
                break
            except Exception as e:
                self.log_signal.emit(f"[ERROR] An error occurred: {e}. Retrying...")
            finally:
                if self.running and not self.stopped:
                    self.log_signal.emit("Disconnected from device. Restarting connection process...")
                    self.status_signal.emit("searching")
                    await asyncio.sleep(2)

    def run(self):
        self.running = True
        self.stopped = False
        asyncio.run(self.ble_loop())

    def stop(self):
        self.stopped = True
        self.running = False
        self.quit()

class HeadTrackerApp(QWidget):
    def __init__(self):
        super().__init__()
        self.init_ui()
        self.ble_worker = BLEWorker()
        self.ble_worker.log_signal.connect(self.update_log)
        self.ble_worker.status_signal.connect(self.update_status)

    def init_ui(self):
        self.setWindowTitle("Nano33 Head Tracker - PC BLE")
        self.resize(400, 300)

        # Layout and widgets
        layout = QVBoxLayout()

        # Log output
        self.log_output = QTextEdit()
        self.log_output.setReadOnly(True)
        layout.addWidget(self.log_output)

        # Button
        self.control_button = QPushButton("Start")
        self.control_button.setFixedHeight(80)
        self.control_button.setStyleSheet("background-color: lightcoral; font-size: 16px;")
        self.control_button.clicked.connect(self.toggle_ble)
        layout.addWidget(self.control_button)

        self.setLayout(layout)
        self.ble_status = "idle"

    def toggle_ble(self):
        if self.ble_status == "idle":
            self.start_ble()
        else:
            self.stop_ble()

    def start_ble(self):
        self.update_status("searching")
        self.ble_worker.start()

    def stop_ble(self):
        self.ble_worker.stop()
        self.update_status("idle")

    def update_log(self, message):
        self.log_output.append(message)

    def update_status(self, status):
        self.ble_status = status
        if status == "idle":
            self.control_button.setText("Start")
            self.control_button.setStyleSheet("background-color: lightcoral; font-size: 16px;")
        elif status == "searching":
            self.control_button.setText("Connecting...")
            self.control_button.setStyleSheet("background-color: lightyellow; font-size: 16px;")
        elif status == "connected":
            self.control_button.setText("Connected")
            self.control_button.setStyleSheet("background-color: lightgreen; font-size: 16px;")

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = HeadTrackerApp()
    window.show()
    sys.exit(app.exec_())
