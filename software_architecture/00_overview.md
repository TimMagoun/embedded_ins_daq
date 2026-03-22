# Software Architecture Overview

## ESP32-P4 Nano Robust UART Sensor Logger

**Purpose:** Entry point for the split software architecture documents.  
**Source Document:** [Software_architecture_embedded_sensor_hub.md](/Users/timmagoun/Projects/embedded_ins_daq/Software_architecture_embedded_sensor_hub.md)

---

## Document Set

This folder breaks the full software architecture into smaller files so engineers can load only the context they need.

Recommended reading order:

1. [01_foundations.md](01_foundations.md)
2. [02_runtime_model.md](02_runtime_model.md)
3. [03_control_and_sensor_modules.md](03_control_and_sensor_modules.md)
4. [04_capture_and_logging_modules.md](04_capture_and_logging_modules.md)
5. [05_communication_pipelines.md](05_communication_pipelines.md)
6. [06_data_contracts.md](06_data_contracts.md)
7. [07_memory_and_buffers.md](07_memory_and_buffers.md)
8. [08_state_fault_storage_observability.md](08_state_fault_storage_observability.md)
9. [09_implementation_guidance.md](09_implementation_guidance.md)

---

## Piece Summary

### [01_foundations.md](01_foundations.md)

Scope, architectural principles, system context, and the top-level software diagram. Read this first to understand the system boundaries and the core design rules.

### [02_runtime_model.md](02_runtime_model.md)

Execution domains, task model, priority guidance, and core affinity. Use this when deciding what runs in ISR context, what runs in tasks, and how work is partitioned across the P4 HP cores.

### [03_control_and_sensor_modules.md](03_control_and_sensor_modules.md)

Control-plane modules: configuration, session control, sensor lifecycle management, sensor-driver contracts, local control, and fault normalization. Use this for startup sequencing and heterogeneous sensor support.

### [04_capture_and_logging_modules.md](04_capture_and_logging_modules.md)

Data-plane modules: clock, UART, SYNC, trigger, record building, binary/status pipelines, framing, storage, and health monitoring. Use this for capture, logging, and persistence implementation.

### [05_communication_pipelines.md](05_communication_pipelines.md)

Sequence and flow diagrams for UART capture, sensor preparation, framing, SYNC, trigger, and fault/status pipelines. Use this when tracing end-to-end data flow.

### [06_data_contracts.md](06_data_contracts.md)

Logical contracts for shared data structures such as `RuntimeConfig`, `SessionInfo`, `SensorProfile`, record envelopes, and fault events. Use this when defining interfaces between modules.

### [07_memory_and_buffers.md](07_memory_and_buffers.md)

Memory placement rules, internal RAM vs PSRAM guidance, buffer ownership, and queue inventory. Use this for allocation policy and real-time safety.

### [08_state_fault_storage_observability.md](08_state_fault_storage_observability.md)

State machines, fault classes and severity, storage/file contracts, and required observability metrics. Use this when implementing lifecycle control and degraded/failure behavior.

### [09_implementation_guidance.md](09_implementation_guidance.md)

Recommended implementation order, architecture compliance criteria, and a short summary of the whole design. Use this for planning and review.

---

## How To Load Context Efficiently

Use these combinations depending on the task:

- New engineer onboarding: `00`, `01`, `02`, `03`, `04`
- Startup/session work: `00`, `02`, `03`, `08`
- UART capture/logging work: `00`, `02`, `04`, `05`, `07`
- Sensor-driver work: `00`, `03`, `05`, `06`, `08`
- Storage/log format work: `00`, `04`, `06`, `08`
- Performance tuning: `00`, `02`, `04`, `07`, `08`
