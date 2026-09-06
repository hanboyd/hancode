"""Diagnostic dump tool for the RC003 capture-only HID filter prototype.

Reads the filter's control device (\\\\.\\Rc003HidCapture) and prints the
capture ring snapshot.  Run AFTER the prototype driver has been installed
and attached (installation is a separate, approved step - this tool does
not install anything).

Usage:
  python rc003_capture_dump.py [--clear] [--watch N] [--repeat N]

--clear      reset ring and counters first
--watch N    dump every N seconds until Ctrl+C (default N=1)
--repeat N   dump N times with 1 s interval, then exit
"""
import argparse
import ctypes
from ctypes import wintypes
import struct
import sys
import time

DEVICE_PATH = r"\\.\Rc003HidCapture"
GENERIC_READ = 0x80000000
GENERIC_WRITE = 0x40000000
OPEN_EXISTING = 3

FILE_DEVICE_UNKNOWN = 0x22
METHOD_BUFFERED = 0
FILE_READ_ACCESS = 1
FILE_WRITE_ACCESS = 2


def ctl_code(device_type, function, access):
    return (device_type << 16) | (access << 14) | (function << 2) | METHOD_BUFFERED


IOCTL_RC003_CAPTURE_DUMP = ctl_code(FILE_DEVICE_UNKNOWN, 0x800, FILE_READ_ACCESS)
IOCTL_RC003_CAPTURE_CLEAR = ctl_code(
    FILE_DEVICE_UNKNOWN, 0x804, FILE_READ_ACCESS | FILE_WRITE_ACCESS
)

HEADER = struct.Struct("<IIIIIIqq")  # matches RC003_DUMP_HEADER
ENTRY = struct.Struct("<IIIBBH3H")  # matches RC003_CAPTURE_ENTRY
SNAPSHOT_SIZE = HEADER.size + 256 * ENTRY.size

USAGE_NAMES = {
    0x0028: "OK(0x0028)",
    0x0080: "VOL_UP(0x0080)",
    0x0081: "VOL_DOWN(0x0081)",
    0x00F1: "BACK(0x00F1)",
}


def usage_name(value):
    return USAGE_NAMES.get(value, f"0x{value:04X}")


def fmt_qpc(ticks, frequency):
    if not frequency:
        return str(ticks)
    return f"{ticks / frequency * 1000.0:10.3f} ms"


def open_device(write=False):
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.CreateFileW.argtypes = (
        wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p,
        wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE,
    )
    kernel32.CreateFileW.restype = wintypes.HANDLE
    access = GENERIC_READ | GENERIC_WRITE if write else GENERIC_READ
    handle = kernel32.CreateFileW(
        DEVICE_PATH,
        access,
        0,
        None,
        OPEN_EXISTING,
        0,
        None,
    )
    if handle == wintypes.HANDLE(-1).value:
        raise ctypes.WinError(ctypes.get_last_error())
    return kernel32, handle


def ioctl(kernel32, handle, code, out_size):
    kernel32.DeviceIoControl.argtypes = (
        wintypes.HANDLE, wintypes.DWORD, ctypes.c_void_p, wintypes.DWORD,
        ctypes.c_void_p, wintypes.DWORD, ctypes.POINTER(wintypes.DWORD),
        ctypes.c_void_p,
    )
    kernel32.DeviceIoControl.restype = wintypes.BOOL
    buffer = ctypes.create_string_buffer(out_size)
    returned = wintypes.DWORD()
    ok = kernel32.DeviceIoControl(
        handle, code, None, 0, buffer, out_size, ctypes.byref(returned), None
    )
    if not ok:
        raise ctypes.WinError(ctypes.get_last_error())
    return buffer.raw[: returned.value]


def dump_snapshot(clear):
    kernel32, handle = open_device(write=clear)
    try:
        if clear:
            ioctl(kernel32, handle, IOCTL_RC003_CAPTURE_CLEAR, 0)
            print("ring cleared")
        raw = ioctl(kernel32, handle, IOCTL_RC003_CAPTURE_DUMP, SNAPSHOT_SIZE)
    finally:
        kernel32.CloseHandle(handle)

    if len(raw) < HEADER.size:
        print(f"short dump: {len(raw)} bytes")
        return

    header = HEADER.unpack(raw[: HEADER.size])
    (
        entry_count, last_seq, seen, matched, id1_seen, length_mismatch,
        qpc_freq, dump_ts,
    ) = header
    print(
        f"entries={entry_count} last_seq={last_seq} seen={seen} matched={matched} "
        f"rid1={id1_seen} length_mismatch={length_mismatch} qpc_freq={qpc_freq}"
    )
    offset = HEADER.size
    for index in range(entry_count):
        if offset + ENTRY.size > len(raw):
            break
        seq, ts_low, ts_high, rid, flags, length, s0, s1, s2 = ENTRY.unpack(
            raw[offset: offset + ENTRY.size]
        )
        ts = (ts_high << 32) | ts_low
        offset += ENTRY.size
        flags_text = []
        for bit, name in ((0x01, "OK"), (0x02, "VOL_UP"), (0x04, "VOL_DOWN"), (0x08, "BACK")):
            if flags & bit:
                flags_text.append(name)
        print(
            f"  [{seq:04d}] rid={rid} len={length} flags={','.join(flags_text) or '-'} "
            f"t={fmt_qpc(ts, qpc_freq)} slots={usage_name(s0)},{usage_name(s1)},{usage_name(s2)}"
        )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--clear", action="store_true")
    parser.add_argument("--watch", type=float, default=0.0, metavar="SECONDS")
    parser.add_argument("--repeat", type=int, default=1, metavar="N")
    args = parser.parse_args()

    if args.watch > 0:
        try:
            while True:
                dump_snapshot(clear=False)
                print()
                time.sleep(args.watch)
        except KeyboardInterrupt:
            return 0
    for _ in range(max(1, args.repeat)):
        dump_snapshot(clear=args.clear)
        args.clear = False
        if args.repeat > 1:
            print()
            time.sleep(1.0)
    return 0


if __name__ == "__main__":
    sys.exit(main())
