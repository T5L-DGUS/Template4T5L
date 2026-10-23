#!/usr/bin/env python3
"""V851 UART4 AA55 TLV、Wi-Fi与ABCD OTA协议模拟器。"""

from __future__ import annotations

import argparse
import dataclasses
import json
import struct
import sys
import time
from pathlib import Path
from typing import Iterable


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_VECTORS = REPO_ROOT / "tests" / "serial_bridge_vectors.json"

AA55_MAGIC = b"\xAA\x55"
ABCD_MAGIC = b"\xAB\xCD"

TLV_CMD_PROPERTY = 0x35
TLV_CMD_FACTORY = 0x36
TLV_CMD_SNAPSHOT = 0x37
TLV_CMD_BOOTSTRAP_RESULT = 0x37
TLV_CMD_EVENT_ALARM = 0x38
TLV_CMD_FACTORY_RESET = 0x39
TLV_COMMANDS = {
    TLV_CMD_PROPERTY,
    TLV_CMD_FACTORY,
    TLV_CMD_SNAPSHOT,
    TLV_CMD_EVENT_ALARM,
    TLV_CMD_FACTORY_RESET,
}

TLV_STRUCT_BOOTSTRAP_RESULT = 0x6C
TLV_STRUCT_EVENT_ALARM = 0x6D
BOOTSTRAP_FIELD_LIMITS = {
    0x01: ("device_sn", 64),
    0x02: ("ble_id", 32),
    0x03: ("api_endpoint", 256),
    0x04: ("bind_status", 32),
    0x05: ("qr_url", 256),
}
BOOTSTRAP_QR_VP_ADDR = 0x5500
BOOTSTRAP_QR_VP_WORDS = 64
BOOTSTRAP_QR_BYTES = BOOTSTRAP_QR_VP_WORDS * 2

WIFI_CMD_SCAN = 0xC0
WIFI_CMD_CONNECT = 0xC1
WIFI_CMD_STATUS = 0xC5
WIFI_COMMANDS = {WIFI_CMD_SCAN, WIFI_CMD_CONNECT, WIFI_CMD_STATUS}

TLV_FRAME_MAX = 2048
OTA_FRAME_MAX = 4124

ACTUATOR_STRUCT_TYPES = tuple(range(0x61, 0x6A))
STATE_STRUCT_TYPES = (0x05,) + ACTUATOR_STRUCT_TYPES
ALARM_VP_COUNT = 10
ALARM_VP_MAPPINGS = {
    (1, 1): ("TEMP_HIGH", "HIGH"),
    (1, 2): ("TEMP_LOW", "HIGH"),
    (1, 3): ("TEMP_SENSOR_FAULT", "HIGH"),
    (1, 4): ("HEATER_FAULT", "HIGH"),
    (1, 5): ("OVERHEAT_PROTECTION", "CRITICAL"),
    (2, 1): ("LIQUID_LOW_WARNING", "MEDIUM"),
    (2, 2): ("LOW_LIQUID", "MEDIUM"),
    (2, 3): ("NEBULIZER_DRY_BURN", "HIGH"),
    (2, 4): ("WATER_LEVEL_SENSOR_FAULT", "HIGH"),
    (2, 5): ("HUMIDITY_SENSOR_FAULT", "HIGH"),
    (4, 1): ("EXHAUST_FAN_FAULT", "HIGH"),
    (5, 1): ("INLET_FAN_FAULT", "HIGH"),
    (7, 1): ("FILTER_LIFE_EXHAUSTED", "MEDIUM"),
}
VP_HOUR_TABLES = {
    "exhaust_interval": {1: 0.5, 2: 1.0, 3: 2.0, 4: 4.0, 5: 8.0},
    "humidifier_interval": {1: 2.0, 2: 4.0, 3: 8.0, 4: 12.0},
    "humidifier_running": {1: 0.5, 2: 1.0, 3: 2.0},
    "uvb_daily": {1: 2, 2: 4, 3: 6, 4: 8},
}


class ProtocolError(ValueError):
    """The supplied bytes do not form a valid supported wire frame."""


@dataclasses.dataclass(frozen=True)
class TlvField:
    tag: int
    value: bytes

    def encode(self) -> bytes:
        if not 0 <= self.tag <= 0xFF:
            raise ValueError("TLV tag must fit in one byte")
        if len(self.value) > 0xFFFF:
            raise ValueError("TLV field is too large")
        return bytes([self.tag]) + len(self.value).to_bytes(2, "big") + self.value


