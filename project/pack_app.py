"""Pack a Keil BIN as the 64 KiB DGUS 112 APP library checked by BOOT."""
import argparse
from pathlib import Path


def crc16(payload):
    value = 0xFFFF
    for byte in payload:
        value ^= byte
        for _ in range(8):
            value = (value >> 1) ^ (0xA001 if value & 1 else 0)
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    raw = args.source.read_bytes()
    if not raw or len(raw) > 0xFF00:
        parser.error('APP code must contain 1..65280 bytes; the final 256 bytes are reserved.')
    if args.source.resolve() == args.destination.resolve():
        parser.error('Source and destination must be different files.')
    payload = raw.ljust(0xFF00, b'\0')
    image = (payload + crc16(payload).to_bytes(2, 'little')).ljust(0x10000, b'\0')
    assert crc16(image[:0xFF02]) == 0
    args.destination.parent.mkdir(parents=True, exist_ok=True)
    args.destination.write_bytes(image)
    assert args.destination.read_bytes() == image
    print(f'{args.destination}: {len(image)} bytes; BOOT CRC passed; APP {len(raw)} bytes')


if __name__ == '__main__':
    main()
