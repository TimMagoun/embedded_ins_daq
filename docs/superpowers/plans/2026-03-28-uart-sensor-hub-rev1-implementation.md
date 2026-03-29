# UART Sensor Hub Rev1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the revision-1 boot-autonomous UART sensor hub on top of the existing ESP32-P4 bring-up baseline, adding config-driven session control, authoritative binary logging, UART capture, sync capture, trigger output, and SD-backed session artifacts.

**Architecture:** Use the existing codebase as input rather than ground truth. Keep the generic platform foundation that already matches the design, adapt the bring-up flow behind clearer interfaces, and allow runtime configuration to replace static board-description assumptions where needed. Reuse the earlier `implementation_plan/` stage order where it still matches the design, but explicitly separate stable platform services from provisional bring-up wiring in `main/app_main.c` and related modules.

**Tech Stack:** ESP-IDF 5.3+, C17 firmware, FreeRTOS, ESP32-P4 UART/GPIO/SDMMC drivers, host `gtest` via CMake/CTest, Python helper tooling under `tools/`

______________________________________________________________________

## Inherited Baseline

The current codebase already provides this platform baseline, which should be evaluated as the starting point for this plan:

- `main/app_main.c`
- `main/include/board_profile.h`
- `main/include/clock_probe.h`
- `main/include/clock_service.h`
- `main/include/clock_smoke.h`
- `main/include/platform_iram.h`
- `main/include/runtime_banner.h`
- `main/platform/board_profile.c`
- `main/platform/clock_service.c`
- `main/platform/clock_probe.c`
- `main/platform/clock_smoke.c`
- `main/platform/runtime_banner.c`
- `main/CMakeLists.txt`
- `host_tests/CMakeLists.txt`
- `host_tests/clock_host_test.cc`
- `host_tests/board_profile_host_test.cc`
- `tools/bootstrap_env.sh`
- `README.md`

Supporting repo baseline that remains in scope:

- `host_tests/smoke_host_test.c`
- `tools/run_case.py`

Evidence as of `2026-03-28`:

- host suite passes with `cmake -S host_tests -B build_host && cmake --build build_host && ctest --test-dir build_host --output-on-failure`
- `app_main` already initializes the clock service, validates the active board profile, starts the ISR clock smoke path, emits `READY: clock_monotonicity`, starts the periodic health banner task, and emits `READY: platform_smoke`
- the host baseline already covers monotonic clock helpers and board-profile validation invariants
- `README.md` and `tools/bootstrap_env.sh` already define the setup, validation, host-test, and platform-smoke workflow that later tasks must extend instead of replacing
- no config/session/fault, record-pipeline, UART capture, sync, trigger, or SD modules exist yet

## Baseline Evaluation

This plan does not assume every existing implementation choice should survive unchanged. Use these categories when implementing revision 1:

- keep: generic platform foundations that remain valid regardless of how runtime configuration is loaded, especially clock primitives, clock probe logic, clock smoke coverage, the host-test harness shape, and board-profile validation logic that is still useful as hardware-capability validation
- adjust: modules that are useful but currently too tied to bring-up semantics, especially `main/app_main.c`, runtime ready banners, and startup sequencing; these should become thin integration points around the session-control and storage boot path
- replace when revision-1 interfaces are ready: assumptions that make static board description the authority for active session behavior; runtime configuration loaded from SD or another control surface should become the canonical source for per-session enablement, baud rates, timing mode, and trigger or sync parameters

The key boundary is:

- hardware capability description may remain platform-owned
- session behavior must become runtime-config owned
- the interface between them must be explicit so future config sources such as SD files or a web control plane can feed the same validated runtime contract without reworking the capture or logging stack

## Scope Alignment

This plan intentionally covers the implementation-plan stages that still match the revision-1 design:

- fully inherited: `01_development_environment_and_debug`, `02_platform_bring_up_and_clock`
- selectively inherited from the current baseline: clock and board-validation primitives, the existing host-test harness, and bootstrap workflow documentation
- intentionally revisited from the current baseline: startup flow in `app_main`, ready-banner semantics, and the role of `board_profile_t` once runtime configuration exists
- implemented by this plan: `03_config_session_and_faults` through `10_trigger_output`
- partially implemented by this plan: `12_observability_local_control_and_soak`

This plan intentionally does not include these prior stages as revision-1 deliverables:

- `11_sensor_preparation_and_readiness`: excluded because the revision-1 design explicitly says session start must not block on sensor readiness
- `13_optional_framing`: excluded because framing is downstream, optional, and must not interfere with authoritative raw logging

