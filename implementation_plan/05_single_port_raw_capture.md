# Implementation Step 05
## Single-Port Raw Capture

**Goal:** capture raw UART data on `PORT1` reliably into preallocated buffers and publish UART records without involving SD yet.

**Depends on:** Step 04

## Write This Software

- implement `uart_hal_adapter` for one sensor port using interrupt-driven or DMA-backed receive
- implement `uart_capture_service` for one port with a fixed-capacity circular RX buffer
- add `UartChunkDescriptor` ownership rules and chunk publication into the binary pipeline
- add per-port statistics for bytes captured, chunks emitted, and overflow count
- add a simple reference configuration for the `u-blox` GNSS on `PORT1`
- keep raw capture authoritative even though no framing exists yet

## Desktop Validation

- unit test circular-buffer accounting and wrap behavior
- unit test chunk descriptor generation across split buffer regions
- unit test overflow detection and loss-counter increments
- unit test that the capture service never mutates bytes owned by a published chunk

## Device Integration Validation

- connect the `u-blox` GNSS or a deterministic UART fixture to `PORT1`
- capture raw bytes continuously for a fixed dwell period into the RAM-backed pipeline
- verify bytes captured is non-zero and overflow count stays at zero under nominal load
- verify the session can stop cleanly and preserve the final chunk boundaries

## What We Can Execute After This Step

- `./tools/device/run_integration.sh --case port1_raw_capture`
- `./tools/device/collect_artifacts.sh --case port1_raw_capture`

## Exit Criteria

- `PORT1` raw capture works without polling
- the logger can preserve the raw byte stream before SD and before any parser exists
