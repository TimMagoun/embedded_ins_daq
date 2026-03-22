# Implementation Step 07
## Two-Port Reference Capture

**Goal:** extend the logger to the first real reference configuration: `u-blox` GNSS on `PORT1` and UART IMU on `PORT2`.

**Depends on:** Step 06

## Write This Software

- extend `uart_hal_adapter` and `uart_capture_service` from one port to two ports
- add per-port buffer allocation, stats, and fault accounting
- add a reference config profile for the UART IMU on `PORT2`
- verify that mixed baud rates and UART settings are supported independently per port
- keep ports `3` and `4` disabled so this step stays focused on the real initial hardware
- extend artifact parsing so host tools report bytes and record counts per port

## Desktop Validation

- unit test per-port isolation in queues, counters, and chunk descriptors
- unit test mixed-port configuration parsing and validation
- unit test that overflow on one port does not corrupt the other port's counters or data path

## Device Integration Validation

- run concurrent logging from the `u-blox` GNSS on `PORT1` and the UART IMU on `PORT2`
- verify both ports produce UART records in the same session file
- verify per-port counters and file contents match the active sensors
- soak for a longer dwell than the single-port test and confirm no cross-port contamination

## What We Can Execute After This Step

- `./tools/device/run_case.sh --case two_port_reference_capture`
- `./tools/device/collect_artifacts.sh --case two_port_reference_capture`
- `./tools/parse_binary_log.py artifacts/latest/device/two_port_reference_capture/session.bin`

## Exit Criteria

- the logger supports the first meaningful hardware deployment
- the reference GNSS and IMU combination can be captured together to SD with isolated per-port accounting
