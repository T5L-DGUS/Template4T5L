"""Build the 256-byte V851 factory NOR record; never connect to a device."""
from __future__ import annotations

import argparse
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
FIELDS = (
    ("sales_country", "SALES_COUNTRY", 8, 8),
    ("product_key", "PRODUCT_KEY", 16, 64),
    ("model", "MODEL", 80, 64),
    ("hardware_version", "HARDWARE_VERSION", 144, 32),
    ("firmware_version", "FIRMWARE_VERSION", 176, 32),
)


def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0xA001 if crc & 1 else 0)
    return crc


def defaults(platform: str = "T5L") -> dict[str, str]:
    source = (ROOT / f"include/{platform}/{platform}OSConfig.h").read_text(encoding="utf-8")
    values = dict(re.findall(r'^#define v851FACTORY_(\w+)\s+"([^"]*)"', source, re.M))
    return {name: values[macro] for name, macro, _, _ in FIELDS}


def build_record(values: dict[str, str]) -> bytes:
    record = bytearray(256)
    record[:8] = b"VFC1\x00\x01\x01\x00"
    for name, _, offset, capacity in FIELDS:
        value = values[name].encode("ascii")
        if not 1 <= len(value) < capacity or any(b < 0x20 or b > 0x7E for b in value):
            raise ValueError(f"{name}: expected 1..{capacity - 1} printable ASCII bytes")
        record[offset:offset + len(value)] = value
    record[254:] = crc16(record[:254]).to_bytes(2, "little")
    return bytes(record)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", choices=("T5L", "T5F"), default="T5L")
    parser.add_argument("--output", type=Path, required=True,
                        help="Raw record only; program at NOR word address 0x0100 (byte offset 0x0200)")
    for name, _, _, _ in FIELDS:
        parser.add_argument("--" + name.replace("_", "-"))
    args = parser.parse_args()
    values = defaults(args.platform)
    for name in values:
        if getattr(args, name) is not None:
            values[name] = getattr(args, name)
    try:
        record = build_record(values)
    except (UnicodeEncodeError, ValueError) as error:
        parser.error(str(error))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(record)
    print(f"{args.output}: {len(record)} bytes, NOR block 0, words 0x0100..0x017F")
    for name, value in values.items():
        print(f"  {name}: {value}")


if __name__ == "__main__":
    main()
