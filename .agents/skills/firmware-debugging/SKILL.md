---
name: firmware-debugging
description: Use when debugging ESP-IDF firmware behavior in this repository, especially boot failures, reset loops, panics, ready-banner timeouts, serial monitor anomalies, flash or port issues, or when GDBStub inspection is needed.
---

# Firmware Debugging

Debug firmware in this repo by preserving first-failure evidence, using the repo capture tools, and escalating to GDBStub only when logs and panic decode are insufficient.

Current known-good hardware paths for this repo:

- built-in Espressif USB JTAG/serial: `/dev/ttyACM1`
- host USB Ethernet NIC: `enxa0cec8b57b54`
- direct-link static IPv4: host `192.168.1.1/24`, ESP `192.168.1.100/24`
- ESP32-P4 rev `v1.3` requires the pre-`v3` compatibility Kconfig path in `sdkconfig.defaults`

## Start Here

1. Load the ESP-IDF environment first.
2. Reproduce with the narrowest command that matches the failure.
3. Capture logs before changing code or reflashing repeatedly.
4. Decode panics offline before opening an interactive debugger.
5. Use GDBStub only when the saved evidence is not enough.

## Required Setup

Always run:

```bash
source ./tools/setup.sh
./tools/bootstrap_env.sh
```

If the issue may be build-related, also run:

```bash
idf.py build
```

## Triage Table

| Situation | First command | What it tells you | Next step |
| --- | --- | --- | --- |
| Flash fails | `idf.py -p /dev/ttyACM1 flash` | Port, permission, boot-mode, or transport failure | Fix the host or port issue before changing firmware |
| Monitor shows no ready banner | `python3 -m tools.run_case --case platform_smoke` | Captures a full boot attempt with logs and copied artifacts | Inspect `monitor.log`, then decode a panic if one was captured |
| Panic or abort appears | `./tools/decode_panic.sh --elf build/embedded_ins_daq.elf --panic-log artifacts/latest/device/monitor.log` | Resolves the backtrace against the matching ELF | Use GDBStub only if the decoded trace is still ambiguous |
| Hang with no panic | `python3 -m tools.monitor --port /dev/ttyACM1 --timeout 30` | Confirms whether logs stall, loop, or stop before readiness | Reproduce narrowly, then use GDBStub if serial evidence is insufficient |
| Crash enters GDBStub | `idf.py gdb` | Starts interactive inspection against the built ELF | Use `references/gdbstub.md` for the first debugger commands |

## Choose The Path

### Boot failure, ready-banner timeout, or unknown runtime issue

Use the case runner first when possible because it saves logs and build artifacts:

```bash
python3 -m tools.run_case --case platform_smoke
python3 -m tools.run_case --case clock_monotonicity
python3 -m tools.run_case --case <case_name> --port /dev/ttyACM1 --timeout 30
```

For manual reproduction with live logs:

```bash
idf.py -p /dev/ttyACM1 flash
python3 -m tools.monitor --port /dev/ttyACM1 --ready-banner "READY: platform_smoke" --timeout 30
```

### Flash or serial-port problem

Separate port and flashing failures from firmware failures:

```bash
idf.py -p /dev/ttyACM1 flash
idf.py -p /dev/ttyACM1 monitor
```

Check that `BOARD_PORT` is set correctly after `source ./tools/setup.sh`.

### Ethernet direct-link validation

If the issue involves the new Ethernet path, verify the current known-good smoke flow first:

```bash
python3 -m tools.monitor --port /dev/ttyACM1 --ready-banner "READY: ethernet_smoke" --timeout 30
sudo ip addr replace 192.168.1.1/24 dev enxa0cec8b57b54
sudo ip link set dev enxa0cec8b57b54 up
ethtool enxa0cec8b57b54
ping 192.168.1.100
```

### Panic, abort, or reset loop

Capture the monitor log, then decode against the exact built ELF:

```bash
./tools/decode_panic.sh \
  --elf build/embedded_ins_daq.elf \
  --panic-log artifacts/latest/device/monitor.log
```

If using run-case, inspect:

- `artifacts/latest/device/<case_name>/monitor.log`
- `artifacts/latest/device/<case_name>/decoded_backtrace.txt`

### Interactive debugger needed

Use GDBStub only after the panic log and decoded backtrace are not enough.

Start from the matching ELF:

```bash
idf.py gdb
```

If direct invocation is needed:

```bash
riscv32-esp-elf-gdb build/embedded_ins_daq.elf
```

For the first interactive commands after attach, read `references/gdbstub.md`.

Use this for:

- ambiguous crash sites after backtrace decode
- inspecting registers or memory at failure
- checking task state during a crash
- confirming where a hang or fault actually stops

## Quick Reference

- Environment check:

```bash
source ./tools/setup.sh
./tools/bootstrap_env.sh
idf.py --version
idf.py --list-targets
```

- Build:

```bash
idf.py build
```

- Flash and monitor:

```bash
idf.py -p /dev/ttyACM1 flash
idf.py -p /dev/ttyACM1 flash monitor
python3 -m tools.monitor --port /dev/ttyACM1 --ready-banner "READY: platform_smoke" --timeout 30
python3 -m tools.monitor --port /dev/ttyACM1 --ready-banner "READY: ethernet_smoke" --timeout 30
sudo ip addr replace 192.168.1.1/24 dev enxa0cec8b57b54
sudo ip link set dev enxa0cec8b57b54 up
ethtool enxa0cec8b57b54
ping 192.168.1.100
```

- Artifact capture:

```bash
python3 -m tools.run_case --case platform_smoke
python3 -m tools.run_case --case clock_monotonicity
```

- Panic decode:

```bash
./tools/decode_panic.sh --elf build/embedded_ins_daq.elf --panic-log artifacts/latest/device/monitor.log
```

- Debugger:

```bash
idf.py gdb
riscv32-esp-elf-gdb build/embedded_ins_daq.elf
```

## Common Mistakes

- Skipping `source ./tools/setup.sh` before `idf.py`, monitor, or debugger commands.
- Reflashing repeatedly before saving the first failing log.
- Treating a serial-port or permission issue as a firmware bug.
- Debugging with an ELF that does not match the flashed image.
- Jumping straight to GDBStub before checking the decoded panic report.
- Recommending JTAG or OpenOCD for this repo stage. This repo uses serial logs, panic decode, and GDBStub first.
- Changing `sdkconfig.defaults` back to the ESP32-P4 `v3.x` revision path on this `v1.3` board. That breaks flashing before runtime debugging even starts.
