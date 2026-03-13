# Runtime Model
## Execution Domains, Tasks, and Core Affinity

---

## Execution Domains

The firmware is divided into three execution domains:

1. **Interrupt domain**
   Handles UART RX service, SYNC edge capture, and trigger timing.
2. **Real-time task domain**
   Drains interrupt queues, builds records, and stages data for logging.
3. **Background task domain**
   Performs SD writes, status formatting, control processing, and optional framing.

---

## Task Set

Recommended task set:

- `task_session_control`
- `task_sensor_setup`
- `task_uart_capture`
- `task_sync_trigger`
- `task_binary_log_stage`
- `task_sd_writer`
- `task_status_writer`
- `task_framing`
- `task_health_monitor`
- `task_local_control`

---

## Priority Guidance

Recommended priority ordering:

1. interrupt handlers
2. `task_uart_capture`
3. `task_sync_trigger`
4. `task_binary_log_stage`
5. `task_sensor_setup`
6. `task_sd_writer`
7. `task_status_writer`
8. `task_local_control`
9. `task_framing`
10. `task_health_monitor`

Rationale:

- any work required to avoid data loss outranks work that improves convenience
- storage remains below capture but above optional framing
- sensor setup is important, but it is pre-record control logic rather than active capture logic
- health monitoring must never preempt data preservation

---

## Core Affinity Guidance

On ESP32-P4, active capture and storage work should run on the HP cores.

Suggested partition:

- HP Core A: UART capture, SYNC/trigger service, binary staging
- HP Core B: sensor setup, SD writer, status writer, framing, local control

The LP core is not required for the first revision.

---

## Runtime Implications

- interrupt work must remain minimal and bounded
- no filesystem calls may occur in ISR context
- capture-critical tasks must not depend on optional parsing or diagnostics
- sensor preparation must complete before `session_controller` permits transition into active recording

