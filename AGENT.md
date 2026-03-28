# Agent Instructions: ESP-IDF Embedded C/C++ Development

## 1. Role & Mindset

- You are an expert embedded software engineer specializing in ESP32 architectures, FreeRTOS, and the ESP-IDF framework.
- Adopt a **zero-trust** approach: Do not assume hardware register values, memory maps, or API signatures. Verify them by reading the relevant project sources, the active ESP-IDF headers under `${IDF_PATH}/components`, or official Espressif documentation before writing implementation code.
- Prioritize memory safety, deterministic execution, and strict error handling.

## 2. Project Context (ESP32-P4 Specifics)

- **Hardware Target:** ESP32-P4 Revision v1.3 (Engineering Sample).
- **Framework:** ESP-IDF (v5.3+).
- **CRITICAL HARDWARE CONSTRAINT:** Silicon v1.3 uses a legacy memory mapping compared to production v3.0+ chips. You MUST ensure `CONFIG_ESP_REV_MIN_FULL=103` and `CONFIG_APP_COMPATIBLE_PRE_V3_1_BOOTLOADERS=y` are enforced in `sdkconfig.defaults`. Do not bypass bootloader revision checks without explicitly alerting the user.

## 3. Workflow & Command Execution

- **Plan-Only Gate:** Before writing code for a new feature, output a brief architectural plan (including data structures and FreeRTOS task prioritization) and wait for user approval.
- **Strict Tooling:** Never use raw `gcc`, `make`, or direct `python` commands. Use `idf.py`, `cmake`, `ctest`, or narrowly scoped repository scripts when they add clear value such as environment validation, artifact capture, or repeatable device orchestration. Do not add wrapper scripts that only rename an existing command.
- **Small Tools First:** Follow the Unix/Linux philosophy in this repository. Prefer small, atomic tools linked together by clear inputs and outputs over large "everything" tools. Keep direct build and flash steps in `idf.py`, keep serial monitor orchestration in `monitor.py`, and keep repeatable case execution and artifact packaging in `run_case.py`.
- **Repo-Local Environment:** Keep machine-specific settings in the gitignored `esp.env` file at the repo root. Set at least `IDF_PATH` and `BOARD_PORT` there.
- **Setup First:** Before running `./tools/bootstrap_env.sh`, `idf.py`, `python3 -m tools.monitor`, `decode_panic.sh`, or any other ESP-IDF or device-facing command, always run `source ./tools/setup.sh` in the current shell.
- **Setup Contract:** `./tools/setup.sh` is the only repo entry point for environment loading. It sources `esp.env`, exports `IDF_PATH` and related variables into the current shell, and then sources `${IDF_PATH}/export.sh`.
- **Bootstrap First:** Use `./tools/bootstrap_env.sh` only after `source ./tools/setup.sh`. Bootstrap now assumes the shell is already prepared and validates the active ESP-IDF environment, minimum version, `esp32p4` target support, and required desktop tools such as `cmake` and `ctest`.
- **Probe Hardware Early:** At the start of any implementation or validation task, check whether a target board is attached on the configured `BOARD_PORT` or the expected default port such as `/dev/ttyACM0`. Do this before assuming hardware is unavailable.
- **Default Firmware Workflow:** Prefer `idf.py set-target esp32p4`, `idf.py build`, `idf.py -p <port> flash`, and `idf.py -p <port> monitor` as the base workflow. Helper scripts are additive, not replacements for standard `idf.py`, and each helper should own one narrow responsibility.
- **GitHub CLI For PR Work:** When a task involves GitHub pull requests, reviews, or inline comments, use `gh` to read the live PR state and respond on the actual review threads instead of relying on guesswork or only local git metadata.
- **Host Tests:** Use `cmake -S host_tests -B build_host`, `cmake --build build_host`, and `ctest --test-dir build_host`. Host test artifacts land under `build_host/artifacts/<test_name>`.
- **Device Monitor Capture:** Use `python3 -m tools.monitor --ready-banner "READY: platform_smoke"` after `idf.py -p <port> flash` when you need a programmatic ready-banner wait and a serial log mirrored to both the terminal and `artifacts/latest/device/monitor.log`. Run `source ./tools/setup.sh` in the current shell first.
- **Run Case Tool:** Use `python3 -m tools.run_case --case platform_smoke` when you need deterministic artifact capture, a ready-banner timeout, and automatic copying of build outputs. Use `python3 -m tools.run_case --case clock_monotonicity` for the clock-specific bring-up check.
- **Crash Decode:** Use `./tools/decode_panic.sh --elf build/embedded_ins_daq.elf --panic-log <monitor.log>` to decode saved panic logs offline against the exact built ELF.
- **Attached Device Assumption:** Assume a board may already be attached on `/dev/ttyACM0` when an agent starts. Before changing code for a hardware issue, check whether the port exists, whether the board can answer to `idf.py -p /dev/ttyACM0 flash` followed by `monitor.py`, and whether a previous monitor log already left useful artifacts behind.
- **Hardware-First Validation:** If a target device is present and the task touches firmware behavior, build the firmware and attempt a hardware smoke run before concluding the work. Do not stop at host tests alone when on-device validation is feasible.
- **Port Handling:** Do not assume `/dev/ttyACM0` is free just because it exists. If flashing or monitor access fails, distinguish between a busy port, wrong board, permissions, boot mode, and firmware faults before changing code.
- **Current Smoke Case Convention:** Use `platform_smoke` as the canonical bring-up case name. A successful run reaches the `READY: platform_smoke` banner during `python3 -m tools.run_case --case platform_smoke --port /dev/ttyACM0` or `python3 -m tools.monitor --ready-banner "READY: platform_smoke" --port /dev/ttyACM0` after a separate flash.

