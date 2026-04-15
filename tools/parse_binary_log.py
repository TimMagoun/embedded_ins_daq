#!/usr/bin/env python3

from __future__ import annotations

import json
import struct
import sys
import zlib
from pathlib import Path

HEADER_STRUCT = struct.Struct("<HHIQII")
SESSION_START_STRUCT = struct.Struct("<IIII")
FAULT_EVENT_STRUCT = struct.Struct("<IIII")
UART_PREFIX_STRUCT = struct.Struct("<I")
SYNC_EDGE_STRUCT = struct.Struct("<II")

RECORD_TYPES = {
    1: "session_start",
    2: "fault_event",
    3: "uart_data",
    4: "sync_edge",
}


def _decode_record(header: tuple[int, int, int, int, int, int], payload: bytes) -> dict:
    record_type, version, payload_length, timestamp_us, source_id, crc32 = header

    record = {
        "type": RECORD_TYPES.get(record_type, f"unknown_{record_type}"),
        "version": version,
        "payload_length": payload_length,
        "timestamp_us": timestamp_us,
        "source_id": source_id,
        "crc32": crc32,
    }

    if record_type == 1:
        session_id, config_hash, enabled_port_mask, enabled_port_count = (
            SESSION_START_STRUCT.unpack(payload)
        )
        record.update(
            {
                "session_id": session_id,
                "config_hash": config_hash,
                "enabled_port_mask": enabled_port_mask,
                "enabled_port_count": enabled_port_count,
            }
        )
    elif record_type == 2:
        fault_code, fault_severity, health_status, _reserved = (
            FAULT_EVENT_STRUCT.unpack(payload)
        )
        record.update(
            {
                "fault_code": fault_code,
                "fault_severity": fault_severity,
                "health_status": health_status,
            }
        )
    elif record_type == 3:
        (data_length,) = UART_PREFIX_STRUCT.unpack(payload[: UART_PREFIX_STRUCT.size])
        data = payload[UART_PREFIX_STRUCT.size :]
        if len(data) != data_length:
            raise ValueError("uart_data payload length does not match encoded prefix")
        record.update({"data_length": data_length, "data_hex": data.hex()})
    elif record_type == 4:
        edge_polarity, _reserved = SYNC_EDGE_STRUCT.unpack(payload)
        record.update({"edge_polarity": edge_polarity})
    else:
        record["payload_hex"] = payload.hex()

    return record


def parse_binary_log(path: Path) -> list[dict]:
    data = path.read_bytes()
    offset = 0
    records: list[dict] = []

    while offset < len(data):
        if offset + HEADER_STRUCT.size > len(data):
            raise ValueError(f"truncated header at byte offset {offset}")

        header_bytes = data[offset : offset + HEADER_STRUCT.size]
        header = HEADER_STRUCT.unpack(header_bytes)
        payload_length = header[2]
        payload_start = offset + HEADER_STRUCT.size
        payload_end = payload_start + payload_length
        if payload_end > len(data):
            raise ValueError(f"truncated payload at byte offset {offset}")

        payload = data[payload_start:payload_end]
        expected_crc = zlib.crc32(header_bytes[:-4])
        expected_crc = zlib.crc32(payload, expected_crc) & 0xFFFFFFFF
        if expected_crc != header[5]:
            raise ValueError(
                f"crc mismatch at byte offset {offset}: expected {expected_crc}, got {header[5]}"
            )

        records.append(_decode_record(header, payload))
        offset = payload_end

    return records


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: parse_binary_log.py <binary-log-path>", file=sys.stderr)
        return 2

    records = parse_binary_log(Path(sys.argv[1]))
    print(json.dumps(records, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
