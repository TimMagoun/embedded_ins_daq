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
- **Specs and Plans:** Treat specs and implementation plans as engineering artifacts, not prose summaries. They must lock in architecture boundaries, behavioral contracts, and verification strategy before implementation starts.

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
- **Architecture for testability:** For any nontrivial subsystem, prefer explicit separation into:
  - **Core:** platform-agnostic data structures and algorithms
  - **Interfaces:** narrow contracts for clocks, queues, byte sources, file sinks, and status/fault reporting
  - **Adapters:** ESP32/FreeRTOS bindings on target and deterministic host implementations in tests
- **Hot-path adapters:** Keep adapters as thin translation layers only. They must not absorb chunking policy, fault policy, serialization, checksum logic, or session semantics. Prefer batch handoff, avoid repeated payload copies, and avoid indirect dispatch in per-byte or ISR-adjacent paths unless measured and justified.
- **ISR Strict Rules:**
  - Must use `IRAM_ATTR` and FreeRTOS `_FromISR` APIs.
  - **NEVER** use blocking functions, logging (`ESP_LOGx`), or float math inside an ISR.

### 6. Testing & Quality Gates

- **Frameworks:** Decouple algorithms from the HAL.
- **Testing Philosophy:** Write atomic tests. High line coverage is insufficient; tests must verify interface contracts, boundary conditions, and fault paths.
- **Test naming:** Name tests `Should<Postcondition>Given<Precondition>` so the single behavior under test is obvious from the identifier alone.
- **Verification depth:** For specs, plans, and implementations, require verification of:
  - happy paths
  - boundary conditions (`N-1`, `N`, `N+1`; just-below / exactly-at / just-above thresholds)
  - illegal lifecycle transitions
  - repeated start/stop or re-entry cycles
  - post-fault invariants
  - multi-source interleaving and ordering
  - on-device resource headroom and fault observability
- **Hardware validation:** Do not accept vague hardware checks like “looks correct” or “file contains data.” Hardware verification must include concrete scenarios, explicit pass/fail criteria, and where applicable an offline comparison against known truth data.
- **Smoke Cases:** `platform_smoke` is the canonical bring-up case (logs identity, port mappings, monotonicity, and periodic UART0 health).
- **Commit Workflow:** Unless scoped down by the user, enforce this exact order before committing:
  1. `uv run pre-commit run --all-files`
  1. `ctest` (targeted or full host)
  1. `uv run python3 tools/check_host_coverage.py`
  1. `./tools/run_cppcheck.sh --strict`
- **Review Deliverable:** When closing a task or PR, summarize the specific contracts verified, fault paths exercised, and any blind spots. "Tests pass" is not an acceptable summary.

### 7. Planning Lessons Learned

- **Do not stop at “use C++” or “separate concerns” as abstract guidance.** The plan must name the concrete file layout that enforces the separation. In this repository, the first-pass plan was too vague because core logic and ESP-IDF integration still lived in the same module files. The corrected approach is to split into `core/`, `interfaces/`, and `adapters/{host,esp32}/`.
- **Do not treat adapter boundaries as automatically free.** The first-pass plan named adapters but did not constrain hot-path behavior. The corrected plan requires thin translation-only adapters, batch-oriented handoff, minimal ISR work, and explicit avoidance of repeated payload copying or per-byte virtual dispatch.
- **Do not accept shallow verification sections.** The first-pass plan had basic happy-path checks and coarse hardware observations. The corrected approach requires detailed behavioral matrices for host tests and explicit device scenarios covering edge conditions, illegal transitions, saturation, storage faults, and post-fault invariants.
- **Do not leave hardware validation qualitative.** The corrected plan requires explicit pass/fail criteria plus offline comparison against known generator truth data for bytes, records, trigger counts, and sync-edge counts.
