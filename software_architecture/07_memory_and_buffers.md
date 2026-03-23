# Memory And Buffers

## Placement Rules, Buffer Ownership, and Queue Inventory

______________________________________________________________________

## Memory Regions

Use three logical memory categories:

1. **Internal low-latency RAM**
   For ISR-visible structures and hot-path queues.
1. **Internal general RAM**
   For active task state, control structures, and moderate-size buffers.
1. **PSRAM**
   For large non-critical buffers only after validation.

______________________________________________________________________

## Mandatory Internal-RAM Residents

These must live in internal RAM:

- ISR event queues
- sensor lifecycle state tables
- sensor driver control state
- per-port circular buffer descriptors
- port counters and fault counters
- session state
- timer state
- binary/status queue metadata
- short-lived record build scratch space

______________________________________________________________________

## Preferred Internal-RAM Residents For Revision 1

These should start in internal RAM:

- per-port raw UART byte buffers
- binary staging buffers
- status staging buffers

Reason:

- revision 1 should optimize for predictability over memory abundance
- the design target is at least `500 ms` of UART retention per enabled port at its configured baud rate
- the reference sizing from the design doc is `64 kB` per enabled port, which exceeds the `921600` baud minimum-retention requirement with margin

______________________________________________________________________

## Candidate PSRAM Residents After Benchmarking

These may move to PSRAM if measured safe:

- secondary binary staging buffer
- framing work buffers
- long status text buffers
- copied parser input buffers

______________________________________________________________________

## Allocation Policy

- preallocate all recording-critical buffers before session start
- do not allocate from the heap in ISR context
- avoid unbounded dynamic allocation during recording
- prefer fixed-capacity pools and ring buffers

______________________________________________________________________

## Buffer And Queue Inventory

### Per-Port UART Buffers

For each port:

- one RX circular byte buffer
- one ISR-to-capture event queue
- one chunk assembly context
- one statistics block

Ownership:

- only `uart_capture_service` may mutate these structures

### SYNC Event Queue

For all SYNC inputs:

- one ISR-to-sync queue or one queue per port

Ownership:

- written by ISR layer
- drained by `sync_capture_service`

### Sensor Preparation State

For each enabled sensor port:

- one lifecycle state record
- one retry/timeout state block
- one driver binding reference

Ownership:

- written only by `sensor_manager`
- read by `session_controller` and reporting paths

### Trigger Event Queue

If trigger record generation is not completed entirely in the trigger path:

- one trigger-event queue from timing callback to trigger service

### Binary Pipeline Buffers

- producer queue for record envelopes
- active fill buffer
- flush-pending buffer

Ownership:

- append path writes only to the active buffer
- storage path reads only flush-pending buffers

### Status Pipeline Buffers

- bounded status event queue
- text staging buffer

Ownership:

- producers publish events
- `status_log_pipeline` serializes them
