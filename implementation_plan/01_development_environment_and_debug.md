# Implementation Step 01
## Development Environment And Debug Workflow

**Goal:** make the project easy for humans and agents to build, flash, test, and debug without manual IDE steps.

**Why first:** every later step depends on fast feedback from both desktop tests and the real board.

## Framework Decision

- use native `ESP-IDF` as the firmware framework and build system for the `ESP32-P4 Nano`
- use the `ESP-IDF`-provided `FreeRTOS`, driver stack, `esp_timer` or `gptimer`, `sdmmc`, panic decoder, and `OpenOCD` integrations rather than adding Arduino or another abstraction layer
- keep firmware as a standard `idf.py` project so the board support, linker scripts, flashing tools, and debug helpers remain aligned with the `ESP32-P4` SDK
- treat `PlatformIO` and Arduino compatibility as out of scope for revision 1 because this project depends on direct access to `ESP32-P4` peripherals, memory placement controls, and bring-up features that are best supported first in `ESP-IDF`

## Platform Compatibility

- target the `Waveshare ESP32-P4-NANO` built around the `ESP32-P4NRW32`
- assume the firmware runs on the `ESP32-P4` HP cores and uses the onboard `16 MB` flash, onboard `32 MB` PSRAM, onboard `USB-C` programming/debug path, and onboard TF slot in native `SDMMC` mode
- keep `UART0` reserved for console, boot logs, flashing recovery, and panic output during early bring-up
- use standard `ESP32-P4` UART peripherals for sensor ports and avoid the LP UART for the capture path
- keep the onboard `ESP32-C6`, Ethernet, and USB OTG features out of the initial firmware scope unless a later stage requires them

## Write This Software

- create a standard `ESP-IDF` repository layout with `firmware/` as the `idf.py` project root, `host_tests/` for desktop tests, `tools/` for automation scripts, and `artifacts/` for captured outputs
- add `firmware/sdkconfig.defaults` and document the exact `ESP-IDF` target selection for `esp32p4`
- add `./tools/bootstrap_env.sh` to verify the required `ESP-IDF` version, install or validate toolchain prerequisites, source the `ESP-IDF` export script, and confirm `IDF_TARGET=esp32p4`
- add `./tools/build_firmware.sh` as the one-command firmware build entry point that delegates to `idf.py -C firmware build`
- add `./tools/run_host_tests.sh` as the one-command desktop unit test entry point for host-side logic that does not require hardware
- add `./tools/device/run_integration.sh` to call `idf.py -C firmware flash monitor` or equivalent non-interactive subcommands, reset the board, stream console output, and return pass or fail
- add automatic artifact capture for serial logs, decoded crashes, test metadata, and copied session files
- add a non-GUI crash workflow that saves raw panic output, runs the `ESP-IDF` backtrace decoder against the built `elf`, and stores a readable backtrace
- keep `UART0` as the default console and recovery path during bring-up
- make `JTAG` or `OpenOCD` support an optional extension, not a dependency for the first autonomous debug loop
- document the exact run surfaces:
  - build: `idf.py -C firmware build`
  - flash: `idf.py -C firmware flash`
  - monitor: `idf.py -C firmware monitor`
  - full automated device run: `./tools/device/run_integration.sh --case <case_name>`
- define the first-line debug flow as `serial boot log -> ready banner timeout -> saved panic log -> decoded backtrace -> optional JTAG follow-up`

## Desktop Validation

- verify `./tools/bootstrap_env.sh` reports missing prerequisites clearly
- verify `./tools/bootstrap_env.sh` fails clearly if the active `ESP-IDF` does not support `esp32p4`
- verify `./tools/build_firmware.sh` produces a firmware build in a clean workspace
- verify `./tools/run_host_tests.sh` can run at least one trivial host test and write results to `artifacts/latest/host`
- verify the artifact collector creates deterministic case folders and does not overwrite previous runs silently
- verify the crash decoder can turn a saved panic log plus firmware `elf` into a readable report

## Device Integration Validation

- flash a minimal smoke-test firmware image to the ESP32-P4 Nano
- capture boot logs automatically through the `USB-C` serial console
- verify the integration runner can reset the board, wait for a ready banner, and fail on timeout
- verify the integration runner records the exact firmware image, `sdkconfig`, monitor log, and decoded panic output for the case
- verify a developer can reproduce the same build and monitor session manually with `idf.py` if a wrapper script fails
- verify all device-run artifacts land in `artifacts/latest/device/<case_name>`

## What We Can Execute After This Step

- `./tools/bootstrap_env.sh`
- `./tools/build_firmware.sh`
- `./tools/run_host_tests.sh`
- `idf.py -C firmware build`
- `idf.py -C firmware flash monitor`
- `./tools/device/run_integration.sh --case board_smoke`

## Exit Criteria

- the repository states unambiguously that revision 1 firmware is built on native `ESP-IDF` for `esp32p4`
- another agent can clone the repo, run one bootstrap command, and get both host and device feedback
- another agent can build and debug either through the stable wrapper scripts or directly through `idf.py` without guessing hidden IDE settings
- every failed device run leaves behind enough logs to debug the failure without reproducing it interactively
