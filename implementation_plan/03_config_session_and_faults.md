# Implementation Step 03
## Config, Session, And Fault Foundations

**Goal:** make startup, session state, and fault handling deterministic before any real capture starts.

**Depends on:** Step 02

## Write This Software

- implement `config_manager` for loading and validating a structured startup configuration
- define `RuntimeConfig`, `SessionInfo`, and `FaultEvent` contracts in code
- implement `session_controller` with the architecture state machine
- implement `fault_manager` with normalized fault classes and severities
- generate unique session IDs and session filename bases
- prevent session start when configuration is invalid or storage prerequisites are missing
- emit binary-ready and human-readable event objects for session and fault transitions

## Desktop Validation

- unit test valid and invalid configuration cases
- unit test session-state transitions, including idempotent stop and fault handling
- unit test that `required_for_start` and SYNC mode conflicts are rejected in configuration validation
- unit test fault deduplication and severity mapping rules

## Device Integration Validation

- boot with a valid config file and verify the system reaches `READY`
- boot with an invalid config file and verify the system reports the error and blocks start
- request session start and stop through a temporary console path and verify state transitions
- inject a synthetic fault and verify it changes state only when severity rules require it

## What We Can Execute After This Step

- `./tools/run_host_tests.sh`
- `./tools/device/run_integration.sh --case config_valid_boot`
- `./tools/device/run_integration.sh --case config_invalid_boot`
- `./tools/device/run_integration.sh --case session_state_machine`

## Exit Criteria

- session lifecycle is centralized in one place
- configuration errors fail early and visibly
- every later module can publish faults through one normalized path
