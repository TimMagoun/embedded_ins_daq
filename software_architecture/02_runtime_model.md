# Runtime Model

## Execution Domains, Tasks, and Core Affinity

______________________________________________________________________

## Execution Domains

The firmware is divided into three execution domains:

1. **Interrupt domain**
   Handles UART RX service, SYNC edge capture, and trigger timing.
1. **Real-time task domain**
   Drains interrupt queues, builds records, and stages data for logging.
1. **Background task domain**
   Performs SD writes, status formatting, control processing, and optional framing.

______________________________________________________________________

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

______________________________________________________________________

## Priority Guidance

Recommended priority ordering:

1. interrupt handlers
1. `task_uart_capture`
1. `task_sync_trigger`
1. `task_binary_log_stage`
1. `task_sensor_setup`
1. `task_sd_writer`
1. `task_status_writer`
1. `task_local_control`
1. `task_framing`
1. `task_health_monitor`

Rationale:

- any work required to avoid data loss outranks work that improves convenience
- storage remains below capture but above optional framing
- sensor setup is important, but it is pre-record control logic rather than active capture logic
- health monitoring must never preempt data preservation

______________________________________________________________________

## Core Affinity Guidance

On ESP32-P4, active capture and storage work should run on the HP cores.

Suggested partition:

- HP Core A: UART capture, SYNC/trigger service, binary staging
- HP Core B: sensor setup, SD writer, status writer, framing, local control

The LP core is not required for the first revision.

______________________________________________________________________

## Runtime Implications

- interrupt work must remain minimal and bounded
- no filesystem calls may occur in ISR context
- capture-critical tasks must not depend on optional parsing or diagnostics
- sensor preparation must complete before `session_controller` permits transition into active recording
