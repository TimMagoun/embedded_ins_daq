# Implementation Plan Overview

## ESP32-P4 Nano Robust UART Sensor Logger

This folder turns the software architecture into a staged implementation plan with small, atomic milestones.

The plan is optimized for one outcome first: a robust embedded sensor logger that preserves raw UART data, timestamps timing events correctly, and makes loss and storage faults explicit.

______________________________________________________________________

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

______________________________________________________________________

## Command Contract

Each stage should add to a stable automation surface. Prefer direct `idf.py`, `ctest`, and small helper scripts only where they add real value such as environment setup, artifact capture, or repeatable device orchestration. By the end of the relevant stage we should be able to run commands in this shape:

- `./tools/bootstrap_env.sh`
- `idf.py build`
- `ctest --test-dir <host_build_dir>` or another documented host test command
- `idf.py flash monitor`
- `python3 -m tools.run_case --case <case_name>` if a device-side runner is needed for timeouts, resets, flashing, or artifact capture
- `./tools/parse_binary_log.py <session.bin>`

These commands define the intended workflow shape. Wrapper scripts are optional unless they provide capabilities that direct tool invocation does not.

______________________________________________________________________

## Stage Order

1. [01_development_environment_and_debug.md](01_development_environment_and_debug.md)
1. [02_platform_bring_up_and_clock.md](02_platform_bring_up_and_clock.md)
1. [03_config_session_and_faults.md](03_config_session_and_faults.md)
1. [04_binary_records_and_staging.md](04_binary_records_and_staging.md)
1. [05_single_port_raw_capture.md](05_single_port_raw_capture.md)
1. [06_single_port_sd_logger.md](06_single_port_sd_logger.md)
1. [07_two_port_reference_capture.md](07_two_port_reference_capture.md)
1. [08_four_port_scaling_and_loss_handling.md](08_four_port_scaling_and_loss_handling.md)
1. [09_sync_input_capture.md](09_sync_input_capture.md)
1. [10_trigger_output.md](10_trigger_output.md)
1. [11_sensor_preparation_and_readiness.md](11_sensor_preparation_and_readiness.md)
1. [12_observability_local_control_and_soak.md](12_observability_local_control_and_soak.md)
1. [13_optional_framing.md](13_optional_framing.md)

______________________________________________________________________

## Definition Of Progress

A stage is complete only when:

- the named software modules for that stage exist
- the desktop unit tests for that stage pass locally
- the on-device integration test for that stage passes on the ESP32-P4 Nano
- the required artifacts are saved automatically for later debugging
- the stage leaves the system in a usable, reviewable state

This sequencing intentionally produces a useful logger before it produces a feature-complete logger.
