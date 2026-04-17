# embedded_ins_daq

Revision 1 firmware is built as a native `ESP-IDF` project for the `Waveshare ESP32-P4-NANO` targeting `esp32p4`.

This repository keeps the default firmware workflow on standard `ESP-IDF` commands:

- `idf.py build`
- `idf.py flash`
- `idf.py monitor`
- `cmake --build <native_build_dir>`
- `ctest --test-dir <native_build_dir>`

The repo is designed for in-container development. Open it in the devcontainer and the shell will already have ESP-IDF and Codex available.

Python developer tooling is managed with `uv`. Formatting and pre-commit hooks are configured for:

- Python with `black`
- shell with `shfmt`
- Markdown with `mdformat`
- C/C++ with `clang-format` using Google style
- C/C++ static analysis with `cppcheck`
- native unit tests with `gtest` orchestrated through `ctest`
- native test coverage with `gcovr` enforced above 90%

## Quickstart

### 1. Open the devcontainer

Use your editor's devcontainer support to open the repository in the container. The container image includes:

- ESP-IDF at `/opt/esp/idf`
- Codex CLI
- `uv`
- the basic host tools needed by this repo

Interactive shells inside the container automatically source the ESP-IDF environment.

### 2. Install the developer tooling

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

The repo hook suite also includes `cppcheck`, which expects the firmware build to
be configured so it can reuse `build/compile_commands.json` for the embedded pass.
For a broader local pass, run `./tools/run_cppcheck.sh --strict`.

### 3. Select the target

In a clean workspace, set the target once:

```bash
idf.py set-target esp32p4
```

The project defaults in [sdkconfig.defaults](/home/agent/workspace/embedded_ins_daq/sdkconfig.defaults) keep the ESP32-P4 v1.3 compatibility settings required by this board.

### 4. Build the firmware

```bash
idf.py build
```

The bring-up firmware emits clear ready banners on `UART0`, which remains the default console and panic-output path during bring-up.

### 5. Run the native smoke test

Configure the small native test target once:

```bash
cmake -S host_tests -B build_native
cmake --build build_native
ctest --test-dir build_native
```

The native test suite uses `gtest`, with `ctest` used as the top-level runner. Native test artifacts are written under `build_native/artifacts/<test_name>`.

Binary log fixtures can be decoded with:

```bash
python3 tools/parse_binary_log.py host_tests/fixtures/session_example.bin
```

### 6. Flash and monitor directly

Use standard `ESP-IDF` commands whenever you want the manual workflow:

```bash
idf.py -p /dev/ttyACM0 flash monitor
```

The first-line debug flow is:

```text
serial boot log -> ready banner timeout -> saved panic log -> decoded backtrace -> GDBStub session when interactive inspection is needed
```

### 7. Capture serial output to both the terminal and a log

Use the repo tool when you want a reusable, non-interactive log file while still seeing the live monitor stream:

```bash
idf.py -p /dev/ttyACM0 flash
python3 -m tools.monitor --ready-banner "READY: platform_smoke"
```

By default this uses `BOARD_PORT` from the devcontainer and writes the latest monitor log to `artifacts/latest/device/monitor.log`.
The tool passes `--disable-auto-color` to `idf.py monitor` so the saved log stays easy to parse while the same stream remains visible in the terminal.

### 8. Run the platform smoke case with artifact capture

When you want a repeatable case folder with logs and copied build outputs, use:

```bash
python3 -m tools.run_case --case platform_smoke
```

For the clock-specific bring-up check, use:

```bash
python3 -m tools.run_case --case clock_monotonicity
```

Artifacts land in:

- `artifacts/runs/device/<case_name>/<timestamp>`
- `artifacts/latest/device/<case_name>`

Each device case captures:

- the monitor log
- the active `sdkconfig`
- the built `elf`
- the built `bin` images when present
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

- Open the repository in the devcontainer before running ESP-IDF or device commands.
- `UART0` is reserved for console, boot logs, flashing recovery, and panic output during early bring-up.
- `platform_smoke` is the default bring-up case name for step 2.
- `clock_monotonicity` is a separate device-side smoke banner emitted after the hardware clock passes both task-context and interrupt-context checks.
- `GDBStub` is enabled in the firmware configuration so interactive serial debugging remains available without requiring JTAG for this stage.
- `PlatformIO`, Arduino, JTAG, and OpenOCD are intentionally out of scope for step 1.
