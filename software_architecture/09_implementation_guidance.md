# Implementation Guidance
## Bring-Up Order, Compliance, and Summary

---

## Recommended Implementation Order

1. `clock_service`
2. `config_manager`
3. `session_controller`
4. `sensor_manager`
5. `sensor_driver_raw_uart`
6. `uart_hal_adapter`
7. `uart_capture_service`
8. `record_builder`
9. `binary_log_pipeline`
10. `sd_storage_service`
11. `fault_manager`
12. `sync_hal_adapter`
13. `sync_capture_service`
14. `trigger_engine`
15. `status_log_pipeline`
16. `local_control_service`
17. `health_monitor`
18. `framing_service`

Reason:

- this order brings up authoritative capture and persistence before optional interpretation

---

## Definition Of Done For Architecture Compliance

An implementation complies with this architecture when:

- every module boundary described in the architecture exists clearly in source structure or equivalent design
- capture-critical buffers are preallocated
- ISR work is minimal and non-blocking
- required sensors are gated through a shared readiness contract before recording starts
- raw UART logging functions without framing enabled
- SD writes are downstream of explicit staging buffers
- all fault classes are surfaced through normalized fault handling
- session state changes are centralized in `session_controller`
- memory placement decisions for critical paths are explicit and documented

---

## Architecture Summary

The logger is organized around three critical flows:

- capture events safely and quickly
- transform them into authoritative records
- persist them without blocking capture

The design is intentionally conservative. It favors:

- explicit ownership
- fixed-capacity buffers
- centralized state transitions
- shared sensor readiness contracts
- strict separation between capture-critical and optional functionality

This should allow a firmware engineer to implement the system while preserving the PRD priorities of reliable capture, timing integrity, and explicit fault reporting.

