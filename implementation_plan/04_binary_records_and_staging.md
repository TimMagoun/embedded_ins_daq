# Implementation Step 04
## Binary Records And RAM Staging

**Goal:** define the authoritative binary record format and prove that records can be built and staged without SD writes yet.

**Depends on:** Step 03

## Write This Software

- implement `record_builder` for session, UART, fault, sensor-state, SYNC, and trigger record types
- define the binary file header, record envelope, versioning scheme, and integrity fields
- implement `binary_log_pipeline` with fixed-capacity RAM staging buffers
- add a RAM-backed sink so record ordering and flush boundaries can be tested before SD integration
- dump the RAM-backed sink to an artifact file using the same binary format later written to SD so host tools can parse it unchanged
- add a small host-side binary log parser used by tests and later artifact inspection
- make pipeline overflow produce a normalized fault rather than silent loss

## Desktop Validation

- golden tests for record encode and decode
- unit tests for record ordering and complete-record flush boundaries
- unit tests for pipeline-full behavior and fault generation
- unit tests that truncated or malformed records are detected by the parser

## Device Integration Validation

- run a synthetic session on the device that emits session, fault, and dummy UART records into the RAM pipeline
- verify the pipeline can open, append, flush to the RAM sink, close, and export a parseable `session.bin` artifact
- verify overflow counters and fault events are emitted when the synthetic producer exceeds the configured staging capacity

## What We Can Execute After This Step

- `./tools/run_host_tests.sh`
- `./tools/device/run_integration.sh --case synthetic_record_pipeline`
- `./tools/parse_binary_log.py artifacts/latest/device/synthetic_record_pipeline/session.bin`

## Exit Criteria

- the binary log contract exists early and is testable
- later capture work can target a stable record and staging interface instead of inventing one ad hoc
