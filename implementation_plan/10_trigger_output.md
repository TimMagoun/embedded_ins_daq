# Implementation Step 10
## Trigger Output

**Goal:** generate logged trigger pulses with bounded jitter while concurrent logging remains active.

**Depends on:** Step 09

## Write This Software

- implement `trigger_engine` with validated rate and pulse-width configuration
- start with the simplest timing path that can meet requirements, then move to `ETM` only if measurements demand it
- generate one trigger record for every asserted pulse
- enforce the rule that a port cannot be both SYNC input and trigger output in the same session
- add trigger counters and scheduling fault reporting
- use the UART IMU on `PORT2` as the reference consumer for trigger-output validation

## Desktop Validation

- unit test trigger schedule calculations across supported rates and pulse widths
- unit test invalid configuration rejection and trigger record counting
- unit test scheduling-fault propagation through `fault_manager`

## Device Integration Validation

- generate trigger pulses on the configured IMU-facing port while raw logging continues on `PORT1` and `PORT2`
- verify trigger records appear in the binary log with the expected count
- measure trigger frequency and jitter under concurrent UART capture and SD logging and require worst-case jitter no worse than `5 us`
- run a `10,000`-pulse validation at representative rates including `1 Hz`, `100 Hz`, and `1 kHz`, with zero missing trigger records
- verify the configured port cannot be armed as both trigger output and SYNC input

## What We Can Execute After This Step

- `python3 -m tools.run_case --case trigger_output_basic`
- `python3 -m tools.run_case --case trigger_output_under_load`
- `./tools/parse_binary_log.py artifacts/latest/device/trigger_output_under_load/session.bin`

## Exit Criteria

- trigger generation is functional, logged, and measured under realistic load
- the project can drive a timing-sensitive UART IMU without sacrificing logger integrity