This plan also excludes local manual control as a core deliverable for revision 1:

- no required button-driven control path
- no required console command interface beyond temporary bring-up hooks if needed for testing
- boot-autonomous startup from SD configuration is the primary operator workflow

## Revision-1 Acceptance Gates

Revision 1 is not complete unless the implementation demonstrates all of the following:

- four-port UART logging up to `921600` baud on configured active ports
- independent trigger-output schedules across up to four ports
- sync-input capture on configured edge directions
- explicit recording of loss, degradation, and storage faults
- one binary session log, one status log, and one copied configuration snapshot per session
- measured timing performance at real pins under representative combined UART and SD load

Timing is a go or no-go checkpoint, not polish:

- if measured sync-input or trigger-output timing cannot hold the required `±1 us` target under representative load, the outcome must be treated as an architecture decision point
- acceptable responses are to relax the timing claim, reduce combined load, or move to a stronger hardware timing path
- revision 1 must not silently preserve the claim without measurement evidence

## File Structure Map

Create or extend the runtime tree in this shape so module boundaries stay aligned with the architecture docs:

- keep `main/include/clock_*.h` and the existing clock-related `main/platform/*` modules as stable platform foundation
- treat `main/include/board_profile.h` and `main/platform/board_profile.c` as hardware-capability description that may remain in place, but do not let them remain the canonical source of session behavior once runtime config is introduced
- treat `main/include/runtime_banner.h`, `main/platform/runtime_banner.c`, and `main/app_main.c` as integration surfaces that can be reshaped to fit revision-1 boot and session semantics
- `main/include/runtime_types.h`: shared enums and small immutable contracts such as port IDs and health classes
- `main/include/runtime_config.h`, `main/control/runtime_config.c`: validated runtime configuration contract
- `main/include/platform_config_adapter.h`, `main/control/platform_config_adapter.c`: translation and compatibility boundary between platform-described capabilities and runtime-selected session configuration
- `main/include/fault_manager.h`, `main/control/fault_manager.c`: normalized fault classes, severities, counters, and event publication
- `main/include/session_controller.h`, `main/control/session_controller.c`: boot/start/stop/fault state machine and session metadata
- `main/include/record_builder.h`, `main/logging/record_builder.c`: binary record encoding
- `main/include/binary_log_pipeline.h`, `main/logging/binary_log_pipeline.c`: fixed-capacity record staging
- `main/include/status_log_pipeline.h`, `main/logging/status_log_pipeline.c`: compact text status staging
- `main/include/storage_service.h`, `main/storage/storage_service.c`: SD mount, session folder management, file writes, sync, close
- `main/include/uart_hal_adapter.h`, `main/capture/uart_hal_adapter.c`: ESP-IDF UART hardware wrapper
- `main/include/uart_capture_service.h`, `main/capture/uart_capture_service.c`: per-port RX buffering and chunk publication
- `main/include/sync_hal_adapter.h`, `main/capture/sync_hal_adapter.c`: GPIO interrupt and output pin wrapper
- `main/include/sync_capture_service.h`, `main/capture/sync_capture_service.c`: sync-edge queueing and normalization
- `main/include/trigger_engine.h`, `main/timing/trigger_engine.c`: trigger schedule validation and pulse emission
- `main/include/health_monitor.h`, `main/control/health_monitor.c`: watermarks, degraded state, periodic health snapshots
- `host_tests/*.cc`: host-only tests for every pure or mostly pure module
- `tools/parse_binary_log.py`: parser for binary session artifacts

The new runtime layer must align with revision-1 interface goals rather than current bring-up implementation details:

- port identifiers and per-port runtime config should map cleanly onto the existing `BOARD_PORT_COUNT` and `board_profile_t.ports[]` model for compatibility, but runtime config should own whether a port participates in the session and how it is configured
- introduce an explicit translation boundary so runtime config can be loaded later from SD, web, or other control surfaces without leaking those concerns into UART, sync, trigger, or storage modules
- `app_main` integration steps should preserve the useful existing safety checks while allowing the startup flow and ready-banner semantics to change to match the real session lifecycle
- host-test additions should follow the existing `host_tests/CMakeLists.txt` pattern so the current `ctest` top-level command keeps discovering both the inherited tests and the new module tests

## Task 1: Control Plane Foundations

**Files:**

- Create: `main/include/runtime_types.h`

