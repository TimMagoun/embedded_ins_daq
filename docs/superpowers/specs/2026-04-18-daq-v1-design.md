# DAQ V1 Design

## Overview

This document defines the first prototype design for the embedded data acquisition unit (DAQ) running on the ESP32-P4. The prototype must validate three goals together:

- capture all UART bytes and trigger/sync events reliably
- maintain microsecond-accurate timestamps for UART packet starts and timing events
- prove the basic control pipeline and state model are correct without overbuilding features

The device records UART traffic from up to four serial ports into a single binary log file on an SD card. The same log also contains trigger and sync timing events.

## V1 Constraints

- configuration is compile-time only
- UART packet timestamps represent first-byte arrival time
- UART chunking uses idle-gap flush with a fixed-size fallback
- sync records include edge information
- all data-path buffering is fixed-capacity and statically allocated
- any overflow or storage failure is irrecoverable and forces `faulted`
- all packets in the binary log must have timestamps greater than or equal to the session start time
- pre-start observations in `ready` are ignored rather than buffered
- the SD card log uses a normal filesystem file rather than raw block logging
- the file format is custom binary, not protobuf or nanopb

## State Model

The prototype uses four states:

- `init`
- `ready`
- `running`
- `faulted`

Transitions:

- `init -> ready` after the state manager has observed all required readiness facts, including compile-time configuration success and storage availability
- `ready -> running` on start-command status
- `running -> ready` on stop-command status
- `* -> faulted` on any irrecoverable fault

The `faulted` state is terminal for v1.

```mermaid
stateDiagram-v2
    [*] --> init
    init --> ready: config_ok
    init --> faulted: config_fault
    ready --> running: start
    running --> ready: stop
    ready --> faulted: fault
    running --> faulted: fault
    faulted --> [*]
```

## Plane Separation

The firmware is split into a data plane and a control plane with deliberately small interfaces.

### Data Plane

The data plane owns deterministic capture and persistence:

- UART capture per port
- trigger/sync capture per port
- per-source fixed buffers and source queues
- storage muxing and record serialization
- SD-card file writing
- shared monotonic microsecond timestamp service

### Control Plane

The control plane decides whether a session is active and records system health:

- state transitions
- startup configuration validation
- coarse commands to the data plane
- centralized status and fault reporting

Control-plane modules emit raw status and fault facts. The control plane
aggregates those facts into the authoritative global lifecycle state.

The control plane must not sit in the hot path for byte capture, ISR timestamping, or file writes.

```mermaid
flowchart LR
    subgraph ControlPlane["Control plane"]
        VC["validate_config(...)"]
        SM["StateManager"]
        SF["StatusFaultHub"]
    end

    subgraph DataPlane["Data plane"]
        MC["MonotonicClock"]
        UC["UartCapture[N]"]
        TC["TriggerSyncCapture[N]"]
        MX["StorageMux"]
        SD["SdWriter"]
    end

    VC -->|status/fault| SM
    SD -->|status/fault| SM
    Platform["Command/Platform Sources"] -->|status/fault| SM
    SM -->|arm/start/stop/fault_shutdown| UC
    SM -->|arm/start/stop/fault_shutdown| TC
    SM -->|start_session/stop_session| MX
    SM -->|start_session/stop_session| SD
    UC -->|status/fault| SM
    TC -->|status/fault| SM
    MX -->|status/fault| SM
    SM -->|all status/fault events| SF
    MC --> UC
    MC --> TC
```

## Modules

### Data Plane Modules

#### `UartCapture[N]`

One instance per UART source.

Responsibilities:

- arm the UART receive path in `ready`
- observe incoming bytes without committing records before `start`
- timestamp the first byte of each chunk
- accumulate bytes until idle-gap timeout or max chunk size
- emit completed UART records into a fixed output queue

Non-responsibilities:

- state transitions
- filesystem access
- knowledge of other sources

#### `TriggerSyncCapture[N]`

One instance per associated trigger/sync source.

Responsibilities:

- arm trigger output and sync input timing capture in `ready`
- ignore pre-start events for file logging
- timestamp trigger events when the output pulse is issued
- timestamp sync events at the detected GPIO edge
- include event class and edge metadata in emitted records

