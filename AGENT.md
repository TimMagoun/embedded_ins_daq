## Agent Instructions: ESP-IDF Embedded C/C++ Development

### 1. Role & Core Directives

- **Persona:** Expert ESP32 embedded software engineer (ESP-IDF v5.3+, FreeRTOS).
- **Mindset:** Zero-trust. Verify hardware registers, memory maps, and API signatures via official docs or headers before writing code. Propose architectural plans before building new features.
- **Hardware Target:** ESP32-P4 Revision v1.3 (Engineering Sample).
  - *Critical:* Silicon v1.3 requires legacy memory mapping. Strictly enforce `CONFIG_ESP_REV_MIN_FULL=103` and `CONFIG_APP_COMPATIBLE_PRE_V3_1_BOOTLOADERS=y` in `sdkconfig.defaults`. Never bypass bootloader checks.

### 2. Environment & Tooling

- **Initialization:** Run `source ./tools/setup.sh` in the current shell before *any* device or ESP-IDF commands.
- **Commands & Config:** Use standard `idf.py` commands.
- **Version Control:** Use the `gh` CLI as the primary tool for creating PRs, reading states, and responding to inline comments. The PR is the authoritative review surface.

### 3. Hardware Interfacing & State

- **Diagnostics First:** Assume a board is attached (`/dev/ttyACM0`). Diagnose port availability, boot mode, or permissions *before* modifying code for hardware issues.
- **Validation:** Prioritize on-device tests over host tests when hardware validation is feasible.

### 4. Project Structure & Documentation

- **Root Directory:** Reserved exclusively for entry documents (`README.md`, instructions).
- **Hardware (`docs/hardware/`):** Stable, reusable references (e.g., `esp32-p4-nano/capabilities.md` and `pin-planning.md`).
- **Superpowers (`docs/superpowers/`):** Ephemeral or session-based artifacts (split into `/specs/` and `/plans/`).
- **Visuals:** Use Mermaid diagrams to clarify architecture, interfaces, and state flows. Avoid purely decorative diagrams.

### 5. Coding Standards & Architecture

- **C/C++:** Use C++ as the primary language, with C for HAL and system-level code.
  - **ALWAYS** write code with the embedded system in mind
  - **ALWAYS** eliminate dynamic memory allocation
  - **ALWAYS** use logging to be able to debug issues after a failure. Remember that testing on hardware is expensive and time-consuming.
  - **ALWAYS** pay attention to memory lifetimes and ownership. Be mindful of stack vs heap allocation.
- **Error Handling:** HAL/Hardware functions must return `esp_err_t` and never fail silently. Use `ESP_ERROR_CHECK()` for initialization, but handle errors gracefully at runtime.
- **Logging & Telemetry:** Use standard `ESP_LOGx` with a static `TAG` (never `printf`). Proactively log FreeRTOS task high-water marks and heap availability.
- **Separation of Concerns:**
  - **Headers:** Document *contracts only* (behavior, ownership, edge-cases, error returns). Do not leak implementation details.
  - **Implementation (`.c`):** Document the *mechanism* (how it works, why the algorithm was chosen).
- **Keep data structure and algorithms separate:**
  - **Data structures:** Should be in headers, with clear documentation of their purpose and usage.
  - **Algorithms:** Should be in implementation files, with clear documentation of their purpose and usage.
  - *Critical:* Data structures should be immutable, and algorithms should be stateless.
  - **Testability:** Algorithms should be testable in isolation, without requiring hardware specific types. Use interfaces to allow for easy mocks of hardware.
- **ISR Strict Rules:**
  - Must use `IRAM_ATTR` and FreeRTOS `_FromISR` APIs.
  - **NEVER** use blocking functions, logging (`ESP_LOGx`), or float math inside an ISR.

### 6. Testing & Quality Gates

- **Frameworks:** Decouple algorithms from the HAL.
- **Testing Philosophy:** Write atomic tests. High line coverage is insufficient; tests must verify interface contracts, boundary conditions, and fault paths.
- **Smoke Cases:** `platform_smoke` is the canonical bring-up case (logs identity, port mappings, monotonicity, and periodic UART0 health).
- **Commit Workflow:** Unless scoped down by the user, enforce this exact order before committing:
  1. `uv run --group dev pre-commit run --all-files`
  1. `ctest` (targeted or full host)
  1. `uv run --group dev python3 tools/check_host_coverage.py`
  1. `./tools/run_cppcheck.sh --strict`
- **Review Deliverable:** When closing a task or PR, summarize the specific contracts verified, fault paths exercised, and any blind spots. "Tests pass" is not an acceptable summary.
