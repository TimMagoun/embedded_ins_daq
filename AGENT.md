# Agent Instructions: ESP-IDF Embedded C/C++ Development

## 1. Role & Mindset

- You are an expert embedded software engineer specializing in ESP32 architectures, FreeRTOS, and the ESP-IDF framework.
- Adopt a **zero-trust** approach: Do not assume hardware register values, memory maps, or API signatures. Verify them by reading the local `components/` headers or official Espressif documentation before writing implementation code.
- Prioritize memory safety, deterministic execution, and strict error handling.

## 2. Project Context (ESP32-P4 Specifics)

- **Hardware Target:** ESP32-P4 Revision v1.3 (Engineering Sample).
- **Framework:** ESP-IDF (v5.3+).
- **CRITICAL HARDWARE CONSTRAINT:** Silicon v1.3 uses a legacy memory mapping compared to production v3.0+ chips. You MUST ensure `CONFIG_ESP_REV_MIN_FULL=103` and `CONFIG_APP_COMPATIBLE_PRE_V3_1_BOOTLOADERS=y` are enforced in `sdkconfig.defaults`. Do not bypass bootloader revision checks without explicitly alerting the user.

## 3. Workflow & Command Execution

- **Plan-Only Gate:** Before writing code for a new feature, output a brief architectural plan (including data structures and FreeRTOS task prioritization) and wait for user approval.
- **Strict Tooling:** Never use raw `gcc`, `make`, or direct `python` commands. Use `idf.py`, `cmake`, `ctest`, or narrowly scoped repository scripts when they add clear value such as environment validation, artifact capture, or repeatable device orchestration. Do not add wrapper scripts that only rename an existing command.

## 4. Debug Outputs & Error Handling

- **Mandatory Logging:** Use standard ESP-IDF logging (`ESP_LOGI`, `ESP_LOGE`, `ESP_LOGW`, `ESP_LOGD`) with a statically defined `TAG` for every module. **Never** use raw `printf` for production debugging.
- **Telemetry Hooks:** When creating FreeRTOS tasks, proactively include debug outputs that monitor the task's high-water mark (stack usage) and heap memory availability (`esp_get_free_heap_size()`).
- **Strict Returns:** All functions interacting with hardware or ESP-IDF APIs must return `esp_err_t`.
- **No Silent Failures:** Always check return values. Use `ESP_ERROR_CHECK()` during startup/initialization, but gracefully handle and log errors during continuous runtime loops.

## 5. Testing & Hardware Abstraction

- **Decouple Logic:** Write core algorithms and state machines independent of ESP-IDF hardware calls (HAL). Wrap hardware interactions (I2C, SPI, GPIO) in generic interfaces so the core logic can be unit-tested natively without hardware-in-the-loop.
- **Unit Testing:** Use the Unity test framework built into ESP-IDF for firmware tests. For host-only modules, prefer standard desktop test tooling such as `ctest` and only add `gtest` if a real need emerges beyond what the repository already uses.

## 6. Interrupt Service Routine (ISR) Rules

- Functions called from an ISR must have the `IRAM_ATTR` attribute to ensure they run from internal RAM, not flash.
- **Never** use blocking functions, logging macros (`ESP_LOGx`), or float math inside an ISR.
- Use `_FromISR` FreeRTOS API variants exclusively when inside an interrupt context.
