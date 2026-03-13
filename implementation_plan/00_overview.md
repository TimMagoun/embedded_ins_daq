# Implementation Plan Overview
## ESP32-P4 Nano Robust UART Sensor Logger

This folder turns the software architecture into a staged implementation plan with small, atomic milestones.

The plan is optimized for one outcome first: a robust embedded sensor logger that preserves raw UART data, timestamps timing events correctly, and makes loss and storage faults explicit.

---

## Planning Principles

- logger-first features come before convenience features
- every stage must leave the codebase in a testable state
- every stage must define both desktop unit validation and on-device integration validation
- every stage must end with concrete commands that another agent can execute without inventing a workflow
- ports `1` and `2` are the reference hardware during early bring-up
- port `1` is assumed to host a `u-blox` GNSS receiver
- port `2` is assumed to host a UART IMU with a trigger input
- ports `3` and `4` stay disabled until the multi-port scaling stage
- optional framing stays behind raw capture, storage, and timing integrity

---

## Command Contract

Each stage should add to a stable automation surface. The exact implementation may vary, but by the end of the relevant stage we should be able to run these commands:

- `./tools/bootstrap_env.sh`
- `./tools/build_firmware.sh`
- `./tools/run_host_tests.sh`
- `./tools/device/run_integration.sh --case <case_name>`
- `./tools/device/collect_artifacts.sh --case <case_name>`
- `./tools/parse_binary_log.py <session.bin>`

These commands are part of the plan. If the final repository chooses slightly different names, the same stable entry points should still exist.

---

## Stage Order

1. [01_development_environment_and_debug.md](01_development_environment_and_debug.md)
2. [02_platform_bring_up_and_clock.md](02_platform_bring_up_and_clock.md)
3. [03_config_session_and_faults.md](03_config_session_and_faults.md)
4. [04_binary_records_and_staging.md](04_binary_records_and_staging.md)
5. [05_single_port_raw_capture.md](05_single_port_raw_capture.md)
6. [06_single_port_sd_logger.md](06_single_port_sd_logger.md)
7. [07_two_port_reference_capture.md](07_two_port_reference_capture.md)
8. [08_four_port_scaling_and_loss_handling.md](08_four_port_scaling_and_loss_handling.md)
9. [09_sync_input_capture.md](09_sync_input_capture.md)
10. [10_trigger_output.md](10_trigger_output.md)
11. [11_sensor_preparation_and_readiness.md](11_sensor_preparation_and_readiness.md)
12. [12_observability_local_control_and_soak.md](12_observability_local_control_and_soak.md)
13. [13_optional_framing.md](13_optional_framing.md)

---

## Definition Of Progress

A stage is complete only when:

- the named software modules for that stage exist
- the desktop unit tests for that stage pass locally
- the on-device integration test for that stage passes on the ESP32-P4 Nano
- the required artifacts are saved automatically for later debugging
- the stage leaves the system in a usable, reviewable state

This sequencing intentionally produces a useful logger before it produces a feature-complete logger.