@dataclasses.dataclass(frozen=True)
class TlvSegment:
    struct_type: int
    fields: tuple[TlvField, ...]
    payload: bytes


@dataclasses.dataclass(frozen=True)
class DecodedFrame:
    kind: str
    command: int
    raw: bytes
    payload: bytes
    segments: tuple[TlvSegment, ...] = ()


@dataclasses.dataclass(frozen=True)
class BootstrapResult:
    struct_type: int = TLV_STRUCT_BOOTSTRAP_RESULT
    device_sn: bytes = b""
    ble_id: bytes = b""
    api_endpoint: bytes = b""
    bind_status: bytes = b""
    qr_url: bytes = b""


class BootstrapResultCache:
    """Models the firmware's atomic, last-valid-result Bootstrap cache."""

    def __init__(self) -> None:
        self._result: BootstrapResult | None = None

    def get(self) -> BootstrapResult | None:
        return self._result

    def receive(self, frame: bytes) -> BootstrapResult:
        result = decode_bootstrap_result(frame)
        self._result = result
        return result


class AlarmReporterModel:
    """模拟固件的告警扫描、队列忙重试及轮询顺序。"""

    def __init__(self) -> None:
        self.current = [0] * ALARM_VP_COUNT
        self.reported = [0] * ALARM_VP_COUNT
        self.next_index = 0

    def scan(self, values: Iterable[int]) -> None:
        materialized = list(values)
        if len(materialized) != ALARM_VP_COUNT:
            raise ValueError("alarm scan needs ten VP words")
        self.current[:] = materialized

    def next_report(self, queue_available: bool = True):
        for scanned in range(ALARM_VP_COUNT):
            index = (self.next_index + scanned) % ALARM_VP_COUNT
            current = self.current[index]
            reported = self.reported[index]
            if reported and reported != current:
                if not queue_available:
                    return None
                code, level = ALARM_VP_MAPPINGS[(index, reported)]
                self.reported[index] = 0
                self.next_index = (index + 1) % ALARM_VP_COUNT
                return index, reported, True, encode_event_alarm(code, level, True)
            if current and reported != current:
                mapping = ALARM_VP_MAPPINGS.get((index, current))
                if mapping is None:
                    continue
                if not queue_available:
                    return None
                code, level = mapping
                self.reported[index] = current
                self.next_index = (index + 1) % ALARM_VP_COUNT
                return index, current, False, encode_event_alarm(code, level, False)
        return None


def crc16_modbus(data: bytes) -> int:
    crc = 0xFFFF
    for value in data:
        crc ^= value
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc & 0xFFFF


def field_u8(tag: int, value: int) -> TlvField:
    if not 0 <= value <= 0xFF:
        raise ValueError("u8 value out of range")
    return TlvField(tag, bytes([value]))


def field_u32(tag: int, value: int) -> TlvField:
    if not 0 <= value <= 0xFFFFFFFF:
        raise ValueError("u32 value out of range")
    return TlvField(tag, value.to_bytes(4, "big"))


def field_binary64(tag: int, value: float) -> TlvField:
    return TlvField(tag, struct.pack(">d", value))


def field_text(tag: int, value: str) -> TlvField:
    return TlvField(tag, value.encode("utf-8"))


def vp_code_to_protocol_value(mapping: str, code: int) -> float | int:
    """Convert one documented VP option code to its wire-protocol value."""
    try:
        return VP_HOUR_TABLES[mapping][code]
    except KeyError as exc:
        raise ValueError(f"invalid {mapping} VP code: {code}") from exc


def protocol_value_to_vp_code(mapping: str, value: float | int) -> int:
    """Convert only an exact documented wire value back to its VP option code."""
    table = VP_HOUR_TABLES.get(mapping)
    if table is None:
        raise ValueError(f"unknown VP mapping: {mapping}")
    for code, protocol_value in table.items():
        if value == protocol_value:
            return code
    raise ValueError(f"unrepresentable {mapping} protocol value: {value}")


def climate_target_to_vp(value: float) -> int:
    """Apply the firmware's exact integer 20..35 Celsius policy."""
    if not 20.0 <= value <= 35.0 or not value.is_integer():
        raise ValueError("climate target must be an integer from 20 to 35")
    return int(value)


def encode_segment(struct_type: int, fields: Iterable[TlvField] | bytes) -> bytes:
    if not 0 <= struct_type <= 0xFF:
        raise ValueError("struct_type must fit in one byte")
    if isinstance(fields, bytes):
        payload = fields
    else:
        payload = b"".join(field.encode() for field in fields)
    if len(payload) > 0xFFFF:
        raise ValueError("segment is too large")
    return bytes([struct_type]) + len(payload).to_bytes(2, "big") + payload


