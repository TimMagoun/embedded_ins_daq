# Capture And Logging Modules
## Clock, Capture, Records, Pipelines, and Storage

---

## Module Map In This Document

- `clock_service`
- `uart_hal_adapter`
- `uart_capture_service`
- `sync_hal_adapter`
- `sync_capture_service`
- `trigger_engine`
- `record_builder`
- `binary_log_pipeline`
- `status_log_pipeline`
- `framing_service`
- `sd_storage_service`
- `health_monitor`

---

## `clock_service`

Provides the single monotonic microsecond timestamp source.

Public interface:

- `clock_init()`
- `clock_now_us() -> uint64`
- `clock_now_isr() -> uint64`

Contract:

- timestamps are monotonic for the duration of the boot
- timestamps are safe to read from ISR context
- no consumer may redefine the clock during a session

---

## `uart_hal_adapter`

Configures ESP-IDF UART peripherals and abstracts interrupt or DMA setup details.

Public interface:

- `uart_open(port_id, UartConfig)`
- `uart_close(port_id)`
- `uart_enable_rx(port_id)`
- `uart_get_stats(port_id) -> UartHalStats`

Contract:

- exposes only non-blocking receive paths
- never allocates dynamically after initialization
- does not write log records directly

---

## `uart_capture_service`

Drains UART RX events, appends bytes into per-port circular buffers, builds chunk descriptors, and reports overflow.

Public interface:

- `uart_capture_init(RuntimeConfig)`
- `uart_capture_start_session(SessionInfo)`
- `uart_capture_stop_session()`
- `uart_capture_get_stats(port_id) -> UartCaptureStats`

Contract:

- raw UART bytes are preserved whether or not framing is enabled
- overflow increments the per-port loss counter and emits a fault event
- no SD or formatting work is performed here

Memory:

- ISR event queue in internal RAM
- circular buffer descriptors in internal RAM
- raw byte storage in internal RAM for revision 1

---

## `sync_hal_adapter`

Configures GPIO direction, interrupt mode, and trigger-capable output behavior for SYNC pins.

Public interface:

- `sync_pin_configure_input(port_id, edge_mode)`
- `sync_pin_configure_output(port_id)`
- `sync_pin_set(port_id, level)`
- `sync_get_stats(port_id) -> SyncHalStats`

Contract:

- all SYNC input capture is interrupt-driven
- a pin cannot be input capture and trigger output at the same time

---

## `sync_capture_service`

Receives SYNC edge events from the ISR path and normalizes them into log-ready structures.

Public interface:

- `sync_capture_init(RuntimeConfig)`
- `sync_capture_start_session(SessionInfo)`
- `sync_capture_stop_session()`

Contract:

- every accepted edge becomes an independent record candidate
- edge polarity and port identity are preserved exactly

Memory:

- ISR queue in internal RAM
- normalization workspace in internal RAM

---

## `trigger_engine`

Generates scheduled trigger pulses on output-configured ports and emits matching trigger log events.

Public interface:

- `trigger_init(RuntimeConfig)`
- `trigger_start_session(SessionInfo)`
- `trigger_stop_session()`
- `trigger_get_stats(port_id) -> TriggerStats`

Contract:

- pulse rate and width follow validated configuration
- trigger-enabled ports cannot be used as SYNC input
- every asserted pulse produces exactly one trigger record candidate

---

## `record_builder`

Transforms normalized events into versioned binary record payloads.

Public interface:

- `build_session_record(...)`
- `build_uart_record(...)`
- `build_sync_record(...)`
- `build_trigger_record(...)`
- `build_sensor_state_record(...)`
- `build_fault_record(...)`

Contract:

- binary record version and schema are centralized here
- record construction is deterministic from normalized inputs

---

## `binary_log_pipeline`

Accepts binary records from producers, stages them into large write-friendly buffers, and provides flush-ready blocks to storage.

Public interface:

- `binary_pipeline_open_session(SessionInfo, ConfigHash)`
- `binary_pipeline_append(BinaryRecordEnvelope)`
- `binary_pipeline_request_flush()`
- `binary_pipeline_close_session()`
- `binary_pipeline_get_stats() -> BinaryPipelineStats`

Contract:

- producers are decoupled from SD writes
- append either succeeds fully or produces a fault/loss indication
- staged buffers are written on complete record boundaries
- record order is preserved

Memory:

- queue metadata in internal RAM
- staging descriptors in internal RAM
- bulk staging buffers in internal RAM for revision 1, with PSRAM as an optimization candidate later

---

## `status_log_pipeline`

Converts system events into human-readable status entries and stages them separately from binary logs.

Public interface:

- `status_log_open_session(SessionInfo)`
- `status_log_publish(StatusEvent)`
- `status_log_close_session()`

Contract:

- status logging is secondary and must never block raw capture
- if it falls behind, the condition is reported rather than hidden

---

## `framing_service`

Optionally inspects copied UART chunks for supported framing rules such as NMEA or UBX and emits metadata records.

Public interface:

- `framing_init(RuntimeConfig)`
- `framing_publish_chunk(FrameInputChunk)`
- `framing_get_stats(port_id) -> FramingStats`

Contract:

- framing never owns the authoritative raw byte stream
- framing failure cannot suppress raw UART logging
- framing may degrade independently if overloaded

---

## `sd_storage_service`

Mounts the SD card, opens session files, writes staged binary and status data, performs sync operations, and reports storage failures.

Public interface:

- `storage_init()`
- `storage_mount()`
- `storage_open_session(SessionInfo)`
- `storage_write_binary_block(BlockRef)`
- `storage_write_status_block(BlockRef)`
- `storage_sync()`
- `storage_close_session()`
- `storage_get_stats() -> StorageStats`

Contract:

- storage writes never run in interrupt context
- binary records are written only as complete staged blocks
- storage faults are surfaced immediately and visibly

---

## `health_monitor`

Observes watermarks, queue depths, SD latency, and fault counts and publishes warnings before loss becomes unavoidable where possible.

Public interface:

- `health_monitor_tick()`
- `health_monitor_get_snapshot() -> HealthSnapshot`

Contract:

- does not directly change capture behavior
- policy actions are routed through `session_controller` or `fault_manager`

