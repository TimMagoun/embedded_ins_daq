# Implementation Step 01
## Development Environment And Debug Workflow

**Goal:** make the project easy for humans and agents to build, flash, test, and debug without manual IDE steps.

**Why first:** every later step depends on fast feedback from both desktop tests and the real board.

## Framework Decision

- use native `ESP-IDF` as the firmware framework and build system for the `ESP32-P4 Nano`
- use the `ESP-IDF`-provided `FreeRTOS`, driver stack, `esp_timer` or `gptimer`, `sdmmc`, panic decoder, and `GDBStub` support rather than adding Arduino or another abstraction layer
- keep firmware as a standard `idf.py` project so the board support, linker scripts, flashing tools, and debug helpers remain aligned with the `ESP32-P4` SDK
- treat `PlatformIO` and Arduino compatibility as out of scope for revision 1 because this project depends on direct access to `ESP32-P4` peripherals, memory placement controls, and bring-up features that are best supported first in `ESP-IDF`

## Platform Compatibility

- target the `Waveshare ESP32-P4-NANO` built around the `ESP32-P4NRW32`
- assume the firmware runs on the `ESP32-P4` HP cores and uses the onboard `16 MB` flash, onboard `32 MB` PSRAM, onboard `USB-C` programming/debug path, and onboard TF slot in native `SDMMC` mode
- keep `UART0` reserved for console, boot logs, flashing recovery, and panic output during early bring-up
- use standard `ESP32-P4` UART peripherals for sensor ports and avoid the LP UART for the capture path
- keep the onboard `ESP32-C6`, Ethernet, and USB OTG features out of the initial firmware scope unless a later stage requires them

## Write This Software

- keep the repository as a standard `ESP-IDF` project rooted at the repo top level and add `tools/` only for narrowly scoped helpers that improve portability or repeatability
- keep `sdkconfig.defaults` at the repo root and document the exact `ESP-IDF` target selection for `esp32p4`
- add `./tools/bootstrap_env.sh` to verify the required `ESP-IDF` version, validate toolchain prerequisites, and confirm that either `IDF_PATH` is set or a documented repo-local install path such as `.local/esp-idf` exists
- document a portable environment contract that works on another developer or agent machine without hardcoded absolute paths
- use `idf.py build` as the default firmware build entry point rather than wrapping it with another script
- use Unity as the default firmware test framework and keep host-test support limited to hardware-independent modules that justify a desktop test target
- use `ctest` or another documented desktop test command for host-side logic instead of requiring a dedicated `run_host_tests.sh` wrapper
- add a device runner script only if direct `idf.py flash monitor` is insufficient for repeatable resets, named cases, timeouts, or automatic artifact capture
- add automatic artifact capture for serial logs, decoded crashes, test metadata, and copied session files
- add a non-GUI crash workflow that saves raw panic output, runs the `ESP-IDF` backtrace decoder against the built `elf`, and stores a readable backtrace
- keep `UART0` as the default console and recovery path during bring-up
- make `ESP-IDF` `GDBStub` the interactive debug mechanism for the first autonomous debug loop and keep `JTAG` and `OpenOCD` out of scope for this step
- document the exact run surfaces `idf.py build`, `idf.py flash`, `idf.py monitor`, `ctest --test-dir <host_build_dir>` when host tests exist, and an optional device runner command if direct `idf.py` invocation proves too limited
- define the first-line debug flow as `serial boot log -> ready banner timeout -> saved panic log -> decoded backtrace -> GDBStub session when interactive inspection is needed`

## Desktop Validation

- verify `./tools/bootstrap_env.sh` reports missing prerequisites clearly
- verify `./tools/bootstrap_env.sh` fails clearly if the active `ESP-IDF` does not support `esp32p4`
- verify `./tools/bootstrap_env.sh` can guide a new developer or agent to a working `ESP-IDF` setup without hardcoded machine-specific paths
- verify `idf.py build` produces a firmware build in a clean workspace
- verify the chosen host test command can run at least one trivial host test and write results to a documented artifact location when host tests exist
- verify the artifact collector creates deterministic case folders and does not overwrite previous runs silently
- verify the crash decoder can turn a saved panic log plus firmware `elf` into a readable report

## Device Integration Validation

- flash a minimal smoke-test firmware image to the ESP32-P4 Nano
- capture boot logs automatically through the `USB-C` serial console
- verify either direct `idf.py flash monitor` or the optional device runner can reset the board, wait for a ready banner, and fail on timeout
- verify the chosen device execution path records the exact firmware image, `sdkconfig`, monitor log, and decoded panic output for the case
- verify a developer can reproduce the same build and monitor session manually with `idf.py` even if helper scripts are absent or fail
- verify all device-run artifacts land in `artifacts/latest/device/<case_name>`

## What We Can Execute After This Step

- `./tools/bootstrap_env.sh`
- `idf.py build`
- `ctest --test-dir <host_build_dir>`
- `idf.py flash monitor`
- `./tools/device/run_case.sh --case board_smoke`

## Exit Criteria

- the repository states unambiguously that revision 1 firmware is built on native `ESP-IDF` for `esp32p4`
- another agent can clone the repo, run one bootstrap command, and get both host and device feedback
- another agent can build and debug directly through documented `ESP-IDF` commands without guessing hidden IDE settings or machine-specific paths
- every failed device run leaves behind enough logs to debug the failure without reproducing it interactively