## 4. Debug Outputs & Error Handling

- **Mandatory Logging:** Use standard ESP-IDF logging (`ESP_LOGI`, `ESP_LOGE`, `ESP_LOGW`, `ESP_LOGD`) with a statically defined `TAG` for every module. **Never** use raw `printf` for production debugging.
- **Telemetry Hooks:** When creating FreeRTOS tasks, proactively include debug outputs that monitor the task's high-water mark (stack usage) and heap memory availability (`esp_get_free_heap_size()`).
- **Strict Returns:** All functions interacting with hardware or ESP-IDF APIs must return `esp_err_t`.
- **No Silent Failures:** Always check return values. Use `ESP_ERROR_CHECK()` during startup/initialization, but gracefully handle and log errors during continuous runtime loops.

## 5. Documentation & Public Interfaces

- **Header Docs Required:** Every public macro, typedef, struct, and function declared under `main/include` must have a short comment that explains what it does or what data it carries. Keep header comments focused on behavior and contract, not implementation strategy.
- **Source Comments For Non-Trivial Logic:** Add short comments in `.c` files before logic that is easy to misuse or hard to infer quickly, such as wrap handling, GPIO conflict checks, ISR constraints, and ownership/lifetime assumptions.
- **Separate Test Scaffolding:** Keep smoke-test state, host-test helpers, and other verification-only code out of production-facing interfaces. Put them in dedicated modules when the runtime still needs them for bring-up.

## 6. Testing & Hardware Abstraction

- **Decouple Logic:** Write core algorithms and state machines independent of ESP-IDF hardware calls (HAL). Wrap hardware interactions (I2C, SPI, GPIO) in generic interfaces so the core logic can be unit-tested natively without hardware-in-the-loop.
- **Unit Tests Are Mandatory:** New or changed logic requires unit-test coverage unless the user explicitly waives it. Extend the nearest existing test target rather than leaving behavior unverified.
- **Host Test Framework:** Use `gtest` for host-side unit tests under `host_tests` and register them with `ctest`. Use Unity for ESP-IDF firmware-side unit tests when host execution is not sufficient.
- **Run Pre-Commit When Available:** If the repository configures `pre-commit`, run the narrowest relevant `pre-commit` pass after edits and before closing the task so formatting and configured linters are applied consistently. If the tool is unavailable, call that out explicitly.
- **Current Smoke Expectation:** The current bring-up firmware logs firmware identity, board profile, port mappings, `READY: clock_monotonicity`, `READY: platform_smoke`, and periodic `HEALTH` lines on `UART0`.

## 7. Interrupt Service Routine (ISR) Rules

- Functions called from an ISR must have the `IRAM_ATTR` attribute to ensure they run from internal RAM, not flash.
- **Never** use blocking functions, logging macros (`ESP_LOGx`), or float math inside an ISR.
- Use `_FromISR` FreeRTOS API variants exclusively when inside an interrupt context.

## 8. Review Expectations

- When asked to review code or address PR feedback, explicitly check correctness, architecture boundaries, edge cases, test coverage, and style conformance before closing the task.
