# Step 04: Monotonic Clock And Shared Types

**Objective:** Add the canonical microsecond timebase and the shared record/work-item data structures used across modules.

**Files:**

- Create: `main/interfaces/clock_interface.hpp`
- Create: `main/core/monotonic_time_core.hpp`
- Create: `main/core/monotonic_time_core.cpp`
- Create: `main/include/monotonic_clock.hpp`
- Create: `main/adapters/esp32/esp32_clock_adapter.hpp`
- Create: `main/adapters/esp32/esp32_clock_adapter.cpp`
- Create: `main/adapters/host/host_clock_adapter.hpp`
- Create: `main/adapters/host/host_clock_adapter.cpp`
- Modify: `main/include/daq_types.hpp`
- Create: `host_tests/test_monotonic_time_core.cpp`

**Implementation Notes:**

- Wrap one `GPTimer` as the only timestamp source for UART, trigger, and sync capture.
- Define compact immutable record descriptors for:
  - UART chunk records
  - timing event records
  - ISR-to-task event work items
  - storage write blocks
- Keep timestamps in `uint64_t` microseconds throughout the pipeline.
- Put timestamp arithmetic, elapsed-time comparison, and idle-gap decision logic in `monotonic_time_core.cpp`.
- Keep the ESP32 GPTimer binding isolated in `adapters/esp32/esp32_clock_adapter.cpp`.
- Use `host_clock_adapter.cpp` for deterministic host-side time control.
- The public clock-facing code should depend on `ClockInterface`, not directly on GPTimer or FreeRTOS types.
- Keep the clock adapter read path minimal and allocation-free; it should expose timestamp reads without extra state copying or policy branching in the hot path.

**Native Verification:**

- Host tests for:
  - monotonic comparison helpers
  - idle-gap expiry calculations
  - record descriptor layout invariants
  - first-byte timestamp preservation rules
  - equality edge cases at idle-gap threshold boundary
  - large timestamp deltas without overflow in comparison helpers
  - zero-delta comparisons
  - ordering invariants for compact event descriptors
- Host Validation Gate:
  - Use the Host Validation Gate from [AGENT.md](/home/agent/workspace/embedded_ins_daq/AGENT.md#6-testing--quality-gates).
  - Use `ctest --test-dir build_host --output-on-failure -R monotonic_time_core` for the step-specific filter.

**On-Device Hardware Verification:**

- Flash firmware with a temporary periodic clock probe log.
- Capture clock samples while the device is otherwise idle and while UART/sync traffic is active.
- Expected result:
  - Successive sampled timestamps are strictly increasing.
  - Measured delta is close to the programmed probe interval.
  - No backwards or duplicated timestamps are observed under load.
  - No timer init failures appear in the console.

**Exit Criteria:**

- A single trusted timebase and shared data contracts exist before any capture logic depends on them.
