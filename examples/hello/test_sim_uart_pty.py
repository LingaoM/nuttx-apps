#!/usr/bin/env python3
"""Smoke test for sim UART PTY RX/TX.

Usage:

  ./examples/hello/test_sim_uart_pty.py /dev/pts/N

It expects a sim:nsh build with:

  CONFIG_SIM_UART_PTY=y
  CONFIG_SIM_UART0_NAME="/dev/ttySIM0"
  CONFIG_EXAMPLES_HELLO=y

The hello example is expected to open /dev/ttySIM0, receive a binary
host-to-NuttX frame, return a binary NuttX-to-host frame, receive an ACK,
and print the final pass line.
"""

import os
import struct
import sys
import termios
import time
import tty


HDR_SIZE = 12
HOST_MAGIC = b"H2NX"
NUTTX_MAGIC = b"N2HX"
ACK_MAGIC = b"ACK!"
HOST_TO_NUTTX_LEN = 32768
NUTTX_TO_HOST_LEN = 49152
HOST_SEED = 0x13579BDF
NUTTX_SEED = 0x2468ACE0
FNV_OFFSET = 2166136261
FNV_PRIME = 16777619


def fail(msg):
    print(f"TEST FAILED: {msg}", file=sys.stderr)
    sys.exit(1)


def pattern(seed, index):
    value = (seed ^ ((index * 1103515245) & 0xFFFFFFFF)) & 0xFFFFFFFF
    value ^= index >> 3
    value ^= value >> 16
    value ^= value >> 8
    return value & 0xFF


def hash_update(value, byte):
    value ^= byte
    value = (value * FNV_PRIME) & 0xFFFFFFFF
    return value


def payload_hash(seed, length):
    value = FNV_OFFSET

    for index in range(length):
        value = hash_update(value, pattern(seed, index))

    return value


def make_payload(seed, length):
    return bytes(pattern(seed, index) for index in range(length))


def write_all(fd, data):
    deadline = time.time() + 30
    off = 0

    while off < len(data) and time.time() < deadline:
        try:
            ret = os.write(fd, data[off:])
            if ret > 0:
                off += ret
                continue
        except BlockingIOError:
            pass

        time.sleep(0.001)

    if off != len(data):
        fail(f"write timeout, wrote {off}/{len(data)} bytes")


def read_exact(fd, length):
    deadline = time.time() + 30
    data = bytearray()

    while len(data) < length and time.time() < deadline:
        try:
            chunk = os.read(fd, min(4096, length - len(data)))
            if chunk:
                data.extend(chunk)
                continue
        except BlockingIOError:
            pass

        time.sleep(0.001)

    if len(data) != length:
        fail(f"read timeout, got {len(data)}/{length} bytes")

    return bytes(data)


def pack_header(magic, length, checksum):
    return magic + struct.pack("<II", length, checksum)


def unpack_header(data):
    magic = data[:4]
    length, checksum = struct.unpack("<II", data[4:HDR_SIZE])
    return magic, length, checksum


def run_host_exchange(slave_path):
    host_fd = None
    old_termios = None

    try:
        print(f"HOST_OPEN: {slave_path}")
        host_fd = os.open(slave_path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)

        try:
            old_termios = termios.tcgetattr(host_fd)
            tty.setraw(host_fd, termios.TCSANOW)
        except termios.error:
            old_termios = None

        payload = make_payload(HOST_SEED, HOST_TO_NUTTX_LEN)
        checksum = payload_hash(HOST_SEED, HOST_TO_NUTTX_LEN)
        write_all(host_fd, pack_header(HOST_MAGIC, len(payload), checksum))
        write_all(host_fd, payload)
        print(f"HOST_TX: {len(payload)} bytes checksum=0x{checksum:08x}")

        header = read_exact(host_fd, HDR_SIZE)
        magic, length, checksum = unpack_header(header)
        if magic != NUTTX_MAGIC:
            fail(f"bad response magic: {magic!r}")

        if length != NUTTX_TO_HOST_LEN:
            fail(f"bad response length: {length}")

        expected_checksum = payload_hash(NUTTX_SEED, length)
        if checksum != expected_checksum:
            fail(f"bad response checksum header=0x{checksum:08x} "
                 f"expected=0x{expected_checksum:08x}")

        payload = read_exact(host_fd, length)
        actual_checksum = FNV_OFFSET
        mismatch = None
        for index, byte in enumerate(payload):
            expected = pattern(NUTTX_SEED, index)
            if byte != expected and mismatch is None:
                mismatch = (index, byte, expected)

            actual_checksum = hash_update(actual_checksum, byte)

        if mismatch is not None:
            index, byte, expected = mismatch
            fail(f"response mismatch offset={index} got=0x{byte:02x} "
                 f"expected=0x{expected:02x}")

        if actual_checksum != checksum:
            fail(f"response checksum payload=0x{actual_checksum:08x} "
                 f"header=0x{checksum:08x}")

        print(f"HOST_RX: {length} bytes checksum=0x{actual_checksum:08x}")

        write_all(host_fd, pack_header(ACK_MAGIC, 0, 0))
        print("HOST_TX: ACK")
    finally:
        if old_termios is not None:
            termios.tcsetattr(host_fd, termios.TCSANOW, old_termios)

        if host_fd is not None:
            os.close(host_fd)


def main():
    if len(sys.argv) != 2:
        fail(f"usage: {sys.argv[0]} /dev/pts/N")

    run_host_exchange(sys.argv[1])
    print("TEST PASSED")


if __name__ == "__main__":
    main()
