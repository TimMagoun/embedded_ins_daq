# Step 03: State Manager

**Objective:** Implement the authoritative `init -> ready -> running -> ready` state machine with terminal `faulted`.

**Files:**

- Create: `main/core/state_manager_types.hpp`
- Create: `main/core/state_manager_core.hpp`
- Create: `main/core/state_manager_core.cpp`
- Create: `main/interfaces/session_control_interface.hpp`
- Create: `main/include/state_manager.hpp`
- Create: `main/state_manager.cpp`
- Modify: `main/include/daq_types.hpp`
- Create: `host_tests/test_state_manager_core.cpp`

**Implementation Notes:**

- Expose a small event-driven interface:
  - `state_manager_init(...)`
  - `state_manager_on_config_result(...)`
  - `state_manager_start(...)`
  - `state_manager_stop(...)`
  - `state_manager_fault(...)`
  - `state_manager_get_state(...)`
- Emit coarse commands only: `arm`, `start_session`, `stop_session`, `fault_shutdown`.
- Reject illegal transitions deterministically and convert them into explicit fault reasons where the spec requires fail-fast behavior.
- Capture the session start timestamp as state associated with the `running` transition, not inside the hot data path.
- Model this as a C++ type with explicit immutable event/value objects rather than a bag of C structs.
- Put the transition table and emitted command logic in `state_manager_core.cpp`.
- Keep `state_manager.cpp` as a thin orchestrator that adapts app-level calls to the pure core and forwards emitted commands through injected interfaces.
- No `extern "C"` needed in this step.

**Native Verification:**

- Host tests for:
  - valid boot path into `ready`
  - start/stop transitions
  - no `start` before config success
  - `start` while already `running` faults or rejects exactly per spec
  - `stop` while `ready` faults or rejects exactly per spec
  - repeated `start -> stop -> start -> stop` cycles reset transient session state cleanly
  - no transition out of `faulted`
  - session start timestamp latched exactly once per run
  - session start timestamp equality handling is explicit when record timestamp equals `session_start`
  - first fault wins and later faults do not overwrite the latched reason
  - commands emitted by the core are exact and ordered: `arm`, `start_session`, `stop_session`, `fault_shutdown`
- Host Validation Gate:
  - Use the Host Validation Gate from [AGENT.md](/home/agent/workspace/embedded_ins_daq/AGENT.md#6-testing--quality-gates).
  - Use `ctest --test-dir build_host --output-on-failure -R state_manager_core` for the step-specific filter.

**On-Device Hardware Verification:**

- Expose temporary console commands or auto-sequenced hooks in `app_main()` to trigger `start` and `stop`.
- Exercise illegal lifecycle requests:
  - `start` before ready
  - `start` twice
  - `stop` twice
  - injected fault during `running`
- Expected result:
  - Boot reaches `ready`.
  - `start` moves to `running`.
  - `stop` returns to `ready`.
  - Illegal lifecycle requests produce the expected rejection or fault behavior.
  - Injected fault moves to `faulted` and blocks further commands permanently until reset.

**Exit Criteria:**

- Control-plane state semantics are stable before data-plane modules start emitting records.
