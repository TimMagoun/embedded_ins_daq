# Implementation Step 13
## Optional Framing

**Goal:** add non-authoritative framing metadata only after the raw logger is already robust.

**Depends on:** Step 12

## Write This Software

- implement `framing_service` on copied UART chunks only
- add initial framing support for `UBX` and `NMEA` on the GNSS path
- add a simple line-based or packet-boundary framer for the UART IMU only if useful for diagnostics
- emit framing metadata records without changing raw UART record generation
- add overload handling so framing can degrade independently from capture and storage

## Desktop Validation

- unit test `UBX` and `NMEA` framing state machines using captured byte streams
- unit test malformed or truncated frame handling
- unit test independent overload behavior so raw logging stays healthy when framing falls behind

## Device Integration Validation

- log the real `u-blox` GNSS stream with framing enabled and verify both raw and frame-metadata records appear
- verify framing can be disabled entirely without affecting raw logging
- overload the framing path and verify the device still preserves raw UART records and surfaces framing degradation visibly

## What We Can Execute After This Step

- `python3 -m tools.run_case --case ubx_nmea_framing`
- `python3 -m tools.run_case --case framing_overload_isolation`
- `./tools/parse_binary_log.py artifacts/latest/device/ubx_nmea_framing/session.bin`

## Exit Criteria

- optional protocol awareness exists as metadata
- the project still behaves like a logger first and a parser second
