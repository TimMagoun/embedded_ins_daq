# Step 03: State Manager

**Objective:** Implement the authoritative `init -> ready -> running -> ready`
state machine with terminal `faulted`, driven only by incoming status and fault
events.

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
  - `state_manager_on_status(...)`
  - `state_manager_on_fault(...)`
  - `state_manager_get_state(...)`
- Emit coarse commands only: `arm`, `start_session`, `stop_session`, `fault_shutdown`.
- Reject illegal transitions deterministically and convert them into explicit fault reasons where the spec requires fail-fast behavior.
- Store readiness prerequisites inside the manager. Callers report facts such as config valid, storage mounted, start command, stop command, or storage removed; callers do not decide when `ready` has been reached.
- Capture the session start timestamp as state associated with the `running` transition, not inside the hot data path.
- Model this as a C++ type with explicit immutable event/value objects rather than a bag of C structs.
- Put the transition table and emitted command logic in `state_manager_core.cpp`.
- Keep `state_manager.cpp` as a thin orchestrator that adapts app-level calls to the pure core and forwards emitted commands through injected interfaces.
- Interpret `(origin, code)` pairs inside the manager so the event stream is sufficient to reconstruct both local module facts and global lifecycle transitions.
- No `extern "C"` needed in this step.

**Native Verification:**

- Host tests for:
  - valid boot path into `ready` only after all required readiness facts are observed
  - start/stop transitions driven by status events
  - no `start` before readiness prerequisites are satisfied
  - `start` while already `running` faults or rejects exactly per spec
  - `stop` while `ready` faults or rejects exactly per spec
  - storage removal while `ready` returns the manager to a non-ready state per spec
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
  - remove storage while `ready`
  - injected fault during `running`
- Expected result:
  - Boot reaches `ready`.
  - `start` moves to `running`.
  - `stop` returns to `ready`.
  - Illegal lifecycle requests produce the expected rejection or fault behavior.
  - Injected fault moves to `faulted` and blocks further commands permanently until reset.

**Exit Criteria:**

- Control-plane state semantics are stable before data-plane modules start emitting records.