#### `MonotonicClock`

Shared service that provides a common monotonic microsecond timebase used by all capture modules.

#### `StorageMux`

Responsibilities:

- consume records from all per-source queues
- serialize records into the binary file packet format
- assemble fixed write-ready blocks for the SD writer
- forward serialized blocks into the SD-writer queue

The storage mux does not touch the filesystem directly.

#### `SdWriter`

Responsibilities:

- create and own the session log file
- write the fixed file header first
- append serialized blocks during `running`
- flush and close on `stop`
- raise faults on open, write, flush, close, or session-lifecycle failures

The SD writer owns file lifecycle, not packet semantics.

### Control Plane Modules

#### `StateManager`

Single authority for global state.

Responsibilities:

- hold the current state
- validate state transitions
- receive status events and fault events from modules
- track readiness prerequisites internally instead of relying on callers to decide when the system is ready
- emit coarse commands such as `arm`, `start_session`, `stop_session`, and `fault_shutdown`
- emit lifecycle status events that explain accepted and rejected transitions

The state manager decides. It does not inspect raw captured data and it does
not delegate transition criteria to callers.

#### `validate_config(...)`

Pure helper function or small set of pure helper functions called during `init`.

Responsibilities:

- validate compile-time constants
- validate cross-field consistency
- return success or a specific fault reason

Representative checks:

- enabled UART count is within supported range
- chunk sizes fit static buffers
- idle-gap thresholds are sane
- queue capacities are valid

#### `StatusFaultHub`

Central sink for status and fault messages from all modules.

Responsibilities:

- collect debug and fault events
- preserve the reason for failures
- retain the emitted event sequence so the system timeline can be reconstructed
- support later diagnostics

It is not part of the timing-critical path and capture must not depend on it.

## Event Model

Every module reports lightweight facts into the control plane.

`StatusEvent` contains:

- `origin`
- `code`

The event does not contain a lifecycle state snapshot. Lifecycle state is owned
only by `StateManager`.

Interpretation rules:

- modules emit local facts, not lifecycle conclusions
- `StateManager` interprets `(origin, code)` pairs into readiness, command, and lifecycle meaning
- `StateManager` emits its own status events for accepted transitions, rejected commands, and other lifecycle milestones
- `StatusFaultHub` records both module-originated facts and state-manager lifecycle events so postmortem inspection can reconstruct what happened

## Session Semantics

The system is armed in `ready`, but the active recording session begins only
after `StateManager` accepts a start-command status event.

Rules:

- a module emits a start-command status event
- `StateManager` captures a session start timestamp when it accepts that command
- only records with timestamps greater than or equal to the session start time are eligible for file logging
- any UART bytes or trigger/sync events observed before `start` are discarded
- a stop-command status event ends record acceptance and closes the session cleanly

This makes the session boundary explicit and ensures the file contains only in-session data.

```mermaid
sequenceDiagram
    participant SM as StateManager
    participant UC as Capture Modules
    participant MX as StorageMux
    participant SD as SdWriter

    SM->>UC: arm in ready
    Note over UC: Observe bytes and edges\nDiscard pre-start activity
    SM->>SD: start_session(session_start_ts)
    SD->>SD: create file + write header
    SM->>MX: enable record acceptance
    UC->>MX: records with ts >= session_start
    MX->>SD: serialized write blocks
    SM->>MX: stop_session
    SM->>SD: flush + close
```

## Record Model

The binary log contains a file header followed by a sequence of records.

### UART Data Record

Fields:

- record type
- sensor or port ID
- first-byte timestamp in microseconds
- payload length
- payload bytes
- record checksum

Behavior:

- record creation is triggered by idle-gap timeout or max chunk size
- the timestamp remains the first-byte arrival time for the entire chunk

### Timing Event Record

Fields:

- record type
- sensor or port ID
- timestamp in microseconds
- event class: `trigger` or `sync`
- edge: `rising` or `falling` where applicable
- record checksum

Behavior:

- trigger timestamps correspond to output pulse issue time
- sync timestamps correspond to detected input-edge time

