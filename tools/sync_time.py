#!/usr/bin/env python3
"""Set the Hiking Gadget clock from this computer over USB (SPEC 5.6).

    py -m pip install pyserial          (once; on Linux: python3 -m pip install pyserial)
    py tools\\sync_time.py               (Windows)   python3 tools/sync_time.py   (Linux/macOS)
    options: --port COM5   --check (only measure the drift)   --tz MSK (also set the zone)
"""
import argparse
import re
import sys
import time
from datetime import datetime, timezone

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    print("pyserial is missing: run  py -m pip install pyserial  (Linux: python3 -m pip install pyserial)")
    sys.exit(2)

ESPRESSIF_VID = 0x303A
HINT = ("No answer. Press any key on the gadget first (with the screen off it may be asleep and invisible "
        "to the PC for up to ~35 s, or powered off). Port busy? Close the Arduino Serial Monitor (Windows "
        "allows one program per COM port). Is 'USB CDC On Boot' Enabled?")


def find_port(explicit):
    if explicit:
        return explicit
    ports = list(list_ports.comports())
    esp = [p for p in ports if p.vid == ESPRESSIF_VID]
    if esp:
        return esp[0].device
    if len(ports) == 1:
        return ports[0].device
    return None


def open_port(name):
    s = serial.Serial()
    s.port = name
    s.baudrate = 115200
    s.timeout = 0.05
    s.dtr = False          # set BEFORE open: a DTR/RTS sequence can reset the S3 over USB-Serial/JTAG
    s.rts = False
    s.open()
    return s


def read_lines(s, seconds):
    end = time.time() + seconds
    buf = b""
    while time.time() < end:
        buf += s.read(256)
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            yield line.decode("ascii", "replace").strip()


def wait_reply(s, prefixes, seconds=2.0):
    for line in read_lines(s, seconds):
        if line.startswith(prefixes):
            return line
    return None


def ping(s, seconds=10.0):
    end = time.time() + seconds
    while time.time() < end:
        s.write(b"PING\n")
        if wait_reply(s, ("PONG HG2",), 0.3):
            return True
    return False


def parse_device_utc(line):
    m = re.search(r"utc=(\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d\.\d{3})Z", line)
    if not m:
        return None
    return datetime.strptime(m.group(1), "%Y-%m-%dT%H:%M:%S.%f").replace(tzinfo=timezone.utc).timestamp()


def main():
    ap = argparse.ArgumentParser(description="Sync the Hiking Gadget clock over USB")
    ap.add_argument("--port", help="serial port, e.g. COM5 or /dev/ttyACM0 (default: autodetect)")
    ap.add_argument("--check", action="store_true", help="only report the offset (TIME?), do not set")
    ap.add_argument("--tz", help="also set the time zone id, e.g. MSK, CET, NYC (see TZ? on the device)")
    a = ap.parse_args()

    name = find_port(a.port)
    if not name:
        print("No serial port found. " + HINT)
        return 1
    try:
        s = open_port(name)
    except serial.SerialException as e:
        print(f"Cannot open {name}: {e}\n" + HINT)
        return 1
    with s:
        if not ping(s):
            print(f"{name}: " + HINT)
            return 1
        if a.check:
            t0 = time.time()
            s.write(b"TIME?\n")
            line = wait_reply(s, ("TIME utc=",))
            t1 = time.time()
            if not line:
                print("No reply to TIME?. " + HINT)
                return 1
            dev = parse_device_utc(line)
            print(line)
            if dev is not None:
                print(f"offset device - PC: {dev - (t0 + t1) / 2:+.2f} s")
            return 0
        now = time.time()
        time.sleep(1.0 - (now - int(now)))           # send at the next PC second boundary
        s.write(f"TIME {time.time():.3f}\n".encode())
        line = wait_reply(s, ("OK TIME", "ERR"))
        if not line:
            print("No reply to TIME. " + HINT)
            return 1
        print(line)
        if line.startswith("ERR"):
            print("The gadget rejected the time: is this computer's clock right?")
            return 1
        m = re.search(r"delta=([-+]?\d+\.\d+)s", line)
        if m:
            print(f"drift before sync (device - PC): {float(m.group(1)):+.2f} s")
        if a.tz:
            s.write(f"TZ {a.tz}\n".encode())
            print(wait_reply(s, ("OK TZ", "ERR")) or "No reply to TZ.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
