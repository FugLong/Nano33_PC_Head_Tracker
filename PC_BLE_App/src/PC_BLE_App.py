import asyncio
import socket
from struct import unpack, pack
from bleak import BleakScanner, BleakClient

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

async def discover_device():
    """Discover BLE devices and return the address of the target device."""
    print("Scanning for BLE devices...")
    while True:
        devices = await BleakScanner.discover()
        for device in devices:
            if device.name == DEVICE_NAME:
                print(f"Found device: {device.name} ({device.address})")
                return device.address
        print(f"No devices found with name '{DEVICE_NAME}'. Retrying...")
        await asyncio.sleep(2)

async def handle_ble_notifications(client):
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
                print(f"[ERROR] Unexpected data size: {len(data)} bytes (expected 32 bytes).")
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
            print(f"[ERROR] Notification handler exception: {e}")

    print("Subscribing to BLE notifications...")
    try:
        await client.start_notify(CHARACTERISTIC_UUID, notification_handler)
        print("Successfully Receiving Data...")

        while client.is_connected:
            try:
                await asyncio.wait_for(client.read_gatt_char(CHARACTERISTIC_UUID), timeout=3)
            except asyncio.TimeoutError:
                print("[ERROR] BLE device unresponsive. Reconnecting...")
                break
            except asyncio.CancelledError:
                print("[INFO] Notification loop cancelled.")
                break
            except Exception as e:
                print(f"[ERROR] Exception while reading characteristic: {e}")
                break
            await asyncio.sleep(1)
    except asyncio.CancelledError:
        print("[INFO] Notification handling cancelled.")
    except Exception as e:
        print(f"[ERROR] Failed to subscribe to notifications: {e}")
    finally:
        try:
            if client.is_connected:
                await client.stop_notify(CHARACTERISTIC_UUID)
        except asyncio.CancelledError:
            print("[INFO] Notification stop cancelled.")
        except Exception as e:
            if "closed" in str(e).lower():
                print("[INFO] Notification resources already closed.")
            else:
                print(f"[ERROR] Failed to stop notifications cleanly: {e}")
        finally:
            sock.close()
            print("Stopped BLE notifications.")

async def connect_to_device():
    """Connect to the BLE device and handle notifications."""
    while True:
        device_address = await discover_device()
        try:
            async with BleakClient(device_address) as client:
                if client.is_connected:
                    print(f"Connected to {DEVICE_NAME} at {device_address}.")
                    await handle_ble_notifications(client)
                else:
                    print(f"Failed to connect to {DEVICE_NAME}. Retrying...")
        except asyncio.CancelledError:
            print("[INFO] Connection process cancelled.")
            break
        except OSError as e:
            if "closed" in str(e).lower():
                print("[INFO] BLE resource was already closed.")
            else:
                print(f"[ERROR] OS-level issue occurred: {e}. Retrying...")
        except Exception as e:
            print(f"[ERROR] An error occurred: {e}. Retrying...")
        finally:
            print("Disconnected from device. Restarting connection process...")
            await asyncio.sleep(2)

if __name__ == "__main__":
    try:
        asyncio.run(connect_to_device())
    except KeyboardInterrupt:
        print("Program terminated by user.")