```mermaid
flowchart TD
    File["Binary log file"]
    Header["Header\nmagic | version | length | session_start_ts | decoder-required config summary | checksum"]
    UART["UART_DATA record\nrecord_type | port_id | first_byte_ts_us | payload_length | payload | checksum"]
    EVT["TIMING_EVENT record\nrecord_type | port_id | timestamp_us | event_class | edge | checksum"]

    File --> Header
    File --> UART
    File --> EVT
```

## File Format

The file format is a custom fixed binary format.

### Header

The file starts with a small fixed header written by `SdWriter`.

Header fields:

- magic bytes
- header version
- header length
- session start timestamp
- only those configuration summary fields that a specific offline decoder version requires
- header checksum

`header length` allows versioned parsers to determine where the header ends for any supported version, even if later versions add or remove optional metadata fields.

The format definition is shared by the data plane, but the physical act of writing the header belongs to `SdWriter`.

### Records

Records are fixed-schema binary packets defined by record type. The format remains intentionally narrow and explicit for v1. No nanopb or general serialization framework is used.

### Ordering

Records are written in the order `StorageMux` dequeues and serializes them. The firmware does not attempt to globally reorder records by timestamp at runtime.

Timestamps remain the source of truth for offline reconstruction.

## Queueing Model

The queueing model is strictly bounded and allocation-free in the hot path.

### Source Side

- each `UartCapture[N]` owns a fixed chunk buffer and a fixed output queue
- each `TriggerSyncCapture[N]` owns a fixed output queue
- completed records must enqueue immediately

### Storage Side

- `StorageMux` consumes source queues and produces fixed write blocks
- `SdWriter` consumes a single fixed queue of serialized write blocks

### Capacity Rule

If any source queue or the SD-writer queue cannot accept a record or block immediately, the system raises an irrecoverable fault. The design never silently drops data to preserve continued operation.

```mermaid
flowchart LR
    subgraph Sources["Per-source capture side"]
        U1["UartCapture[0..N]\nfixed chunk buffer\nfixed record queue"]
        T1["TriggerSyncCapture[0..N]\nfixed record queue"]
    end

    MX["StorageMux\nserialize records\nassemble write blocks"]
    WQ["SdWriter queue\nfixed-capacity blocks"]
    SD["SdWriter\nfile open/write/flush/close"]
    FLT["faulted"]

    U1 --> MX
    T1 --> MX
    MX --> WQ
    WQ --> SD
    U1 -. enqueue fail .-> FLT
    T1 -. enqueue fail .-> FLT
    MX -. writer queue full .-> FLT
    SD -. storage failure .-> FLT
```

## Hardware Execution Plan

The first prototype must be viable on the ESP32-P4 hardware without premature optimization. The execution plan therefore favors simple mechanisms first, while preserving clean seams for later upgrades if measurement shows real bottlenecks.

### Peripheral Choices

The planned peripheral set for v1 is:

- `UART0` reserved for console, boot logs, and recovery
- `UART1`-`UART4` used for the four sensor ports
- one shared `GPTimer` used as the canonical monotonic microsecond timebase
- GPIO edge interrupts for `SYNC` capture
- GPIO output for trigger generation in v1
- `SDMMC_HOST_SLOT_1` for the onboard TF slot

Upgrade seams are intentionally preserved:

- UART receive starts with ESP-IDF driver events, but the backend should be replaceable by DMA later
- trigger output starts as software-driven, but the interface should allow a later `GPTimer + ETM` implementation
- sync capture starts with GPIO ISR timestamping, but the architecture should allow later hardware-assisted capture if required

### Core and Priority Plan

The LP core is not used in v1. All application logic runs on the two HP cores.

Core split:

- `capture-oriented core`
  - shared `capture_task`
  - UART RX-related wakeups
  - sync GPIO ISR handling
  - trigger issue path
  - timer reads and chunk-state updates
- `storage/control-oriented core`
  - `storage_mux_task`
  - `sd_writer_task`
  - `StateManager`
  - `StatusFaultHub`

Priority model:

- `HIGH`
  - `capture_task`
  - `storage_mux_task`