def encode_tlv_frame(
    command: int,
    segments: Iterable[tuple[int, Iterable[TlvField] | bytes]],
) -> bytes:
    if command not in TLV_COMMANDS:
        raise ValueError("unsupported TLV command")
    encoded_segments = b"".join(
        encode_segment(struct_type, fields) for struct_type, fields in segments
    )
    if not encoded_segments:
        raise ValueError("a TLV frame needs at least one segment")
    declared_length = 1 + len(encoded_segments)
    frame = AA55_MAGIC + declared_length.to_bytes(2, "big")
    frame += bytes([command]) + encoded_segments
    if len(frame) > TLV_FRAME_MAX:
        raise ValueError("TLV frame exceeds 2048 bytes")
    return frame


def encode_snapshot(
    segments: Iterable[tuple[int, Iterable[TlvField] | bytes]],
) -> bytes:
    """Encode a full T5L-to-V851 Environment + actuator snapshot."""
    materialized = list(segments)
    if tuple(struct_type for struct_type, _ in materialized) != STATE_STRUCT_TYPES:
        raise ValueError(
            "a snapshot needs ordered Environment 0x05 and actuator 0x61-0x69 segments"
        )
    return encode_tlv_frame(TLV_CMD_SNAPSHOT, materialized)


def encode_factory_report() -> bytes:
    """按默认产品配置编码T5L到V851的工厂信息帧。"""
    return encode_tlv_frame(
        TLV_CMD_FACTORY,
        [
            (
                0x6A,
                [
                    field_text(0x01, "JP"),
                    field_text(0x03, "MCQX_PET_CABIN"),
                    field_text(0x04, "MCQX-PET-CABIN-V1"),
                    field_text(0x05, "HW-V2.0"),
                    field_text(0x06, "FW-V1.0.0"),
                ],
            )
        ],
    )


def encode_event_alarm(code: str, level: str, recovered: bool) -> bytes:
    """编码0x38/0x6D设备告警；可选payload按当前T5L协议省略。"""
    if not code or not level:
        raise ValueError("alarm code and level must not be empty")
    return encode_tlv_frame(
        TLV_CMD_EVENT_ALARM,
        [
            (
                TLV_STRUCT_EVENT_ALARM,
                [
                    field_u8(0x01, 1),
                    field_text(0x02, code),
                    field_text(0x03, level),
                    field_u8(0x04, int(recovered)),
                ],
            )
        ],
    )


def _decode_fields(payload: bytes) -> tuple[TlvField, ...]:
    fields: list[TlvField] = []
    offset = 0
    while offset < len(payload):
        if len(payload) - offset < 3:
            raise ProtocolError("truncated TLV field header")
        field_length = int.from_bytes(payload[offset + 1 : offset + 3], "big")
        end = offset + 3 + field_length
        if end > len(payload):
            raise ProtocolError("TLV field exceeds its segment")
        fields.append(TlvField(payload[offset], payload[offset + 3 : end]))
        offset = end
    return tuple(fields)


def decode_tlv_frame(frame: bytes) -> DecodedFrame:
    if len(frame) < 8 or frame[:2] != AA55_MAGIC:
        raise ProtocolError("not an AA55 TLV frame")
    declared = int.from_bytes(frame[2:4], "big")
    command = frame[4]
    if command not in TLV_COMMANDS:
        raise ProtocolError("unsupported TLV command")
    if declared < 4 or len(frame) != 4 + declared or len(frame) > TLV_FRAME_MAX:
        raise ProtocolError("invalid TLV frame length")

    segments: list[TlvSegment] = []
    offset = 5
    while offset < len(frame):
        if len(frame) - offset < 3:
            raise ProtocolError("truncated segment header")
        segment_length = int.from_bytes(frame[offset + 1 : offset + 3], "big")
        end = offset + 3 + segment_length
        if end > len(frame):
            raise ProtocolError("segment exceeds frame")
        payload = frame[offset + 3 : end]
        segments.append(TlvSegment(frame[offset], _decode_fields(payload), payload))
        offset = end
    if not segments:
        raise ProtocolError("empty TLV frame")
    return DecodedFrame("tlv", command, frame, frame[5:], tuple(segments))