- Create: `main/include/runtime_config.h`

- Create: `main/include/platform_config_adapter.h`

- Create: `main/include/fault_manager.h`

- Create: `main/include/session_controller.h`

- Create: `main/control/runtime_config.c`

- Create: `main/control/platform_config_adapter.c`

- Create: `main/control/fault_manager.c`

- Create: `main/control/session_controller.c`

- Create: `host_tests/runtime_config_host_test.cc`

- Create: `host_tests/platform_config_adapter_host_test.cc`

- Create: `host_tests/fault_manager_host_test.cc`

- Create: `host_tests/session_controller_host_test.cc`

- Modify: `main/CMakeLists.txt`

- Modify: `host_tests/CMakeLists.txt`

- Modify: `main/app_main.c`

- [ ] **Step 1: Write the failing host tests for config, fault normalization, and session transitions**

```cpp
TEST(RuntimeConfigTest, RejectsTriggerPulseWidthNotLessThanPeriod);
TEST(RuntimeConfigTest, RejectsPortConfiguredAsSyncAndTriggerTogether);
TEST(PlatformConfigAdapterTest, MapsBoardCapabilitiesIntoRuntimePortSet);
TEST(PlatformConfigAdapterTest, RejectsRuntimeSelectionThatExceedsBoardCapabilities);
TEST(FaultManagerTest, LatchesDegradedHealthForRecoverableStorageBackpressure);
TEST(SessionControllerTest, BlocksStartWhenConfigInvalid);
TEST(SessionControllerTest, TransitionsToFaultedOnFatalFault);
```

- [ ] **Step 2: Run the host tests to verify the control plane does not exist yet**

```bash
cmake -S host_tests -B build_host
cmake --build build_host --target runtime_config_host_test platform_config_adapter_host_test fault_manager_host_test session_controller_host_test
ctest --test-dir build_host --output-on-failure -R "runtime_config|platform_config_adapter|fault_manager|session_controller"
```

Expected: configure or compile failure because the new test targets and implementation files are not present yet.

- [ ] **Step 3: Add the minimal shared contracts and control-plane implementations**

```c
typedef enum {
  PORT_TIMING_DISABLED = 0,
  PORT_TIMING_SYNC_INPUT,
  PORT_TIMING_TRIGGER_OUTPUT,
} port_timing_mode_t;

typedef enum {
  SESSION_BOOT = 0,
  SESSION_CONFIG_INVALID,
  SESSION_READY,
  SESSION_STARTING,
  SESSION_RECORDING,
  SESSION_STOPPING,
  SESSION_FAULTED,
} session_state_t;

esp_err_t runtime_config_validate(const runtime_config_t* config,
                                  runtime_config_error_t* error);
esp_err_t platform_config_adapter_build_runtime(const board_profile_t* board,
                                                const runtime_config_source_t* source,
                                                runtime_config_t* out);
void fault_manager_publish(fault_manager_t* manager, const fault_event_t* event);
bool session_controller_request_start(session_controller_t* controller,
                                      const runtime_config_t* config);
```

- [ ] **Step 4: Integrate the control plane into the boot path and rerun the focused host suite**

```bash
cmake --build build_host
ctest --test-dir build_host --output-on-failure -R "runtime_config|fault_manager|session_controller|clock|board_profile|host_smoke"
```

Expected: all control-plane tests pass; `app_main` still performs board validation and clock smoke, but the session boot path now flows through a runtime-config boundary instead of treating `board_profile_t` as the direct source of active-session behavior.

- [ ] **Step 5: Commit the control-plane baseline**

```bash
git add main/CMakeLists.txt host_tests/CMakeLists.txt main/app_main.c main/include/runtime_types.h main/include/runtime_config.h main/include/platform_config_adapter.h main/include/fault_manager.h main/include/session_controller.h main/control/runtime_config.c main/control/platform_config_adapter.c main/control/fault_manager.c main/control/session_controller.c host_tests/runtime_config_host_test.cc host_tests/platform_config_adapter_host_test.cc host_tests/fault_manager_host_test.cc host_tests/session_controller_host_test.cc
git commit -m "feat: add runtime config and session control foundations"
```

## Task 2: Binary Record Contract And RAM Staging

**Files:**

- Create: `main/include/record_builder.h`

- Create: `main/include/binary_log_pipeline.h`

- Create: `main/include/status_log_pipeline.h`

- Create: `main/logging/record_builder.c`

- Create: `main/logging/binary_log_pipeline.c`

