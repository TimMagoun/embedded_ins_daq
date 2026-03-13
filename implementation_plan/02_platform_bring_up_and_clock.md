# Implementation Step 02
## Platform Bring-Up And Clock

**Goal:** establish the firmware skeleton, board assumptions, and the single monotonic device clock.

**Depends on:** Step 01

## Write This Software

- create the firmware module layout that matches the architecture boundaries
- implement `clock_service` with a single microsecond-resolution monotonic source
- add a board configuration layer for the ESP32-P4 Nano with explicit placeholder mappings for ports `1` and `2`
- add startup banners that print firmware version, build ID, and active board profile
- add internal-RAM placement annotations or wrappers for ISR-visible structures
- add a tiny runtime health banner that proves the scheduler and console path are alive
- document the provisional pin plan for `UART0`, `PORT1`, `PORT2`, and the SD interface

## Desktop Validation

- unit test clock monotonicity with a fake timer backend
- unit test wraparound assumptions and timestamp conversion helpers
- unit test board-profile validation so bad pin maps fail before firmware build or session start

## Device Integration Validation

- boot the board and verify the firmware prints the build ID and board profile
- verify `clock_now_us()` increases monotonically over repeated reads
- verify the clock is safe to read from both normal task context and an interrupt-driven smoke path
- verify the console remains usable after several resets

## What We Can Execute After This Step

- `./tools/device/run_integration.sh --case platform_smoke`
- `./tools/device/run_integration.sh --case clock_monotonicity`

## Exit Criteria

- the repository has a stable firmware skeleton
- the system has one authoritative device clock that later records can trust
- ports `1` and `2` have a documented provisional mapping for continued bring-up