def decode_bootstrap_result(frame: bytes) -> BootstrapResult:
    decoded = decode_tlv_frame(frame)
    if decoded.command != TLV_CMD_BOOTSTRAP_RESULT:
        raise ProtocolError("not a BootstrapResult frame")

    result: BootstrapResult | None = None
    for segment in decoded.segments:
        if segment.struct_type != TLV_STRUCT_BOOTSTRAP_RESULT:
            continue
        values = {
            "device_sn": b"",
            "ble_id": b"",
            "api_endpoint": b"",
            "bind_status": b"",
            "qr_url": b"",
        }
        for field in segment.fields:
            descriptor = BOOTSTRAP_FIELD_LIMITS.get(field.tag)
            if descriptor is not None:
                name, capacity = descriptor
                values[name] = field.value[: capacity - 1]
        result = BootstrapResult(**values)
    if result is None:
        raise ProtocolError("BootstrapResult segment is missing")
    return result


def bootstrap_qr_vp_payload(result: BootstrapResult) -> bytes:
    """Return the exact zero-padded bytes written to the DGUS QR VP."""
    qr_url = result.qr_url.split(b"\x00", 1)[0]
    if not qr_url or len(qr_url) >= BOOTSTRAP_QR_BYTES:
        return bytes(BOOTSTRAP_QR_BYTES)
    return qr_url.ljust(BOOTSTRAP_QR_BYTES, b"\x00")


def encode_wifi_scan(page: int, count: int = 5) -> bytes:
    if not 0 <= page <= 51 or not 1 <= count <= 0xFF:
        raise ValueError("invalid Wi-Fi scan page/count")
    payload = bytes([WIFI_CMD_SCAN, page * 5, count])
    return AA55_MAGIC + len(payload).to_bytes(2, "big") + payload


def encode_wifi_connect(ssid: str | bytes, password: str | bytes) -> bytes:
    ssid_bytes = ssid.encode() if isinstance(ssid, str) else ssid
    password_bytes = password.encode() if isinstance(password, str) else password
    if len(ssid_bytes) > 32 or len(password_bytes) > 32:
        raise ValueError("SSID/password is longer than 32 bytes")
    payload = bytes([WIFI_CMD_CONNECT, 1])
    payload += len(ssid_bytes).to_bytes(2, "big") + ssid_bytes
    payload += len(password_bytes).to_bytes(2, "big") + password_bytes
    return AA55_MAGIC + len(payload).to_bytes(2, "big") + payload


def decode_wifi_frame(frame: bytes) -> DecodedFrame:
    if len(frame) < 5 or frame[:2] != AA55_MAGIC:
        raise ProtocolError("not an AA55 Wi-Fi frame")
    declared = int.from_bytes(frame[2:4], "big")
    if declared < 1 or len(frame) != 4 + declared or len(frame) > TLV_FRAME_MAX:
        raise ProtocolError("invalid Wi-Fi frame length")
    command = frame[4]
    if command not in WIFI_COMMANDS:
        raise ProtocolError("unsupported Wi-Fi command")
    return DecodedFrame("wifi", command, frame, frame[5:])


def encode_ota_frame(command: int, payload: bytes = b"") -> bytes:
    if not 0 <= command <= 0xFF:
        raise ValueError("OTA command must fit in one byte")
    body = bytes([command]) + payload
    frame = ABCD_MAGIC + len(body).to_bytes(2, "big") + body
    if len(frame) > OTA_FRAME_MAX:
        raise ValueError("OTA frame exceeds 4124 bytes")
    return frame


def decode_ota_frame(frame: bytes) -> DecodedFrame:
    if len(frame) < 5 or frame[:2] != ABCD_MAGIC:
        raise ProtocolError("not an ABCD OTA frame")
    declared = int.from_bytes(frame[2:4], "big")
    if declared < 1 or len(frame) != 4 + declared or len(frame) > OTA_FRAME_MAX:
        raise ProtocolError("invalid OTA frame length")
    return DecodedFrame("ota", frame[4], frame, frame[5:])


def decode_frame(frame: bytes) -> DecodedFrame:
    if frame[:2] == ABCD_MAGIC:
        return decode_ota_frame(frame)
    if len(frame) >= 5 and frame[4] in TLV_COMMANDS:
        return decode_tlv_frame(frame)
    return decode_wifi_frame(frame)


