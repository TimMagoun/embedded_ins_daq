# Data Contracts
## Shared Logical Interfaces Between Modules

Field names here are conceptual. Implementations may use different concrete type names as long as the same contracts are preserved.

---

## `RuntimeConfig`

Contains:

- global session settings
- per-port UART settings
- per-port capture mode
- per-port sensor type
- per-port sensor profile
- per-port required-for-start policy
- per-port initialization timeout/retry policy
- per-port SYNC mode
- per-port trigger configuration
- framing enable/disable settings
- logging policy parameters

Contract:

- immutable once session start is accepted

---

## `SessionInfo`

Contains:

- session identifier
- session start timestamp in device-clock domain
- filename base
- configuration hash
- build/version identifiers

Contract:

- created once by `session_controller`
- treated as read-only by all consumers
- filename base must be unique across power cycles when persisted to SD

---

## `HealthSnapshot`

Contains:

- current health classification
- sticky degraded flag
- queue and buffer watermark summary
- SD write-latency summary
- per-port loss counters

Contract:

- health classification is derived from normalized faults and runtime watermarks
- health state does not replace the session lifecycle state machine

---

## `SensorProfile`

Contains:

- sensor type identifier
- profile name
- protocol family
- configuration recipe reference
- retry policy
- timeout policy
- `required_for_start` flag
- `raw_capture_only` flag

Contract:

- immutable during a session
- each enabled port resolves to exactly one effective sensor profile

---

## `SensorReadinessSummary`

Contains:

- per-port lifecycle state
- per-port failure reason if any
- aggregate `all_required_ready` flag

Contract:

- consumed by `session_controller` as the gating decision for recording start

---

## `UartChunkDescriptor`

Contains:

- port index
- chunk start timestamp
- byte count
- pointer/reference to contiguous byte region
- flags indicating overflow boundary or chunk conditions

Contract:

- points to valid raw bytes until consumed by record building
- ownership of the backing bytes remains with `uart_capture_service`

---

## `FrameInputChunk`

Contains:

- port index
- timestamp reference
- copied byte payload
- framing mode

Contract:

- framing receives a copy or isolated buffer view
- framing may process asynchronously without stalling capture

---

## `BinaryRecordEnvelope`

Contains:

- record type
- record version
- timestamp
- source ID
- payload length
- payload reference
- integrity metadata

Contract:

- once appended to the pipeline, the envelope is immutable
- append order defines on-disk order

---

## `FaultEvent`

Contains:

- fault class
- severity
- port or subsystem origin
- timestamp
- optional counters or causal metadata

Contract:

- generated immediately when a fault is detected
- both binary and human-readable representations derive from the same normalized event
