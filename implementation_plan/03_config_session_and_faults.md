# Implementation Step 03
## Config, Session, And Fault Foundations

**Goal:** make startup, session state, and fault handling deterministic before any real capture starts.

**Depends on:** Step 02

## Write This Software

- implement `config_manager` for loading and validating a structured startup configuration
- define `RuntimeConfig`, `SessionInfo`, and `FaultEvent` contracts in code
- implement `session_controller` with the architecture state machine: `BOOT`, `CONFIG_INVALID`, `READY`, `STARTING`, `RECORDING`, `STOPPING`, `FAULTED`
- implement `fault_manager` with normalized fault classes and severities
- add a temporary `storage_readiness_provider` abstraction that reports whether session start is permitted before the real SD implementation exists
- generate unique session IDs and session filename bases
- prevent session start when configuration is invalid or when the `storage_readiness_provider` reports unavailable
- emit binary-ready and human-readable event objects for session and fault transitions
- define the control-plane contract that degraded health is tracked separately from the session lifecycle, even if the richer `health_monitor` implementation arrives later

Note for sequencing:

- in this step, `storage_readiness_provider` may be a deterministic stub used only to validate control-plane behavior
- the real SD-backed implementation replaces this stub in Step 06
- `READY` is the operator-visible idle state in revision 1; no separate persisted `ARMED` state is required

## Desktop Validation

- unit test valid and invalid configuration cases
- unit test session-state transitions, including idempotent stop and fault handling
- unit test that `required_for_start` and SYNC mode conflicts are rejected in configuration validation
- unit test fault deduplication and severity mapping rules
- unit test that warning or error conditions can latch degraded health without inventing a second lifecycle state

## Device Integration Validation

- boot with a valid config file and the temporary ready `storage_readiness_provider` stub and verify the system reaches `READY`
- boot with an invalid config file and verify the system reports the error and blocks start
- boot with the temporary unavailable `storage_readiness_provider` stub and verify the system blocks start without pretending storage is healthy
- request session start and stop through a temporary console path and verify state transitions
- inject a synthetic fault and verify it changes state only when severity rules require it
- inject a recoverable synthetic fault and verify the system can surface degraded health while remaining in `READY` or `RECORDING` as appropriate

## What We Can Execute After This Step

- `./tools/run_host_tests.sh`
- `./tools/device/run_integration.sh --case config_valid_boot`
- `./tools/device/run_integration.sh --case config_invalid_boot`
- `./tools/device/run_integration.sh --case storage_prereq_blocked`
- `./tools/device/run_integration.sh --case session_state_machine`

## Exit Criteria

- session lifecycle is centralized in one place
- configuration errors fail early and visibly
- control-plane storage gating is defined before the real SD implementation arrives
- every later module can publish faults through one normalized path
