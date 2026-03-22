# Implementation Step 12
## Observability, Local Control, And Soak

**Goal:** make the logger operationally robust for long unattended runs and agent-driven debugging.

**Depends on:** Step 11

## Write This Software

- implement `status_log_pipeline` and `health_monitor`
- add local button and console control through `local_control_service`
- add human-readable summaries for queue depth, SD latency, per-port counts, and sensor readiness
- add periodic checkpoints and status events that explain degraded operation before total failure
- add automated long-run integration cases and artifact retention rules
- extend the debug workflow so failed device tests automatically preserve console logs, config, firmware build ID, and copied session files
- add a repeatable recovery workflow for SD faults, boot failures, and unclean shutdowns

## Desktop Validation

- unit test status-event formatting and rate limiting
- unit test health-threshold evaluation and warning generation
- unit test console command parsing and start or stop intent translation
- unit test host-side artifact summarization so soak failures are easy to inspect

## Device Integration Validation

- run at least `100` repeated start or stop cycles through the console path and at least `25` through the physical control path with no state-machine deadlock
- run a long-duration soak test of at least `2 hours` with the GNSS and IMU attached and require zero unexplained resets, zero silent-stop conditions, and zero lost-artifact cases
- inject SD faults or forced stalls and verify the logger enters `DEGRADED` when persistence is impaired but still partially functional, and enters `FAULTED` when authoritative binary logging can no longer continue
- verify each failing run leaves enough artifacts for offline diagnosis without rerunning immediately

## What We Can Execute After This Step

- `./tools/device/run_integration.sh --case local_control_smoke`
- `./tools/device/run_integration.sh --case long_soak_reference`
- `./tools/device/run_integration.sh --case sd_fault_recovery`
- `./tools/device/collect_artifacts.sh --case long_soak_reference`

## Exit Criteria

- the system is practical to operate and debug over long runs
- degraded and faulted persistence behavior is visible and testable in automation
- another agent can trigger tests, inspect artifacts, and understand failures with minimal manual intervention