- Create: `main/logging/status_log_pipeline.c`

- Create: `host_tests/record_builder_host_test.cc`

- Create: `host_tests/binary_log_pipeline_host_test.cc`

- Create: `tools/parse_binary_log.py`

- Modify: `main/CMakeLists.txt`

- Modify: `host_tests/CMakeLists.txt`

- Modify: `README.md`

- [ ] **Step 1: Write the failing tests and parser fixture expectations for binary records**

```cpp
TEST(RecordBuilderTest, EncodesSessionStartWithConfigHashAndPortSummary);
TEST(RecordBuilderTest, EncodesUartDataWithFirstByteTimestamp);
TEST(BinaryLogPipelineTest, PreservesAppendOrderAcrossFlushBoundaries);
TEST(BinaryLogPipelineTest, EmitsOverflowFaultWhenStagingIsFull);
```

```python
assert records[0]["type"] == "session_start"
assert records[1]["type"] == "fault_event"
assert records[2]["type"] == "uart_data"
```

- [ ] **Step 2: Run the tests to verify record and pipeline modules are missing**

```bash
cmake --build build_host --target record_builder_host_test binary_log_pipeline_host_test
ctest --test-dir build_host --output-on-failure -R "record_builder|binary_log_pipeline"
```

Expected: build failure because the logging modules and parser do not exist yet.

- [ ] **Step 3: Implement the minimal record envelope, record builders, RAM staging pipeline, and parser**

```c
typedef struct {
  uint16_t record_type;
  uint16_t record_version;
  uint32_t payload_length;
  uint64_t timestamp_us;
  uint32_t source_id;
  uint32_t crc32;
} binary_record_header_t;

esp_err_t record_builder_build_session_start(const session_info_t* session,
                                             const runtime_config_t* config,
                                             record_buffer_t* out);
esp_err_t binary_log_pipeline_append(binary_log_pipeline_t* pipeline,
                                     const record_buffer_t* record,
                                     fault_event_t* overflow_fault);
```

- [ ] **Step 4: Verify encode/decode, staging, and parser behavior**

```bash
cmake --build build_host
ctest --test-dir build_host --output-on-failure -R "record_builder|binary_log_pipeline"
python3 tools/parse_binary_log.py host_tests/fixtures/session_example.bin
```

Expected: host tests pass and the parser prints ordered decoded records rather than failing on header parsing.

- [ ] **Step 5: Commit the record-contract milestone**

```bash
git add main/CMakeLists.txt host_tests/CMakeLists.txt README.md main/include/record_builder.h main/include/binary_log_pipeline.h main/include/status_log_pipeline.h main/logging/record_builder.c main/logging/binary_log_pipeline.c main/logging/status_log_pipeline.c host_tests/record_builder_host_test.cc host_tests/binary_log_pipeline_host_test.cc tools/parse_binary_log.py
git commit -m "feat: add binary record format and RAM log pipeline"
```

## Task 3: Single-Port UART Capture To The RAM Pipeline

**Files:**

- Create: `main/include/uart_hal_adapter.h`

- Create: `main/include/uart_capture_service.h`

- Create: `main/capture/uart_hal_adapter.c`

- Create: `main/capture/uart_capture_service.c`

- Create: `host_tests/uart_capture_service_host_test.cc`

- Modify: `main/CMakeLists.txt`

- Modify: `host_tests/CMakeLists.txt`

- Modify: `main/app_main.c`

- [ ] **Step 1: Write the failing tests for ring-buffer accounting, chunk boundaries, and overflow**

```cpp
TEST(UartCaptureServiceTest, PublishesChunkTimestampFromFirstByteArrival);
TEST(UartCaptureServiceTest, PreservesBytesAcrossWraparound);
TEST(UartCaptureServiceTest, EmitsOverflowFaultWhenRingBufferFills);
```

- [ ] **Step 2: Run the tests to confirm the UART capture service is not implemented**

```bash
cmake --build build_host --target uart_capture_service_host_test
ctest --test-dir build_host --output-on-failure -R "uart_capture_service"
```

Expected: build failure because the UART capture module and test target are missing.

- [ ] **Step 3: Implement one-port UART capture with chunk publication into the existing pipeline**

```c
typedef struct {
  uint8_t* storage;
  size_t capacity_bytes;
  size_t write_index;
  size_t read_index;
  uint64_t active_chunk_start_us;
  bool chunk_open;
} uart_ring_buffer_t;

esp_err_t uart_capture_service_on_rx_bytes(uart_capture_service_t* service,
                                           port_id_t port_id,
                                           uint64_t timestamp_us,
                                           const uint8_t* bytes,
                                           size_t length);
```

