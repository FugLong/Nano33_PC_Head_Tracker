import sys
import asyncio
import socket
from struct import unpack, pack
from bleak import BleakScanner, BleakClient
from PyQt5.QtWidgets import QApplication, QVBoxLayout, QPushButton, QTextEdit, QWidget, QMessageBox
from PyQt5.QtCore import QThread, pyqtSignal
import psutil
import os

p = psutil.Process(os.getpid())
p.nice(psutil.HIGH_PRIORITY_CLASS)

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

class BLEWorker(QThread):
    log_signal = pyqtSignal(str)
    status_signal = pyqtSignal(str)

    def __init__(self):
        super().__init__()
        self.running = False
        self.stopped = False
        self.udp_socket = None

    async def discover_device(self):
        self.log_signal.emit("Scanning for BLE devices...")
        while self.running:
            if self.stopped:
                self.log_signal.emit("BLE loop stopped by user.")
                return
            devices = await BleakScanner.discover()
            for device in devices:
                if device.name == DEVICE_NAME:
                    self.log_signal.emit(f"Found device: {device.name} ({device.address})")
                    return device.address
            self.log_signal.emit(f"No devices found with name '{DEVICE_NAME}'. Retrying...")
            await asyncio.sleep(1)

    async def handle_ble_notifications(self, client):
        self.udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        server_address = (UDP_IP, UDP_PORT)

        async def notification_handler(sender, data):
            try:
                if len(data) != 32:
                    self.log_signal.emit(f"[ERROR] Unexpected data size: {len(data)} bytes (expected 32 bytes).")
                    return

                meaningful_data = data[:HAT_STRUCT_SIZE]
                unpacked_data = unpack(HAT_STRUCT_FORMAT, meaningful_data)
                yaw, roll, pitch = unpacked_data[2:5]

                opentrack_data = pack('<6d', 0.0, 0.0, 0.0, yaw, pitch, roll)
                self.udp_socket.sendto(opentrack_data, server_address)
            except Exception as e:
                self.log_signal.emit(f"[ERROR] Notification handler exception: {e}")

        self.log_signal.emit("Subscribing to BLE notifications...")
        try:
            await client.start_notify(CHARACTERISTIC_UUID, notification_handler)
            self.log_signal.emit("Successfully receiving data...")

            while client.is_connected and not self.stopped:
                await asyncio.sleep(0.5)
        except asyncio.CancelledError:
            self.log_signal.emit("[INFO] Notification loop cancelled.")
        except Exception as e:
            self.log_signal.emit(f"[ERROR] Exception while handling BLE notifications: {e}")
        finally:
            try:
                if client.is_connected:
                    await client.stop_notify(CHARACTERISTIC_UUID)
            except Exception as e:
                self.log_signal.emit(f"[ERROR] Failed to stop notifications cleanly: {e}")
            self.udp_socket.close()

    async def ble_loop(self):
        while self.running and not self.stopped:
            device_address = await self.discover_device()
            if self.stopped:
                self.log_signal.emit("Discovery process stopped by user.")
                break
            if device_address is None:
                continue
            try:
                async with BleakClient(device_address) as client:
                    if self.stopped:
                        self.log_signal.emit("BLE connection process stopped by user.")
                        break
                    if client.is_connected:
                        self.log_signal.emit(f"Connected to {DEVICE_NAME} at {device_address}.")
                        self.status_signal.emit("connected")
                        await self.handle_ble_notifications(client)
                    else:
                        self.log_signal.emit("Connection lost.")
            except Exception as e:
                if not self.stopped:
                    self.log_signal.emit(f"[ERROR] BLE connection error: {e}. Retrying...")
                    self.status_signal.emit("searching")
            finally:
                if not self.stopped:
                    self.log_signal.emit("Connection lost or disconnected. Switching to searching state...")
                    self.status_signal.emit("searching")
                    await asyncio.sleep(1)

    def run(self):
        self.running = True
        self.stopped = False
        self.log_signal.emit("BLE worker started.")
        asyncio.run(self.ble_loop())

    def stop(self):
        if not self.stopped:
            self.stopped = True
            self.running = False
            self.log_signal.emit("BLE worker stopped.")
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

        layout = QVBoxLayout()
        self.log_output = QTextEdit()
        self.log_output.setReadOnly(True)
        layout.addWidget(self.log_output)

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
        self.update_log("Stopping BLE worker and resetting to idle state.")
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
