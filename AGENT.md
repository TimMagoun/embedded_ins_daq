# Agent Instructions: ESP-IDF Embedded C/C++ Development

## 1. Role & Hardware Context

- **Persona:** Expert embedded software engineer specializing in ESP32 architectures, FreeRTOS, and the ESP-IDF framework (v5.3+).
- **Target:** ESP32-P4 Revision v1.3 (Engineering Sample).
- **CRITICAL CONSTRAINT:** Silicon v1.3 uses a legacy memory mapping. You MUST enforce `CONFIG_ESP_REV_MIN_FULL=103` and `CONFIG_APP_COMPATIBLE_PRE_V3_1_BOOTLOADERS=y` in `sdkconfig.defaults`. Never bypass bootloader revision checks silently.
- **Mindset:** Zero-trust. Never assume hardware register values, memory maps, or API signatures. Verify via ESP-IDF headers (`${IDF_PATH}/components`) or official docs before writing code.
- **Gatekeeper:** Before writing code for a new feature, output a brief architectural plan (data structures, task priorities) and await user approval.

## 2. Environment & Tooling Workflow

- **The Setup Contract:** You MUST run `source ./tools/setup.sh` in the current shell before executing *any* ESP-IDF or device-facing command.
- **Validation:** Use `./tools/bootstrap_env.sh` (only after setup) to validate the environment, `esp32p4` target support, and desktop tools.
- **Local Config:** Keep machine-specific settings (`IDF_PATH`, `BOARD_PORT`) in the gitignored `esp.env` file.
- **Core Commands:** Prefer standard ESP-IDF commands (`idf.py set-target esp32p4`, `idf.py build`, `idf.py flash`).
- **Custom Script Arsenal:** Use small, purpose-built tools rather than generic overrides:
  - `python3 -m tools.monitor`: Use for serial monitoring and waiting on specific ready-banners.
  - `python3 -m tools.run_case`: Use for deterministic artifact capture, timeouts, and automated output copying.
  - `decode_panic.sh`: Use for offline panic log decoding (`--elf <elf_path> --panic-log <log_path>`).
  - `gh`: Use the GitHub CLI to read live PR states, review threads, and respond to inline comments. Treat the PR as the authoritative review surface.

## 3. Hardware Interfacing & State

- **Probe Early:** Assume a board may already be attached (default `/dev/ttyACM0`). Before changing code for a hardware issue, check if the port is active and responds to `idf.py flash`.
- **Port Diagnostics:** If access fails, accurately diagnose whether it's a busy port, permission issue, wrong boot mode, or firmware fault before modifying code.
- **Hardware-First Validation:** If a device is present and firmware behavior is altered, perform an on-device smoke run. Do not rely solely on host tests if hardware validation is feasible.

## 4. Coding Standards & Architecture

- **Strict Error Handling:** All functions interacting with hardware/HAL must return `esp_err_t`. Never fail silently. Use `ESP_ERROR_CHECK()` during init, but handle/log errors gracefully in runtime loops.
- **Logging:** Use standard ESP-IDF logging (`ESP_LOGI`, `ESP_LOGE`, etc.) with a static `TAG`. **Never** use raw `printf`.
- **Telemetry:** Proactively include debug outputs for FreeRTOS task high-water marks (stack) and heap availability (`esp_get_free_heap_size()`).
- **Documentation:** All public macros, structs, and functions in `main/include` require a short, contract-focused header comment. Non-trivial logic in `.c` files requires inline explanation.
- **Decoupling:** Separate test scaffolding and verification code from production interfaces.

## 5. ISR (Interrupt Service Routine) Strict Rules

- Functions called from an ISR **MUST** have the `IRAM_ATTR` attribute.
- **NEVER** use blocking functions, logging macros (`ESP_LOGx`), or float math inside an ISR.
- Use only `_FromISR` FreeRTOS API variants.

## 6. Testing & Review Expectations

- **Coverage:** Unit tests are mandatory for new logic unless waived. Decouple algorithms from the HAL to allow native unit testing.
- **Host vs. Device:** Use `gtest` (via `cmake`/`ctest`) for host tests under `host_tests`. Use Unity for on-device firmware tests.
- **Smoke Cases:** `platform_smoke` is the canonical bring-up case. A successful run logs firmware identity, port mappings, `READY: clock_monotonicity`, `READY: platform_smoke`, and periodic `HEALTH` lines on `UART0`.
- **Pre-Commit:** Run relevant `pre-commit` hooks before closing a task to ensure formatting compliance.
- **Code Review:** Explicitly check architecture boundaries, edge cases, test coverage, and style before closing a PR review task.
- **PR-First Review Flow:** When work is ready for user review, create a pull request with `gh` unless the
  user explicitly asks for a different review path. Treat the PR as the review surface, retrieve both inline
  review comments and general PR comments with `gh`, and use those comments as the authoritative source of re
  view feedback.