class FrameStreamDecoder:
    """Noise-tolerant UART4 stream splitter with firmware length semantics."""

    def __init__(self) -> None:
        self.buffer = bytearray()
        self.dropped_bytes = 0
        self.invalid_frames = 0

    def feed(self, data: bytes) -> list[DecodedFrame]:
        self.buffer.extend(data)
        decoded: list[DecodedFrame] = []
        while True:
            aa = self.buffer.find(AA55_MAGIC)
            ab = self.buffer.find(ABCD_MAGIC)
            candidates = [index for index in (aa, ab) if index >= 0]
            if not candidates:
                keep = 1 if self.buffer[-1:] in (b"\xAA", b"\xAB") else 0
                self.dropped_bytes += len(self.buffer) - keep
                if keep:
                    self.buffer[:] = self.buffer[-1:]
                else:
                    self.buffer.clear()
                break
            start = min(candidates)
            if start:
                del self.buffer[:start]
                self.dropped_bytes += start
            if len(self.buffer) < 5:
                break

            declared = int.from_bytes(self.buffer[2:4], "big")
            if self.buffer[:2] == ABCD_MAGIC:
                total = 4 + declared
                valid_header = 1 <= declared and total <= OTA_FRAME_MAX
            else:
                command = self.buffer[4]
                if command in TLV_COMMANDS:
                    total = 4 + declared
                    valid_header = 4 <= declared and total <= TLV_FRAME_MAX
                elif command in WIFI_COMMANDS:
                    total = 4 + declared
                    valid_header = 1 <= declared and total <= TLV_FRAME_MAX
                else:
                    total = 0
                    valid_header = False
            if not valid_header:
                del self.buffer[0]
                self.dropped_bytes += 1
                continue
            if len(self.buffer) < total:
                break
            raw = bytes(self.buffer[:total])
            del self.buffer[:total]
            try:
                decoded.append(decode_frame(raw))
            except ProtocolError:
                self.invalid_frames += 1
        return decoded


def load_vectors(path: Path = DEFAULT_VECTORS) -> dict[str, object]:
    return json.loads(path.read_text(encoding="utf-8"))


def _frame_summary(frame: DecodedFrame) -> dict[str, object]:
    result: dict[str, object] = {
        "kind": frame.kind,
        "command": f"0x{frame.command:02X}",
        "length": len(frame.raw),
        "payload_hex": frame.payload.hex(" ").upper(),
    }
    if frame.segments:
        result["segments"] = [
            {
                "struct_type": f"0x{segment.struct_type:02X}",
                "fields": [
                    {"tag": f"0x{field.tag:02X}", "value": field.value.hex(" ").upper()}
                    for field in segment.fields
                ],
            }
            for segment in frame.segments
        ]
    return result


def _open_serial(port: str, baud: int):
    try:
        import serial  # type: ignore
    except ImportError as exc:
        raise SystemExit("monitor requires pyserial: python -m pip install pyserial") from exc
    return serial.Serial(port, baudrate=baud, timeout=0.05)


def command_decode(args: argparse.Namespace) -> int:
    raw = bytes.fromhex(args.hex)
    print(json.dumps(_frame_summary(decode_frame(raw)), ensure_ascii=False, indent=2))
    return 0


def command_monitor(args: argparse.Namespace) -> int:
    decoder = FrameStreamDecoder()
    with _open_serial(args.port, args.baud) as port:
        while True:
            data = port.read(port.in_waiting or 1)
            for frame in decoder.feed(data):
                print(json.dumps(_frame_summary(frame), ensure_ascii=False))
            time.sleep(0.001)


def command_wifi_scan(args: argparse.Namespace) -> int:
    print(encode_wifi_scan(args.page).hex(" ").upper())
    return 0


def command_wifi_connect(args: argparse.Namespace) -> int:
    print(encode_wifi_connect(args.ssid, args.password).hex(" ").upper())
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    decode = sub.add_parser("decode", help="decode one hex frame")
    decode.add_argument("hex")
    decode.set_defaults(func=command_decode)

    scan = sub.add_parser("wifi-scan", help="build a C0 scan frame")
    scan.add_argument("page", type=int)
    scan.set_defaults(func=command_wifi_scan)

    connect = sub.add_parser("wifi-connect", help="build an exact C1 frame")
    connect.add_argument("ssid")
    connect.add_argument("password")
    connect.set_defaults(func=command_wifi_connect)

    monitor = sub.add_parser("monitor", help="split/decode live UART4 traffic")
    monitor.add_argument("--port", required=True)
    monitor.add_argument("--baud", type=int, default=115200)
    monitor.set_defaults(func=command_monitor)
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    return int(args.func(args))


if __name__ == "__main__":
    sys.exit(main())