- [ ] **Step 4: Verify host behavior and device-side RAM-only capture**

```bash
cmake --build build_host
ctest --test-dir build_host --output-on-failure -R "uart_capture_service"
python3 -m tools.run_case --case port1_raw_capture
```

Expected: host tests pass, the device case produces non-zero UART data records in the RAM-backed session artifact, and no fatal control-plane regression appears in the monitor log.

- [ ] **Step 5: Commit the single-port raw capture milestone**

```bash
git add main/CMakeLists.txt host_tests/CMakeLists.txt main/app_main.c main/include/uart_hal_adapter.h main/include/uart_capture_service.h main/capture/uart_hal_adapter.c main/capture/uart_capture_service.c host_tests/uart_capture_service_host_test.cc
git commit -m "feat: add single-port UART capture pipeline"
```

## Task 4: SD Storage, Session Artifacts, And Boot-Autonomous Start

**Files:**

- Create: `main/include/storage_service.h`

- Create: `main/storage/storage_service.c`

- Create: `host_tests/storage_service_host_test.cc`

- Modify: `main/control/session_controller.c`

- Modify: `main/logging/binary_log_pipeline.c`

- Modify: `main/logging/status_log_pipeline.c`

- Modify: `main/app_main.c`

- Modify: `main/CMakeLists.txt`

- Modify: `host_tests/CMakeLists.txt`

- Modify: `tools/run_case.py`

- Modify: `README.md`

- [ ] **Step 1: Write the failing storage and auto-start tests**

```cpp
TEST(StorageServiceTest, CreatesSessionFolderWithBinaryStatusAndConfigFiles);
TEST(StorageServiceTest, SurfacesWriteFailureAsNormalizedFault);
TEST(SessionControllerTest, EntersReadyOnlyAfterStorageMountAndConfigLoad);
TEST(SessionControllerTest, StartsRecordingAutomaticallyOnBootWhenConfigValid);
```

- [ ] **Step 2: Run the tests to verify SD storage and auto-start are not implemented**

```bash
cmake --build build_host --target storage_service_host_test session_controller_host_test
ctest --test-dir build_host --output-on-failure -R "storage_service|session_controller"
```

Expected: failures in the new storage tests and in any new auto-start expectations.

- [ ] **Step 3: Implement the storage abstraction and wire boot flow through mount, config load, session open, and record start**

```c
esp_err_t storage_service_mount(storage_service_t* service);
esp_err_t storage_service_open_session(storage_service_t* service,
                                       const session_info_t* session);
esp_err_t storage_service_write_binary_block(storage_service_t* service,
                                             const uint8_t* data,
                                             size_t length);
esp_err_t storage_service_copy_config_snapshot(storage_service_t* service,
                                               const runtime_config_t* config);
```

- [ ] **Step 4: Verify host tests and the first SD-backed device session**

```bash
cmake --build build_host
ctest --test-dir build_host --output-on-failure -R "storage_service|session_controller|record_builder|binary_log_pipeline"
python3 -m tools.run_case --case port1_sd_logger
python3 tools/parse_binary_log.py artifacts/latest/device/port1_sd_logger/session.bin
```

Expected: session folder contains `session.bin`, a text status log, and a copied config; boot reaches recording without manual start when the SD card and config are valid.

- [ ] **Step 5: Commit the SD-backed logger milestone**

```bash
git add main/CMakeLists.txt host_tests/CMakeLists.txt README.md main/app_main.c main/include/storage_service.h main/storage/storage_service.c main/control/session_controller.c main/logging/binary_log_pipeline.c main/logging/status_log_pipeline.c host_tests/storage_service_host_test.cc tools/run_case.py
git commit -m "feat: add SD-backed session storage and autostart"
```

## Task 5: Two-Port Reference Capture

**Files:**

- Modify: `main/capture/uart_hal_adapter.c`

- Modify: `main/capture/uart_capture_service.c`

- Modify: `main/control/runtime_config.c`

- Modify: `main/app_main.c`

- Create: `host_tests/two_port_capture_host_test.cc`

- Modify: `main/CMakeLists.txt`

- Modify: `host_tests/CMakeLists.txt`

- Modify: `README.md`

- [ ] **Step 1: Write the failing tests for two-port isolation and mixed configuration**

