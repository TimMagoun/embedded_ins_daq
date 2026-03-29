# embedded_ins_daq

Revision 1 firmware is built as a native `ESP-IDF` project for the `Waveshare ESP32-P4-NANO` targeting `esp32p4`.

This repository keeps the default firmware workflow on standard `ESP-IDF` commands:

- `idf.py build`
- `idf.py flash`
- `idf.py monitor`
- `cmake --build <host_build_dir>`
- `ctest --test-dir <host_build_dir>`

Small helper scripts exist only where they add repeatability for environment validation, artifact capture, and device-case orchestration.

Python developer tooling is managed with `uv`. Formatting and pre-commit hooks are configured for:

- Python with `black`
- shell with `shfmt`
- Markdown with `mdformat`
- C/C++ with `clang-format` using Google style
- host unit tests with `gtest` orchestrated through `ctest`

## Quickstart

### 1. Create a repo-local environment file

Copy [esp.env.example](/home/agent/workspace/embedded_ins_daq/esp.env.example) to `esp.env` and set:

- `IDF_PATH` to your local ESP-IDF checkout
- `BOARD_PORT` to the default serial device for your board

Example:

```bash
cp esp.env.example esp.env
```

### 2. Load the environment

Run:

```bash
source ./tools/setup.sh
```

The setup script:

- loads variables from `esp.env` into the current shell
- sources `${IDF_PATH}/export.sh`
- makes `idf.py` and the ESP-IDF Python environment available to subsequent commands

Run it again in each new shell before using ESP-IDF or device tools.

### 3. Validate the environment

```bash
./tools/bootstrap_env.sh
```

The bootstrap script verifies:

- the active `ESP-IDF` version is new enough for this project
- `esp32p4` is supported by the active toolchain
- desktop tooling like `cmake` and `ctest` is present
- `uv` is present for the repo's Python tooling

### 3a. Install the developer tooling

Install `uv` first if it is not already on your `PATH`, then sync the repo-local Python environment:

```bash
uv sync --group dev
```

Install the git hook:

```bash
uv run --group dev pre-commit install
```

Run the full formatter pass at any time with:

```bash
uv run --group dev pre-commit run --all-files
```

### 4. Select the target

In a clean workspace, set the target once:

```bash
idf.py set-target esp32p4
```

The project defaults in [sdkconfig.defaults](/home/agent/workspace/embedded_ins_daq/sdkconfig.defaults) keep the ESP32-P4 v1.3 compatibility settings required by this board.

### 5. Build the firmware

```bash
idf.py build
```

The bring-up firmware emits clear ready banners on `UART0`, which remains the default console and panic-output path during bring-up.

### 6. Run the host smoke test

Configure the small desktop test target once:

```bash
cmake -S host_tests -B build_host
cmake --build build_host
ctest --test-dir build_host
```

The host test suite uses `gtest`, with `ctest` used as the top-level runner. Host test artifacts are written under `build_host/artifacts/<test_name>`.

Binary log fixtures can be decoded with:

```bash
python3 tools/parse_binary_log.py host_tests/fixtures/session_example.bin
```

### 7. Flash and monitor directly

Use standard `ESP-IDF` commands whenever you want the manual workflow:

```bash
idf.py -p /dev/ttyACM0 flash monitor
```

The first-line debug flow is:

```text
serial boot log -> ready banner timeout -> saved panic log -> decoded backtrace -> GDBStub session when interactive inspection is needed
```

### 8. Capture serial output to both the terminal and a log

Use the repo tool when you want a reusable, non-interactive log file while still seeing the live monitor stream:

```bash
idf.py -p /dev/ttyACM0 flash
python3 -m tools.monitor --ready-banner "READY: platform_smoke"
```

By default this uses `BOARD_PORT` from `esp.env` and writes the latest monitor log to `artifacts/latest/device/monitor.log`.
It assumes `source ./tools/setup.sh` has already been run in the current shell.
The tool passes `--disable-auto-color` to `idf.py monitor` so the saved log stays easy to parse while the same stream remains visible in the terminal.

### 9. Run the platform smoke case with artifact capture

When you want a repeatable case folder with logs and copied build outputs, use:

```bash
python3 -m tools.run_case --case platform_smoke
```

For the clock-specific bring-up check, use:

```bash
python3 -m tools.run_case --case clock_monotonicity
```

For the two-port SD-backed reference capture milestone, use:

```bash
python3 -m tools.run_case --case two_port_reference_capture
```

Artifacts land in:

- `artifacts/runs/device/<case_name>/<timestamp>`
- `artifacts/latest/device/<case_name>`

Each device case captures:

- the monitor log
- the active `sdkconfig`
- the built `elf`
- the built `bin` images when present
- extracted artifact files emitted by the firmware, including `session.bin` when present
- a decoded panic report when a backtrace is found

## Crash Workflow

If a panic log was already captured, decode it offline with:

```bash
./tools/decode_panic.sh \
  --elf build/embedded_ins_daq.elf \
  --panic-log artifacts/latest/device/monitor.log
```

This keeps crash decoding non-GUI and tied to the exact built `elf`.

## Notes

- Run `source ./tools/setup.sh` in each new shell before ESP-IDF or device commands.
- `./tools/bootstrap_env.sh` can be rerun any time after setup to revalidate the active toolchain.
- `UART0` is reserved for console, boot logs, flashing recovery, and panic output during early bring-up.
- `platform_smoke` is the default bring-up case name for step 2.
- `clock_monotonicity` is a separate device-side smoke banner emitted after the hardware clock passes both task-context and interrupt-context checks.
- `GDBStub` is enabled in the firmware configuration so interactive serial debugging remains available without requiring JTAG for this stage.
- `PlatformIO`, Arduino, JTAG, and OpenOCD are intentionally out of scope for step 1.
