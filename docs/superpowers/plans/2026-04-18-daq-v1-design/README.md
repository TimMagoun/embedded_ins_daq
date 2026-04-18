# DAQ V1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to execute these step files in order.

**Goal:** Build the first ESP32-P4 DAQ prototype that captures UART, trigger, and sync events into a single SD-card binary log with microsecond timestamps and fail-fast fault handling.

**Architecture:** The implementation is split into a low-latency data plane and a coarse-grained control plane, and each subsystem is further split into `core`, `interfaces`, and `adapters`. Core units own data structures and algorithms only; interfaces define platform-neutral contracts; adapters bridge those contracts to ESP-IDF on target and lightweight fakes on host.

**Tech Stack:** ESP-IDF 5.x, C++ for firmware modules, narrow C HAL shims where required, FreeRTOS tasks and queues, GPTimer, UART driver events, GPIO ISRs, SDMMC/FATFS, host-side CTest.

______________________________________________________________________

## Working Assumptions

- Run `source ./tools/setup.sh` before any ESP-IDF or host-test commands.
- Create firmware sources under `main/` and host tests under `host_tests/`.
- Use `UART0` only for console and recovery; sensor ports start at `UART1`.
- Treat all queue overflows and storage failures as irrecoverable faults.
- Keep ISR-facing state in internal SRAM and ISR entry points IRAM-safe where required.
- Default all new firmware modules to C++ (`.cpp` / `.hpp`) and reserve C for HAL-facing or ESP-IDF linkage boundaries only.

## C++ / C Boundary Rules

- Prefer C++ for control-plane logic, data structures, serialization, queue orchestration, and testable algorithms.
- Use C only where ESP-IDF or low-level linkage makes it materially simpler:
  - `app_main`
  - ISR entry points registered through C-style callback APIs
  - thin HAL adapters around driver or ROM-facing APIs if a pure-C boundary is cleaner
- Any function that must be visible to C code should be declared inside an `extern "C"` block in headers and defined with matching linkage in implementation files.
- Keep `extern "C"` scopes narrow. Do not wrap whole C++ classes or entire headers unless the file is intentionally a C ABI surface.

## Core / Interface / Adapter Rules

- `core/` contains platform-agnostic data structures and algorithms only.
- `interfaces/` contains abstract contracts, immutable value types, and narrow transport APIs used by core logic.
- `adapters/host/` contains host fakes, file-backed implementations, and deterministic test harness glue.
- `adapters/esp32/` contains ESP-IDF, FreeRTOS, GPTimer, GPIO, UART, and SDMMC bindings.
- `core/` files must not include ESP-IDF headers, FreeRTOS headers, or direct OS driver types.
- Host tests should target `core/` first, then selected `interfaces/` contract tests, then adapter smoke tests only where needed.
- If a module needs a queue, clock read, byte source, or file sink, define that as an interface and inject it into the core algorithm rather than calling ESP-IDF directly.

## Adapter Performance Rules

- Adapters must be thin translation layers, not secondary policy engines.
- Keep hot-path adapters one-way where possible: convert external events into compact core inputs, call the core, and return.
- Avoid virtual dispatch in per-byte, per-edge, or ISR-adjacent loops unless measurement shows it is safe.
- Prefer batch handoff across boundaries:
  - drain UART bytes into a fixed buffer, then call the core once for the batch
  - enqueue compact sync/trigger work items, then process them in task context
  - assemble storage blocks in core, then hand fixed blocks to the file adapter
- Avoid buffer copies at adapter boundaries. Pass spans, fixed-buffer views, indices, or references into statically owned storage when lifetimes permit.
- Keep ISR bodies minimal:
  - timestamp
  - capture compact metadata
  - enqueue work
  - return
- Do not put serialization, queue policy, fault policy, checksum generation, or session logic in ESP32 adapters.
- If a hot-path abstraction appears in profiling, specialize that adapter boundary locally instead of collapsing the core/interface split.
- Any future change that adds per-byte copying, dynamic allocation, or indirect dispatch in a hot adapter path must include a measurement note and explicit justification.

## Proposed File Layout