```cpp
TEST(TwoPortCaptureTest, KeepsPortCountersAndChunksIsolated);
TEST(TwoPortCaptureTest, SupportsIndependentBaudRatesPerPort);
TEST(RuntimeConfigTest, AcceptsReferenceGnssAndImuPortProfileSet);
```

- [ ] **Step 2: Run the tests to verify the two-port reference milestone does not exist yet**

```bash
cmake --build build_host --target two_port_capture_host_test runtime_config_host_test
ctest --test-dir build_host --output-on-failure -R "two_port_capture|runtime_config"
```

Expected: failures because the capture path is still single-port.

- [ ] **Step 3: Extend the UART path from one port to the `PORT1` GNSS plus `PORT2` IMU reference configuration**

```c
esp_err_t uart_capture_service_init_ports(uart_capture_service_t* service,
                                          const runtime_config_t* config);
uart_capture_stats_t uart_capture_service_get_stats(const uart_capture_service_t* service,
                                                    port_id_t port_id);
```

- [ ] **Step 4: Verify the first real dual-sensor logging setup**

```bash
cmake --build build_host
ctest --test-dir build_host --output-on-failure -R "two_port_capture|uart_capture_service|runtime_config"
python3 -m tools.run_case --case two_port_reference_capture
python3 tools/parse_binary_log.py artifacts/latest/device/two_port_reference_capture/session.bin
```

Expected: the session artifact contains isolated UART records for `PORT1` and `PORT2` with independent counters and no cross-port corruption.

- [ ] **Step 5: Commit the two-port reference milestone**

```bash
git add main/CMakeLists.txt host_tests/CMakeLists.txt README.md main/app_main.c main/control/runtime_config.c main/capture/uart_hal_adapter.c main/capture/uart_capture_service.c host_tests/two_port_capture_host_test.cc
git commit -m "feat: add two-port reference capture"
```

## Task 6: Four-Port Scaling, Loss Handling, And Sync Input Capture

**Files:**

- Create: `main/include/sync_hal_adapter.h`

- Create: `main/include/sync_capture_service.h`

- Create: `main/capture/sync_hal_adapter.c`

- Create: `main/capture/sync_capture_service.c`

- Create: `host_tests/sync_capture_service_host_test.cc`

- Modify: `main/control/runtime_config.c`

- Modify: `main/capture/uart_capture_service.c`

- Modify: `main/app_main.c`

- Modify: `main/CMakeLists.txt`

- Modify: `host_tests/CMakeLists.txt`

- [ ] **Step 1: Write the failing tests for four-port scaling, retention sizing, sync edge normalization, and overload accounting**

```cpp
TEST(SyncCaptureServiceTest, PreservesObservedEdgePolarity);
TEST(SyncCaptureServiceTest, EmitsOverflowFaultWhenEdgeQueueFills);
TEST(RuntimeConfigTest, AcceptsFourIndependentPortModes);
TEST(RuntimeConfigTest, RejectsSyncAndTriggerConflictOnSamePort);
TEST(UartCaptureServiceTest, SizesRetentionForFiveHundredMillisecondsAtConfiguredBaud);
TEST(UartCaptureServiceTest, OverflowOnOnePortDoesNotCorruptAnotherPort);
```

- [ ] **Step 2: Run the tests to confirm sync capture and four-port scaling are not present**

```bash
cmake --build build_host --target sync_capture_service_host_test runtime_config_host_test
ctest --test-dir build_host --output-on-failure -R "sync_capture_service|runtime_config"
```

Expected: new sync tests fail before implementation.

- [ ] **Step 3: Extend runtime config to four ports, then implement interrupt-driven sync capture**

```c
esp_err_t sync_hal_adapter_configure_input(port_id_t port_id,
                                           sync_edge_mode_t edge_mode);
esp_err_t sync_capture_service_publish_isr(sync_capture_service_t* service,
                                           port_id_t port_id,
                                           uint64_t timestamp_us,
                                           bool level_high);
esp_err_t sync_capture_service_drain(sync_capture_service_t* service,
                                     binary_log_pipeline_t* pipeline,
                                     fault_manager_t* faults);
size_t uart_capture_required_retention_bytes(uint32_t baud_rate,
                                             uint32_t retention_ms);
```

- [ ] **Step 4: Verify host behavior and concurrent four-port capture with sync events and explicit overload reporting**

