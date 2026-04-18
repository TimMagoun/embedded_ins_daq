# Step 09: SD Writer

**Objective:** Own session file lifecycle, write the header first, append blocks, and fail fast on any storage error.

**Files:**

- Create: `main/interfaces/storage_file_interface.hpp`
- Create: `main/core/sd_writer_core.hpp`
- Create: `main/core/sd_writer_core.cpp`
- Create: `main/include/sd_writer.hpp`
- Create: `main/sd_writer.cpp`
- Create: `main/adapters/esp32/esp32_sdmmc_file_adapter.hpp`
- Create: `main/adapters/esp32/esp32_sdmmc_file_adapter.cpp`
- Create: `main/adapters/host/host_file_adapter.hpp`
- Create: `main/adapters/host/host_file_adapter.cpp`
- Modify: `main/include/daq_config.hpp`
- Create: `host_tests/test_sd_writer_core.cpp`

**Implementation Notes:**

- Mount SDMMC/FATFS and create a normal filesystem file per session.
- On `start_session`, create the file and write the fixed header before accepting write blocks.
- On `stop_session`, flush and close cleanly.
- Treat open, write, flush, close, and lifecycle misuse as irrecoverable faults.
- Keep file semantics in this module only; packet semantics stay in `storage_mux`.
- Put lifecycle state transitions, header-before-record enforcement, and error mapping in `sd_writer_core.cpp`.
- Keep `sd_writer.cpp` as a wrapper over `StorageFileInterface`.
- `esp32_sdmmc_file_adapter.cpp` should isolate FATFS/SDMMC specifics.
- `host_file_adapter.cpp` should support deterministic host-side file and failure simulation.
- Hand fixed write blocks to the file adapter as-is; do not re-slice or reserialize records inside the adapter.
- Keep file adapters free of session policy beyond the narrow file-operation contract they implement.
- If a narrow C ABI is still needed for target integration, keep it confined to the ESP32 adapter layer.

**Native Verification:**

- Host tests with a file-backed or mocked writer for:
  - header-before-record ordering
  - append of multiple blocks
  - stop flush and close
  - open failure fault
  - mid-session write failure fault
  - double-start or stop-without-start lifecycle fault
  - file adapter write segmentation does not change core lifecycle behavior or fault mapping
  - fault during header write
  - fault during final flush
  - fault during close
  - post-fault additional writes are rejected
  - stop without queued data still produces a valid header-only file if that is the chosen behavior
- Command:
  - `ctest --test-dir build_host --output-on-failure -R sd_writer_core`

**On-Device Hardware Verification:**

- Insert a formatted SD card and run a short capture session.
- Execute explicit scenarios:
  - start/stop with no records
  - short mixed-data session
  - long enough session to force many block appends
  - card removed before start
  - card removed during header write or early running phase if reproducible
  - card removed during sustained write phase
- Expected result:
  - Session file is created under the expected path.
  - File starts with the v1 header.
  - Removing the card mid-session or forcing an I/O error transitions firmware to `faulted`.
  - No further file writes occur after the fault is latched.

**Exit Criteria:**

- Persistent storage behavior is correct and fail-fast before full application wiring.
