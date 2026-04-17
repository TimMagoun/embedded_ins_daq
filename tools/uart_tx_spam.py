#!/usr/bin/env python3

from __future__ import annotations

import argparse
import itertools
import time

import serial


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Continuously transmit recognizable UART traffic for capture validation."
    )
    parser.add_argument("--port", required=True)
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--duration", type=float, default=20.0)
    parser.add_argument("--interval", type=float, default=0.05)
    parser.add_argument("--prefix", default="TASK3")
    return parser


def main() -> int:
    args = build_parser().parse_args()
    deadline = time.monotonic() + args.duration

    with serial.Serial(args.port, args.baud, timeout=1) as ser:
        for counter in itertools.count():
            if time.monotonic() >= deadline:
                break

            payload = (
                f"{args.prefix}:{counter:06d}:"
                "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\r\n"
            ).encode("ascii")
            ser.write(payload)
            ser.flush()
            time.sleep(args.interval)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