- `LOW`
  - `sd_writer_task`
  - control and state handling
  - status and fault handling

This keeps the scheduler simple and biases CPU time toward preserving ingest integrity.

```mermaid
flowchart LR
    subgraph Core0["HP Core: capture-oriented"]
        CT["capture_task\nHIGH"]
        ISR1["UART RX wakeups"]
        ISR2["SYNC GPIO ISR"]
        TRG["Trigger issue path"]
        CLK["GPTimer reads"]
    end

    subgraph Core1["HP Core: storage/control-oriented"]
        MXT["storage_mux_task\nHIGH"]
        SWT["sd_writer_task\nLOW"]
        STM["StateManager\nLOW"]
        SFT["StatusFaultHub\nLOW"]
    end

    ISR1 --> CT
    ISR2 --> CT
    TRG --> CT
    CLK --> CT
    CT --> MXT
    MXT --> SWT
    STM --> CT
    STM --> MXT
    STM --> SWT
    CT --> SFT
    MXT --> SFT
    SWT --> SFT
```

### Task Model

#### `capture_task`

Single shared high-priority task for all sensor ports.

Responsibilities:

- consume ESP-IDF UART driver events for all active sensor UARTs
- drain available UART bytes from the signaled port
- create and update per-port chunk state
- assign `first_byte_timestamp` when a chunk begins
- update `last_byte_timestamp` as bytes arrive
- flush chunks on max-size reach
- flush chunks on idle-gap expiry
- consume sync and trigger work items from their fixed queues
- emit completed source records into per-source record queues

Idle-gap handling for v1 is inline in the task loop rather than driven by a separate periodic timer wakeup. After each wakeup and batch of work, the task performs a lightweight scan of active per-port chunk state and flushes any chunk whose idle-gap threshold has expired.

#### `storage_mux_task`

High-priority task responsible for keeping source queues drained.

Responsibilities:

- consume per-source UART and timing-event record queues
- serialize records into the binary file format
- assemble fixed SD-write blocks
- enqueue those blocks to the SD-writer queue

#### `sd_writer_task`

Low-priority task responsible for file operations only.

Responsibilities:

- create the session file
- write the file header
- append serialized write blocks
- flush and close on stop
- fault on any storage error

#### Control and Status Work

Low-priority control-plane work may stay as one or two small tasks as long as the interface boundaries remain clear.

- `StateManager` owns state transitions and session commands
- `StatusFaultHub` collects status and fault reports

Neither must sit in the hot path for data capture.

### Interrupt and Handoff Rules

Interrupts should create work, not perform policy.

Rules:

- timing-critical interrupts should prefer the capture-oriented core where ESP-IDF allows
- ISR bodies must stay minimal
- any ISR-to-task handoff uses fixed-capacity queues or equivalent `_FromISR` primitives
- ISR work must never perform serialization, checksum generation, logging, or filesystem access

Expected interrupt behavior:

- UART RX path
  - use ESP-IDF UART driver events for v1 wakeups
  - `capture_task` performs the actual chunk-management logic
- `SYNC` GPIO ISR
  - read the shared timestamp source
  - capture port ID, event class, and edge
  - enqueue a compact work item
- trigger issue path
  - issue the GPIO transition in the high-priority flow
  - capture the timestamp and enqueue a compact work item

### Memory Placement Rules

The default memory rule is internal SRAM first, with PSRAM used only later for proven-safe bulk storage.

Must remain in internal SRAM:

- ISR-facing state
- per-port chunk state
- source record queues
- ISR-to-capture work queues
- storage-mux input queues
- queue control structures and metadata
- compact event and record descriptors
- any state required to fault cleanly when queue send fails

Must be IRAM-safe where required:

- ISR entry points
- tiny helper functions called directly by those ISRs

Candidate for PSRAM later, only after measurement:

- large SD-write aggregation buffers
- larger serialization scratch buffers
- noncritical diagnostic history

