#!/usr/bin/env python3
import asyncio
from bleak import BleakScanner, BleakClient
import sys

DEVICE_NAME = "ModeWifi"
SERVICE_UUID = "180F"  # Battery Service
CHARACTERISTIC_UUID = "2A19"  # Battery Level Characteristic

async def scan_for_device():
    print(f"Scanning for {DEVICE_NAME}...")
    devices = await BleakScanner.discover()
    for d in devices:
        if d.name and DEVICE_NAME in d.name:
            print(f"Found device: {d.name} ({d.address})")
            return d
    print("Device not found!")
    return None

async def connect_and_test(device):
    print(f"\nConnecting to {device.name}...")
    async with BleakClient(device.address) as client:
        print("Connected!")
        
        # Subscribe to notifications
        def notification_handler(sender, data):
            battery_level = int.from_bytes(data, byteorder='little')
            print(f"Received battery level: {battery_level}%")
        
        await client.start_notify(CHARACTERISTIC_UUID, notification_handler)
        print("Subscribed to battery level notifications")
        
        # Wait for notifications
        print("\nWaiting for notifications (press Ctrl+C to stop)...")
        try:
            while True:
                await asyncio.sleep(1)
        except KeyboardInterrupt:
            print("\nStopping...")
            await client.stop_notify(CHARACTERISTIC_UUID)

async def main():
    device = await scan_for_device()
    if device:
        try:
            await connect_and_test(device)
        except Exception as e:
            print(f"Error: {e}")
    else:
        print("\nTroubleshooting steps:")
        print("1. Check if the ESP32 is powered (LED should be blue)")
        print("2. Verify the code is uploaded correctly")
        print("3. Check Serial Monitor for any error messages")
        print("4. Try a different USB port or powered USB hub")
        sys.exit(1)

if __name__ == "__main__":
    asyncio.run(main()) 