# Step 10: App Integration

**Objective:** Wire the control plane and data plane into a runnable firmware application with pinned tasks and clear ownership boundaries.

**Files:**

- Create: `main/include/daq_app.hpp`
- Create: `main/daq_app.cpp`
- Modify: `main/app_main.cpp`
- Modify: `main/CMakeLists.txt`
- Create: `host_tests/test_interface_contracts.cpp`
- Create: `host_tests/test_host_adapters.cpp`
- Create: `host_tests/test_daq_app.cpp`

**Implementation Notes:**

- Instantiate:
  - `StateManager`
  - `StatusFaultHub`
  - one `MonotonicClock`
  - per-port `UartCapture`
  - per-port `TriggerSyncCapture`
  - one `StorageMux`
  - one `SdWriter`
- Create tasks matching the design:
  - `capture_task` on the capture-oriented HP core
  - `storage_mux_task` on the storage/control-oriented HP core
  - `sd_writer_task` on the storage/control-oriented HP core
- Keep control commands coarse and out of the hot path.
- Ensure all module start/stop/fault callbacks line up with the state machine’s authority.
- Keep orchestration in C++ objects with explicit ownership and static allocation.
- Compose the application from `core` units plus injected `interfaces` implemented by either `adapters/esp32` or `adapters/host`.
- Keep `daq_app.cpp` free of direct algorithm logic; it should wire dependencies, ownership, task lifetimes, and command flow only.
- Preserve batching at task boundaries:
  - capture-side adapter drains bytes/events, then invokes core logic
  - storage-side wrapper forwards complete blocks to the writer adapter
- Do not insert extra queues, copies, or abstract dispatch layers on a hot path unless profiling data justifies them.
- `app_main` remains the primary required `extern "C"` symbol in this step.
- Any FreeRTOS task entry shims may use `extern "C"` if needed by the exact API signature, but the task bodies should delegate immediately into C++ methods or free functions.

**Native Verification:**

- Host tests for:
  - interface contract conformance between core expectations and host adapters
  - host adapter behavior for deterministic clocks, file writes, and injected queue pressure
  - startup order
  - start-session wiring
  - stop-session wiring
  - propagated fault causes from any module
  - no record acceptance outside `running`
  - adapter substitution does not change core-observable behavior for the same event batches
  - repeated session cycles do not leak state across runs
  - fault during start-session transitions to terminal fault and aborts later work
  - fault during stop-session transitions to terminal fault if the spec requires fail-fast close handling
  - first fault wins across competing module failures
  - post-fault adapters may still receive stimuli but the app emits no new records or writes
  - high-watermark/status telemetry paths do not alter capture behavior
- Command:
  - `ctest --test-dir build_host --output-on-failure -R daq_app`

**On-Device Hardware Verification:**

- Flash and run a start/stop session without the external mock rig, then with it connected.
- Execute integrated scenarios:
  - clean start/stop with no external data
  - pre-start traffic then start
  - mixed four-port UART + trigger + sync traffic
  - stop while traffic is active
  - forced source-queue saturation
  - forced writer-queue saturation
  - SD removal during active capture
  - fault injection followed by further external traffic to confirm terminal behavior
- Expected result:
  - Boot reaches `ready`.
  - Starting a session creates a file and begins accepting records.
  - Stopping a session closes the file and returns to `ready`.
  - Injected module faults route to `faulted` and halt further capture.
  - Task high-water marks and heap logs remain within acceptable margins during stress.
  - Queue watermark logs show headroom until intentional fault scenarios.

**Exit Criteria:**

- The firmware runs the complete v1 architecture on target hardware.
