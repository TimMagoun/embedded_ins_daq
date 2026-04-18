# Step 06: UART Capture

**Objective:** Implement per-port UART chunk capture with first-byte timestamps, idle-gap flush, and max-size flush.

**Files:**

- Create: `main/interfaces/uart_interface.hpp`
- Create: `main/interfaces/queue_interface.hpp`
- Create: `main/core/uart_capture_types.hpp`
- Create: `main/core/uart_capture_core.hpp`
- Create: `main/core/uart_capture_core.cpp`
- Create: `main/include/uart_capture.hpp`
- Create: `main/uart_capture.cpp`
- Create: `main/adapters/esp32/esp32_uart_adapter.hpp`
- Create: `main/adapters/esp32/esp32_uart_adapter.cpp`
- Create: `main/adapters/host/host_uart_adapter.hpp`
- Create: `main/adapters/host/host_uart_adapter.cpp`
- Create: `main/adapters/esp32/esp32_queue_adapter.hpp`
- Create: `main/adapters/esp32/esp32_queue_adapter.cpp`
- Create: `main/adapters/host/host_queue_adapter.hpp`
- Create: `main/adapters/host/host_queue_adapter.cpp`
- Modify: `main/include/daq_config.hpp`
- Modify: `main/include/daq_types.hpp`
- Create: `host_tests/test_uart_capture_core.cpp`

**Implementation Notes:**

- One `UartCapture` instance per enabled sensor UART.
- Start with ESP-IDF UART driver events as the wakeup mechanism.
- Maintain per-port chunk state:
  - `active`
  - `first_byte_ts_us`
  - `last_byte_ts_us`
  - `length`
  - fixed payload buffer
- Ignore all bytes observed in `ready`.
- In `running`, emit a record when:
  - idle-gap expires
  - chunk reaches max size
- Queue overflow is fatal and reported immediately.
- Put chunk state, first-byte timestamp rules, flush decisions, and output-record generation in `uart_capture_core.cpp`.
- Keep `uart_capture.cpp` as a coordinating wrapper that translates interface events into core inputs.
- `esp32_uart_adapter.cpp` should be the only place that touches ESP-IDF UART driver APIs.
- `host_uart_adapter.cpp` should feed deterministic byte/event streams into the same interface contract used by target code.
- Queue interactions must go through `QueueInterface` so queue-full behavior is testable on host without FreeRTOS.
- Drain bytes from the UART driver into a fixed local buffer and hand the batch to `uart_capture_core.cpp`; do not call core logic once per byte unless measurement later proves it necessary.
- Keep adapter-to-core payload handoff zero-copy or single-copy into statically owned buffers only; do not repack the same bytes across multiple transient buffers.
- Do not put chunking policy, idle-gap decisions, or fault decisions into `esp32_uart_adapter.cpp`.
- If the UART event callback path requires a C function pointer, expose only that narrow callback in `extern "C"` and hand off immediately to C++ adapter code.

**Native Verification:**

- Host tests for:
  - first byte stamps the whole chunk
  - later bytes do not overwrite first-byte timestamp
  - idle-gap emits exactly one chunk
  - max-size emits without waiting for idle
  - bytes received before `start` are discarded
  - queue full returns an irrecoverable fault
  - batch ingest semantics match expected output regardless of how the adapter chunks driver reads
  - exact idle-gap boundary behavior when elapsed time is just below, exactly at, and just above threshold
  - stop with an active partial chunk follows the specified flush-or-drop behavior
  - start immediately after pre-start bytes does not leak pre-start data into the first in-session chunk
  - multiple back-to-back chunks on one port preserve per-chunk first-byte timestamps
  - interleaved batches from multiple ports preserve independent chunk state
  - max-size boundary at `N-1`, `N`, and `N+1` bytes
  - zero-byte batches are ignored without corrupting state
  - post-fault input does not mutate chunk state or emit additional records
- Host Validation Gate:
  - Use the Host Validation Gate from [AGENT.md](/home/agent/workspace/embedded_ins_daq/AGENT.md#6-testing--quality-gates).
  - Use `ctest --test-dir build_host --output-on-failure -R uart_capture_core` for the step-specific filter.

**On-Device Hardware Verification:**

- Connect the mock UART generator to one sensor port first, then all enabled ports.
- Drive known message patterns at low baud and then up to `921600`.
- Execute explicit scenarios:
  - single-port low-rate idle-gap chunking
  - single-port continuous stream forcing max-size chunking
  - four-port simultaneous traffic with distinct deterministic patterns
  - pre-start traffic followed by clean start
  - stop during an active chunk
  - induced queue saturation by temporarily shrinking queue depth in a debug build
- Expected result:
  - Captured file contains every byte with no gaps or duplicates.
  - Chunk boundaries track idle gaps and max-size fallback as configured.
  - Record timestamps align to first-byte arrival when checked against the logic analyzer.
  - No pre-start bytes appear in the session file.
  - Stop behavior for an active chunk matches the documented rule exactly.
  - Saturation causes a deterministic fault with no silent truncation.

**Exit Criteria:**

- UART data capture is correct and deterministic before storage muxing is introduced.
