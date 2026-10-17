#!/usr/bin/env python3
"""
Sync current time to Hiking Gadget on boot.
Run IMMEDIATELY after uploading firmware (within 3 seconds).

Usage:
    python sync_time.py             # auto-detect port
    python sync_time.py COM3        # specify port (Windows)
    python sync_time.py /dev/ttyUSB0  # specify port (Linux)

Requires: pip install pyserial
"""
import sys
import time
import serial
import serial.tools.list_ports


def find_esp_port():
    ports = list(serial.tools.list_ports.comports())
    # Prefer ESP32 / CH340 / CP210x devices
    for p in ports:
        desc = (p.description + p.manufacturer if p.manufacturer else p.description).lower()
        if any(k in desc for k in ["cp210", "ch340", "ch341", "esp32", "silicon"]):
            return p.device
    # Fallback: first available port
    if ports:
        return ports[0].device
    return None


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else find_esp_port()
    if not port:
        print("ERROR: No serial port found. Specify port as argument.")
        sys.exit(1)

    ts = int(time.time())
    cmd = f"T{ts}\n".encode()

    print(f"Connecting to {port} ...")
    with serial.Serial(port, 115200, timeout=2) as s:
        time.sleep(0.5)   # wait for device to settle after connect
        s.write(cmd)
        s.flush()
        print(f"Sent: T{ts}  (UTC: {time.strftime('%Y-%m-%d %H:%M:%S', time.gmtime(ts))})")

    print("Done. Your gadget clock is now synced.")


if __name__ == "__main__":
    main()