```bash
cmake --build build_host
ctest --test-dir build_host --output-on-failure -R "sync_capture_service|runtime_config|uart_capture_service"
python3 -m tools.run_case --case sync_input_capture
python3 -m tools.run_case --case four_port_stress
python3 -m tools.run_case --case four_port_overload_faults
python3 tools/parse_binary_log.py artifacts/latest/device/sync_input_capture/session.bin
```

Expected: parsed logs contain `sync_edge` records with correct port IDs and polarities while UART records remain present, and overload runs produce explicit loss or degradation records rather than silent drops.

- [ ] **Step 5: Commit the four-port scaling milestone**

```bash
git add main/CMakeLists.txt host_tests/CMakeLists.txt main/app_main.c main/control/runtime_config.c main/include/sync_hal_adapter.h main/include/sync_capture_service.h main/capture/sync_hal_adapter.c main/capture/sync_capture_service.c main/capture/uart_capture_service.c host_tests/sync_capture_service_host_test.cc
git commit -m "feat: add four-port capture, overload handling, and sync input logging"
```

## Task 7: Trigger Output, Health Monitoring, And Degraded/Faulted Policy

**Files:**

- Create: `main/include/trigger_engine.h`

- Create: `main/include/health_monitor.h`

- Create: `main/timing/trigger_engine.c`

- Create: `main/control/health_monitor.c`

- Create: `host_tests/trigger_engine_host_test.cc`

- Create: `host_tests/health_monitor_host_test.cc`

- Modify: `main/control/fault_manager.c`

- Modify: `main/control/session_controller.c`

- Modify: `main/logging/record_builder.c`

- Modify: `main/app_main.c`

- Modify: `main/CMakeLists.txt`

- Modify: `host_tests/CMakeLists.txt`

- Modify: `README.md`

- [ ] **Step 1: Write the failing tests for trigger schedule math, jitter-related fault surfacing, and degraded-health policy**

```cpp
TEST(TriggerEngineTest, RejectsPulseWidthGreaterThanOrEqualToPeriod);
TEST(TriggerEngineTest, SchedulesAssertAndDeassertEdgesFromSharedClock);
TEST(HealthMonitorTest, LatchesDegradedWhenStorageLatencyExceedsBudget);
TEST(SessionControllerTest, StopsAuthoritativeRecordingOnFatalTimingFault);
TEST(FaultManagerTest, ClassifiesCardRemovalAndPipelineExhaustionExplicitly);
```

- [ ] **Step 2: Run the tests to verify trigger and health-monitor modules are still missing**

```bash
cmake --build build_host --target trigger_engine_host_test health_monitor_host_test session_controller_host_test
ctest --test-dir build_host --output-on-failure -R "trigger_engine|health_monitor|session_controller"
```

Expected: build failure or new red tests because trigger and health modules do not exist yet.

- [ ] **Step 3: Implement the trigger engine, health monitor, and final policy wiring**

```c
esp_err_t trigger_engine_validate_config(const trigger_config_t* config);
esp_err_t trigger_engine_start_session(trigger_engine_t* engine,
                                       const session_info_t* session);
void health_monitor_publish_storage_sample(health_monitor_t* monitor,
                                           uint32_t write_latency_ms);
health_snapshot_t health_monitor_snapshot(const health_monitor_t* monitor);
```

- [ ] **Step 4: Verify host tests and measured device behavior under concurrent logging load, including the explicit timing acceptance gate**

```bash
cmake --build build_host
ctest --test-dir build_host --output-on-failure -R "trigger_engine|health_monitor|session_controller|fault_manager"
python3 -m tools.run_case --case trigger_output_basic
python3 -m tools.run_case --case trigger_output_under_load
python3 tools/parse_binary_log.py artifacts/latest/device/trigger_output_under_load/session.bin
```

Expected: trigger events appear in the binary log, degraded or faulted transitions are visible in the status path, the monitor log shows explicit health counters instead of silent loss, and measured timing either passes the target or forces an explicit architecture review outcome.

- [ ] **Step 5: Commit the timing and observability milestone**

```bash
git add main/CMakeLists.txt host_tests/CMakeLists.txt README.md main/app_main.c main/control/fault_manager.c main/control/session_controller.c main/control/health_monitor.c main/include/trigger_engine.h main/include/health_monitor.h main/timing/trigger_engine.c main/logging/record_builder.c host_tests/trigger_engine_host_test.cc host_tests/health_monitor_host_test.cc
git commit -m "feat: add trigger output and health observability"
```

## Task 8: Final Integration, Soak Validation, And Cleanup

