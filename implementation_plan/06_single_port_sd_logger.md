# Implementation Step 06
## Single-Port SD Logger

**Goal:** turn the `PORT1` raw capture path into a useful logger by writing authoritative binary records to the SD card.

**Depends on:** Step 05

## Write This Software

- implement `sd_storage_service` for SDMMC mount, session-file creation, block writes, sync, and close
- connect `binary_log_pipeline` flush-ready blocks to the storage service
- create the binary session file and a minimal human-readable status file
- add unclean-termination detection in the binary file contract
- add bounded flush and sync policies so data-at-risk in RAM is explicit
- surface `SD_MOUNT_FAILURE`, `SD_WRITE_FAILURE`, and `SD_SYNC_FAILURE` through `fault_manager`

## Desktop Validation

- unit test file-header and file-close markers
- unit test truncated-session detection with host-side parser fixtures
- unit test storage backpressure handling with a mocked slow or failing sink
- unit test that partial record writes are never reported as valid complete records

## Device Integration Validation

- run a real `PORT1` logging session to the onboard TF card
- verify the binary file is created, readable, and contains session plus UART records
- verify the status file contains start, stop, and storage events
- run a controlled reset during or after a session and verify the next boot can detect an unclean prior session

## What We Can Execute After This Step

- `python3 -m tools.run_case --case port1_sd_logger`
- `./tools/parse_binary_log.py artifacts/latest/device/port1_sd_logger/session.bin`

## Exit Criteria

- the project has a usable single-port field logger
- SD writes are downstream of explicit staging buffers
- power-loss and storage-fault behavior is observable instead of ambiguous