- `main/app_main.cpp`
- `main/CMakeLists.txt`
- `main/include/c_linkage.h`
- `main/include/daq_types.hpp`
- `main/include/daq_config.hpp`
- `main/include/daq_faults.hpp`
- `main/include/daq_status.hpp`
- `main/include/validate_config.hpp`
- `main/include/daq_app.hpp`
- `main/interfaces/clock_interface.hpp`
- `main/interfaces/queue_interface.hpp`
- `main/interfaces/uart_interface.hpp`
- `main/interfaces/sync_capture_interface.hpp`
- `main/interfaces/storage_file_interface.hpp`
- `main/interfaces/status_sink_interface.hpp`
- `main/interfaces/session_control_interface.hpp`
- `main/core/state_manager_types.hpp`
- `main/core/state_manager_core.hpp`
- `main/core/state_manager_core.cpp`
- `main/core/monotonic_time_core.hpp`
- `main/core/monotonic_time_core.cpp`
- `main/core/record_format_types.hpp`
- `main/core/record_format_core.hpp`
- `main/core/record_format_core.cpp`
- `main/core/record_checksum_core.hpp`
- `main/core/record_checksum_core.cpp`
- `main/core/uart_capture_types.hpp`
- `main/core/uart_capture_core.hpp`
- `main/core/uart_capture_core.cpp`
- `main/core/trigger_sync_capture_types.hpp`
- `main/core/trigger_sync_capture_core.hpp`
- `main/core/trigger_sync_capture_core.cpp`
- `main/core/storage_mux_core.hpp`
- `main/core/storage_mux_core.cpp`
- `main/core/sd_writer_core.hpp`
- `main/core/sd_writer_core.cpp`
- `main/state_manager.cpp`
- `main/validate_config.cpp`
- `main/status_fault_hub.cpp`
- `main/daq_app.cpp`
- `main/adapters/esp32/esp32_clock_adapter.hpp`
- `main/adapters/esp32/esp32_clock_adapter.cpp`
- `main/adapters/esp32/esp32_uart_adapter.hpp`
- `main/adapters/esp32/esp32_uart_adapter.cpp`
- `main/adapters/esp32/esp32_sync_gpio_adapter.hpp`
- `main/adapters/esp32/esp32_sync_gpio_adapter.cpp`
- `main/adapters/esp32/esp32_sdmmc_file_adapter.hpp`
- `main/adapters/esp32/esp32_sdmmc_file_adapter.cpp`
- `main/adapters/esp32/esp32_queue_adapter.hpp`
- `main/adapters/esp32/esp32_queue_adapter.cpp`
- `main/adapters/host/host_clock_adapter.hpp`
- `main/adapters/host/host_clock_adapter.cpp`
- `main/adapters/host/host_uart_adapter.hpp`
- `main/adapters/host/host_uart_adapter.cpp`
- `main/adapters/host/host_sync_capture_adapter.hpp`
- `main/adapters/host/host_sync_capture_adapter.cpp`
- `main/adapters/host/host_file_adapter.hpp`
- `main/adapters/host/host_file_adapter.cpp`
- `main/adapters/host/host_queue_adapter.hpp`
- `main/adapters/host/host_queue_adapter.cpp`
- `host_tests/CMakeLists.txt`
- `host_tests/test_validate_config.cpp`
- `host_tests/test_state_manager_core.cpp`
- `host_tests/test_monotonic_time_core.cpp`
- `host_tests/test_record_format_core.cpp`
- `host_tests/test_record_checksum_core.cpp`
- `host_tests/test_uart_capture_core.cpp`
- `host_tests/test_trigger_sync_capture_core.cpp`
- `host_tests/test_storage_mux_core.cpp`
- `host_tests/test_sd_writer_core.cpp`
- `host_tests/test_interface_contracts.cpp`
- `host_tests/test_host_adapters.cpp`
- `host_tests/test_daq_app.cpp`

## Required `extern "C"` Surfaces

- `app_main` entry point in `main/app_main.cpp`
- ISR callback functions registered with GPIO or other ESP-IDF C APIs
- Any narrow C ABI shim that remains necessary inside `main/adapters/esp32/`
- Any C ABI helper exposed to host tests or C modules through `main/include/c_linkage.h`

## Ordered Steps

1. [01-project-skeleton.md](./01-project-skeleton.md)
1. [02-config-and-fault-contracts.md](./02-config-and-fault-contracts.md)
1. [03-state-manager.md](./03-state-manager.md)
1. [04-monotonic-clock-and-shared-types.md](./04-monotonic-clock-and-shared-types.md)
1. [05-binary-record-format.md](./05-binary-record-format.md)
1. [06-uart-capture.md](./06-uart-capture.md)
1. [07-trigger-sync-capture.md](./07-trigger-sync-capture.md)
1. [08-storage-mux.md](./08-storage-mux.md)
1. [09-sd-writer.md](./09-sd-writer.md)
1. [10-app-integration.md](./10-app-integration.md)
1. [11-end-to-end-hardware-validation.md](./11-end-to-end-hardware-validation.md)

## Global Verification Gates

- Native:
  - `cmake -S host_tests -B build_host`
  - `cmake --build build_host`
  - `ctest --test-dir build_host --output-on-failure`
- Hardware:
  - `idf.py build`
  - `idf.py -p /dev/ttyACM0 flash monitor`
- Full quality gates before merge:
  - `uv run --group dev pre-commit run --all-files`
  - `ctest --test-dir build_host --output-on-failure`
  - `uv run --group dev python3 tools/check_host_coverage.py`
  - `./tools/run_cppcheck.sh --strict`
