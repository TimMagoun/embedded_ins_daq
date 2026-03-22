# Communication Pipelines
## End-To-End Flows Between Modules

---

## UART Data Pipeline

```mermaid
sequenceDiagram
    participant UART as UART Peripheral
    participant ISR as UART RX ISR
    participant UCAP as uart_capture_service
    participant RB as record_builder
    participant BLP as binary_log_pipeline
    participant SD as sd_storage_service

    UART->>ISR: RX event / DMA completion
    ISR->>UCAP: RX-ready notification
    UCAP->>UCAP: append bytes to per-port circular buffer
    UCAP->>RB: UartChunkDescriptor
    RB->>BLP: BinaryRecordEnvelope(UART_RAW)
    BLP->>SD: flush-ready block
```

Guarantees:

- bytes enter a preallocated per-port buffer before optional processing
- record creation is downstream of capture buffering
- SD backpressure is absorbed by staging buffers until retention is exhausted

---

## Sensor Preparation Pipeline

```mermaid
sequenceDiagram
    participant SES as session_controller
    participant SM as sensor_manager
    participant DRV as sensor_driver_*
    participant RB as record_builder
    participant BLP as binary_log_pipeline

    SES->>SM: prepare_all(SessionInfo)
    SM->>DRV: probe()
    DRV-->>SM: probe result
    SM->>DRV: configure()
    DRV-->>SM: configure result
    SM->>DRV: verify_ready()
    DRV-->>SM: ready / failed
    SM->>DRV: arm_for_recording()
    DRV-->>SM: arm result
    SM->>RB: SensorStateRecordInput
    RB->>BLP: BinaryRecordEnvelope(SENSOR_STATE)
    SM-->>SES: readiness summary
```

Guarantees:

- heterogeneous sensor setup is normalized through one lifecycle contract
- failed required sensors block transition into active recording
- raw capture does not begin until preparation gating is complete

---

## Optional Framing Pipeline

```mermaid
sequenceDiagram
    participant UCAP as uart_capture_service
    participant FRM as framing_service
    participant RB as record_builder
    participant BLP as binary_log_pipeline

    UCAP->>FRM: FrameInputChunk(copy)
    FRM->>RB: FramingMetadataInput
    RB->>BLP: BinaryRecordEnvelope(FRAME_META)
```

Guarantees:

- framing consumes copies, not authoritative capture buffers
- raw logging remains valid if framing is disabled or overloaded

---

## SYNC Edge Pipeline

```mermaid
sequenceDiagram
    participant GPIO as SYNC GPIO
    participant ISR as SYNC ISR
    participant SCAP as sync_capture_service
    participant RB as record_builder
    participant BLP as binary_log_pipeline

    GPIO->>ISR: edge
    ISR->>SCAP: port + edge + timestamp
    SCAP->>RB: SyncEdgeRecordInput
    RB->>BLP: BinaryRecordEnvelope(SYNC_EDGE)
```

---

## Trigger Pipeline

```mermaid
sequenceDiagram
    participant SES as session_controller
    participant TR as trigger_engine
    participant CLK as clock_service/timer
    participant RB as record_builder
    participant BLP as binary_log_pipeline

    SES->>TR: start with trigger config
    CLK->>TR: scheduled pulse event
    TR->>TR: assert GPIO / schedule deassert
    TR->>RB: TriggerRecordInput
    RB->>BLP: BinaryRecordEnvelope(TRIGGER)
```

---

## Fault And Status Pipeline

```mermaid
flowchart LR
    SRC[All Runtime Modules] --> FM[fault_manager]
    FM --> RB[record_builder]
    RB --> BLP[binary_log_pipeline]
    FM --> SLP[status_log_pipeline]
    SLP --> SDS[sd_storage_service]
```

