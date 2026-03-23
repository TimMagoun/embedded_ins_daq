# Implementation Step 08

## Four-Port Scaling And Loss Handling

**Goal:** generalize the proven two-port logger to the full four-port architecture and make overload behavior explicit.

**Depends on:** Step 07

## Write This Software

- extend the port tables, capture paths, and config handling to all four sensor ports
- preallocate per-port buffers sized to satisfy the `500 ms` retention target at configured baud rates
- add explicit `UART_OVERFLOW`, `UART_BUFFER_EXHAUSTED`, and `BINARY_PIPELINE_OVERFLOW` handling
- add health counters and watermarks for queue depth, buffer usage, and staged-bytes backlog
- ensure overload and retention-exhaustion paths can latch degraded health without bypassing the `session_controller` lifecycle model
- keep the fast path allocation-free during active recording
- add a synthetic traffic fixture for `PORT3` and `PORT4` so full-scale stress tests do not depend on four real sensors

## Desktop Validation

- unit test buffer-sizing math at the highest supported baud rate
- unit test full-system loss accounting with simulated SD stalls and capture bursts
- unit test that loss events are surfaced even when exact byte loss cannot be counted precisely

## Device Integration Validation

- run four active UART inputs using two real sensors plus two deterministic generators or loopback fixtures
- verify logging continues while storage lags until retention is exhausted
- deliberately overload the pipeline and verify explicit loss records are present in both counters and logs
- verify overload can force degraded health while the session remains in `RECORDING` until authoritative persistence is no longer defensible
- confirm the system does not silently discard required records

## What We Can Execute After This Step

- `python3 -m tools.run_case --case four_port_stress`
- `python3 -m tools.run_case --case four_port_overload_faults`
- `./tools/parse_binary_log.py artifacts/latest/device/four_port_stress/session.bin`

## Exit Criteria

- the logger meets its core multi-port capture goal
- retention limits and overload failures are measurable and visible