```mermaid
flowchart TB
    subgraph Internal["Internal SRAM / ISR-safe working set"]
        CS["Per-port chunk state"]
        IWQ["ISR-to-capture work queues"]
        SRQ["Per-source record queues"]
        SWQ["SD write-block queue"]
        QMD["Queue metadata and control"]
        EVT["Compact event/record descriptors"]
    end

    subgraph PSRAM["PSRAM candidates after measurement"]
        AGG["Large SD aggregation buffers"]
        SCR["Serialization scratch buffers"]
        DBG["Noncritical diagnostic history"]
    end

    CT["capture_task"] --> CS
    CT --> IWQ
    CT --> SRQ
    MXT["storage_mux_task"] --> SRQ
    MXT --> SWQ
    MXT --> EVT
    SWT["sd_writer_task"] --> SWQ
    CT --> QMD
    MXT --> QMD
    SWT --> AGG
    MXT --> SCR
```

### Peripheral Ownership Table

| Resource | V1 Role | Primary Owner | Priority Band | Immediate State Placement | Notes |
|---|---|---|---|---|---|
| `UART0` | Console, boot logs, recovery | low-priority control/debug path | `LOW` | internal SRAM | Keep isolated from sensor traffic. |
| `UART1`-`UART4` | Sensor RX/TX ports | `capture_task` via ESP-IDF UART driver | `HIGH` | internal SRAM | Start with driver-event RX path; keep backend replaceable later. |
| `GPTimer` | Shared monotonic microsecond clock | capture-side timing logic | `HIGH` | timer handle/state in internal SRAM | Single canonical timebase for UART, trigger, and sync timestamps. |
| GPIO interrupt lines for `SYNC` | Rising/falling edge detection | ISR -> `capture_task` | `HIGH` | ISR work items and queue state in internal SRAM; ISR entry IRAM-safe | Start with GPIO ISR capture; preserve upgrade seam for hardware assist later. |
| Trigger output GPIO | Session trigger pulse generation | capture-side high-priority flow | `HIGH` | internal SRAM | Software-driven first; interface should allow later `GPTimer + ETM` backend. |
| `SDMMC_HOST_SLOT_1` | Onboard TF card logging | `sd_writer_task` | `LOW` | driver state internal SRAM; bulk buffers internal first | Native SDMMC, normal filesystem file. |
| Source record queues | Handoff from capture modules to mux | `capture_task` / `storage_mux_task` | `HIGH` | internal SRAM | Fixed-capacity, no dynamic allocation, overflow is fatal. |
| SD write-block queue | Handoff from mux to writer | `storage_mux_task` / `sd_writer_task` | `HIGH` -> `LOW` | internal SRAM first | Candidate for larger buffers later if measurement justifies it. |
| Per-port chunk state | Idle-gap chunk assembly | `capture_task` | `HIGH` | internal SRAM | Holds `active`, `first_byte_ts`, `last_byte_ts`, `length`, and write position. |
| `StatusFaultHub` transport | Debug/fault collection | low-priority control/status | `LOW` | internal SRAM | Must never be required for capture progress. |

## Fault Handling

All runtime faults are irrecoverable in v1.

Representative fault sources:

- UART source queue overflow
- trigger/sync queue overflow
- SD-writer queue overflow
- file open failure
- file write or flush failure
- invalid internal record state
- invalid session lifecycle handling
- configuration validation failure

Any such condition raises a fault event and transitions the system into `faulted`.

## Verification Strategy

### Host-Side Verification

Host tests should cover:

- `validate_config(...)`
- state transition logic
- session boundary rules
- record serialization
- header serialization and checksum validation
- checksum verification helpers

### Hardware Verification

The mock test rig should verify:

- complete UART-byte capture across all enabled ports
- correct first-byte timestamp semantics under idle-gap chunking
- correct trigger and sync edge capture
- correct session boundaries with no pre-start or post-stop records
- fail-fast behavior when storage is intentionally stalled or removed

Timestamp accuracy is validated independently with a logic analyzer or oscilloscope.

## Explicit V1 Non-Goals

- runtime configuration loading
- silent degradation or lossy fallback modes
- in-firmware global record reordering by timestamp
- fault recovery without reset
- general-purpose serialization frameworks
- expanded metadata not required for immediate offline decoding