**Files:**

- Modify: `main/app_main.c`

- Modify: `README.md`

- Modify: `tools/run_case.py`

- Modify: `tools/monitor.py`

- Modify: `docs/superpowers/specs/2026-03-28-uart-sensor-hub-design.md`

- [ ] **Step 1: Write the final failing integration expectations as executable case definitions**

```python
CASES["full_session_nominal"] = DeviceCase(ready_banner="READY: recording")
CASES["storage_backpressure_fault"] = DeviceCase(expect_fault="SD_WRITE_FAILURE")
CASES["four_port_soak"] = DeviceCase(duration_s=300)
CASES["timing_acceptance_gate"] = DeviceCase(expect_measurement="timing_result.json")
```

- [ ] **Step 2: Run the existing automated paths and capture the gaps**

```bash
cmake --build build_host
ctest --test-dir build_host --output-on-failure
python3 -m tools.run_case --case full_session_nominal
python3 -m tools.run_case --case storage_backpressure_fault
python3 -m tools.run_case --case four_port_soak
python3 -m tools.run_case --case timing_acceptance_gate
```

Expected: at least one case still fails until case definitions, ready banners, and artifact collection are fully aligned.

- [ ] **Step 3: Finish integration wiring and document the operator workflow**

```c
runtime_banner_log_ready("recording");
runtime_banner_log_ready("degraded");
runtime_banner_log_ready("faulted");
```

```python
artifact_names = ["session.bin", "status.log", "config.json", "monitor.log", "timing_result.json"]
```

- [ ] **Step 4: Run the full verification matrix**

```bash
source ./tools/setup.sh
./tools/bootstrap_env.sh
idf.py build
cmake -S host_tests -B build_host
cmake --build build_host
ctest --test-dir build_host --output-on-failure
python3 -m tools.run_case --case full_session_nominal
python3 -m tools.run_case --case trigger_output_under_load
python3 -m tools.run_case --case four_port_soak
python3 -m tools.run_case --case timing_acceptance_gate
uv run --group dev pre-commit run --all-files
```

Expected: host tests pass, device cases save complete artifacts, and formatting hooks succeed.

- [ ] **Step 5: Commit the integrated rev1 logger**

```bash
git add main/app_main.c README.md tools/run_case.py tools/monitor.py docs/superpowers/specs/2026-03-28-uart-sensor-hub-design.md
git commit -m "feat: complete uart sensor hub rev1 integration"
```

## Spec Coverage Check

- session boot flow, config validation, and start gating are covered by Task 1 and Task 4
- authoritative binary record contract is covered by Task 2
- UART chunk capture is covered by Task 3
- SD-backed session artifacts and copied config snapshots are covered by Task 4
- the explicit two-port GNSS plus IMU checkpoint is covered by Task 5
- four-port scaling, `500 ms` retention sizing, overload handling, and sync-input capture are covered by Task 6
- trigger-output scheduling, explicit degraded or faulted policy, and the timing acceptance gate are covered by Task 7
- soak, artifact collection, and operator-facing autostart workflows are covered by Task 8

## Intentional Exclusions

- no sensor-preparation or sensor-readiness gating in revision 1
- no optional framing or parsing metadata in revision 1
- no cloud or network control path in revision 1
- no live GNSS clock discipline or phase correction in revision 1
- no requirement for local button or console control as a supported operator interface in revision 1

## Placeholder Scan

No task in this plan uses `TODO`, `TBD`, or “implement later”. Every task names concrete file paths, expected tests, and the command shape required to verify progress.

## Type Consistency Check

- `runtime_config_t`, `session_info_t`, `fault_event_t`, `binary_log_pipeline_t`, `uart_capture_service_t`, `sync_capture_service_t`, `trigger_engine_t`, and `health_monitor_t` are used consistently across tasks
- timing modes remain `disabled | sync-input | trigger-output`
- session states remain `BOOT | CONFIG_INVALID | READY | STARTING | RECORDING | STOPPING | FAULTED`

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-03-28-uart-sensor-hub-rev1-implementation.md`.

Two execution options:

1. Subagent-Driven (recommended) - dispatch a fresh subagent per task, review between tasks, fast iteration
1. Inline Execution - execute tasks in this session using executing-plans, batch execution with checkpoints

______________________________________________________________________

## Progress Snapshot

Status as of `2026-03-29` on branch `tim/exp/superpowers`:

- [x] Atomic stack slice 1: runtime config contract and validation
