# Implementation Step 11
## Sensor Preparation And Readiness

**Goal:** add pre-record sensor setup and readiness gating without moving sensor-specific logic into the capture-critical path.

**Depends on:** Step 10

## Write This Software

- implement `sensor_manager` and the shared lifecycle contract
- implement `sensor_driver_raw_uart` as the fallback policy
- implement `sensor_driver_gnss_ubx` for the `u-blox` receiver on `PORT1`
- implement a first IMU driver for the UART IMU on `PORT2`
- support bounded retries, timeouts, and `required_for_start` gating
- emit sensor-state records into the binary log during preparation
- keep all driver behavior on the pre-record control path only

## Desktop Validation

- unit test the sensor lifecycle state machine
- unit test timeout and retry behavior for both success and failure cases
- unit test `required_for_start` gating and raw-capture-only fallback behavior
- unit test driver selection from `SensorProfile`

## Device Integration Validation

- start a session with both sensors connected and verify both reach `READY`
- disconnect either required sensor and verify session start is blocked cleanly
- verify successful preparation writes sensor-state records before active recording begins
- verify once recording starts, raw UART logging remains authoritative even if a driver later misbehaves

## What We Can Execute After This Step

- `python3 -m tools.run_case --case sensor_prepare_success`
- `python3 -m tools.run_case --case sensor_prepare_missing_required`
- `./tools/parse_binary_log.py artifacts/latest/device/sensor_prepare_success/session.bin`

## Exit Criteria

- the logger has a normalized startup contract for heterogeneous sensors
- required sensors gate recording without contaminating the active capture path
