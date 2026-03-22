# Implementation Step 09
## SYNC Input Capture

**Goal:** add timestamped SYNC edge capture without disturbing the proven raw UART logging path.

**Depends on:** Step 08

## Write This Software

- implement `sync_hal_adapter` for per-port SYNC input configuration
- implement `sync_capture_service` with an interrupt-driven event queue
- add `SYNC_EDGE` record generation through `record_builder`
- enforce the per-port SYNC mode state so a pin cannot be input capture and trigger output simultaneously
- add per-port SYNC counters and overflow faults
- start with `PORT1` and `PORT2` hardware validation before enabling all four ports

## Desktop Validation

- unit test edge normalization, polarity handling, and timestamp propagation
- unit test SYNC queue overflow behavior
- unit test invalid mode transitions and configuration conflicts

## Device Integration Validation

- inject a known pulse train into a SYNC input on the active reference setup
- verify each accepted edge produces a log record with port, polarity, and timestamp
- verify UART capture remains healthy while SYNC events are being logged concurrently
- compare logged edge counts against the stimulus source and require exact count agreement for a `10,000`-edge nominal test with zero `SYNC_QUEUE_OVERFLOW` faults
- measure timestamp deltas against the stimulus source and require observed timestamp error no worse than `1 us` relative to the device clock measurement method used by the fixture

## What We Can Execute After This Step

- `./tools/device/run_case.sh --case sync_input_capture`
- `./tools/parse_binary_log.py artifacts/latest/device/sync_input_capture/session.bin`

## Exit Criteria

- SYNC timestamps are captured without polling
- timing events are added as independent records without weakening raw UART logging
