# Step 07: Trigger/Sync Capture

**Objective:** Implement trigger output timestamping and sync edge capture with rising/falling metadata.

**Files:**

- Create: `main/interfaces/sync_capture_interface.hpp`
- Create: `main/core/trigger_sync_capture_types.hpp`
- Create: `main/core/trigger_sync_capture_core.hpp`
- Create: `main/core/trigger_sync_capture_core.cpp`
- Create: `main/include/trigger_sync_capture.hpp`
- Create: `main/trigger_sync_capture.cpp`
- Create: `main/adapters/esp32/esp32_sync_gpio_adapter.hpp`
- Create: `main/adapters/esp32/esp32_sync_gpio_adapter.cpp`
- Create: `main/adapters/host/host_sync_capture_adapter.hpp`
- Create: `main/adapters/host/host_sync_capture_adapter.cpp`
- Modify: `main/include/daq_config.hpp`
- Modify: `main/include/daq_types.hpp`
- Create: `host_tests/test_trigger_sync_capture_core.cpp`

**Implementation Notes:**

- Use GPIO ISR capture for sync input edges in v1.
- Timestamp ISR work items from the shared monotonic clock and hand them off through fixed-capacity `_FromISR` queues.
- Timestamp trigger events when the output pulse is issued in the high-priority path.
- Ignore pre-start events for file logging while still proving the arming path works in `ready`.
- Include event class and edge fields in the emitted timing-event records.
- Put event classification, session-boundary filtering, and record generation in `trigger_sync_capture_core.cpp`.
- Keep `trigger_sync_capture.cpp` as a thin adapter-facing coordinator.
- `esp32_sync_gpio_adapter.cpp` should isolate GPIO ISR registration and edge handoff.
- `host_sync_capture_adapter.cpp` should drive synthetic trigger/sync events through the same interface contract.
- Keep ISR work items compact and fixed-size so the adapter does not copy large structures or execute nontrivial branching in interrupt context.
- Do not put event classification, edge interpretation policy, or session-boundary logic into the ISR or ESP32 adapter layer.
- Use `extern "C"` only for ISR entry points or GPIO callback functions that ESP-IDF registers through a C ABI.
- The ISR body should hand off to adapter code immediately and must not contain capture policy.

**Native Verification:**

- Host tests for:
  - trigger event packaging
  - rising and falling sync edge packaging
  - pre-start event discard behavior
  - `_FromISR` queue-full path escalates to fault
  - event timestamps are passed through unchanged to record descriptors
  - core behavior is invariant whether adapters deliver one event at a time or grouped task-level batches
  - trigger and sync records at timestamps exactly equal to `session_start`
  - alternating rising/falling edges preserve edge metadata and ordering
  - post-stop events are ignored
  - first fault wins on ISR queue overflow
  - near-simultaneous trigger and sync events preserve arrival order as delivered to the core
- Command:
  - `ctest --test-dir build_host --output-on-failure -R trigger_sync_capture_core`

**On-Device Hardware Verification:**

- Connect the mock rig trigger/sync lines and drive both rising-only and bidirectional sync patterns.
- Execute explicit scenarios:
  - pre-start trigger and sync activity
  - one trigger pulse during `running`
  - repeated triggers at maximum intended rate
  - rising/falling sync edge bursts
  - near-coincident trigger and sync edges
  - induced ISR queue saturation in a debug build
- Expected result:
  - Trigger records appear when pulses are issued.
  - Sync records preserve rising vs falling edge identity.
  - Logic analyzer comparison shows timestamp accuracy within the hardware validation budget.
  - No pre-start or post-stop timing events appear in the session file.
  - Saturation faults instead of dropping timing events silently.

**Exit Criteria:**

- Timing-event capture is validated independently before queue convergence and file writing.
