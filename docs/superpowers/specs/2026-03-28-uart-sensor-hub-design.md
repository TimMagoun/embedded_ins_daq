# UART Sensor Hub Design

## Overview

This document defines the revision 1 design for a boot-autonomous UART sensor logger built on the `Waveshare ESP32-P4-NANO`.

The system is designed to:

- capture up to four UART sensor streams concurrently
- generate up to four independent trigger-output schedules
- timestamp external sync inputs such as GNSS `1PPS`
- log all timing and UART data against one monotonic device clock
- start automatically on boot from SD-card configuration
- preserve explicit evidence of loss or degradation under overload

The design is intentionally logger-first. Raw UART bytes and timing events are authoritative. Optional framing or higher-level interpretation is downstream and must not interfere with capture.

## Goals

- support up to four sensor ports at up to `921600` baud each
- support per-port timing role selection
- support per-port trigger period and pulse width
- support per-port sync-input edge selection
- maintain one free-running monotonic clock for the full session
- avoid live clock discipline or phase correction during recording
- continue through recoverable overload while recording explicit fault and loss events

## Out Of Scope For Revision 1

- network control or cloud connectivity
- live clock discipline from GNSS or any other external source
- blocking session start on sensor readiness
- production hardware beyond the `ESP32-P4-NANO` plus external harness or interface board
- optional framing or parsing in the hot capture path

## System Model

The ESP32-P4 Nano is the timing master and storage controller for the session.

Revision 1 assumes one supported board configuration. The firmware owns one fixed compile-time mapping from logical ports to UART and GPIO resources. There is no runtime board-selection or board-capability negotiation layer.

On boot, the firmware:

1. initializes the platform
1. mounts the SD card
1. loads configuration
1. validates per-port timing and UART settings
1. opens the session files
1. arms UART capture and timing I/O
1. starts logging automatically

The device clock is internal and monotonic for the full session. External timing references such as GNSS `1PPS` are logged as observations only and correlated offline after collection.

## Sensor Port Model

Each sensor port exposes:

- `UART_RX`
- `UART_TX`
- `SYNC`
- `3.3V`
- `GND`

Each port is configured independently with:

- `uart logging = enabled | disabled`
- `timing mode = none | sync | trigger`

If `timing mode = sync`, the port also defines:

- `sync_edge_mode = rising | falling | change`
- default `sync_edge_mode = rising`

If `timing mode = trigger`, the port also defines:

- `period_us`
- `pulse_width_us`

This model supports mixed sensor sets such as:

- GNSS with UART plus `1PPS` sync input
- IMU with UART plus trigger input driven by the hub
- sensors with UART plus data-ready or sync output into the hub

## Architecture

The recommended revision 1 architecture is a hybrid hardware-timing design:

- one authoritative monotonic timer for the full session
- native ESP32-P4 UART peripherals for sensor links
- GPIO interrupt capture for sync-input edges
- hardware-assisted trigger generation where the platform supports it cleanly
- software-owned scheduling with no task-context timing-critical GPIO writes

The firmware is partitioned into five runtime domains.

### 1. Clock And Timing Core

Responsibilities:

- own the monotonic `timestamp_us` domain
- validate trigger schedule parameters
- schedule per-port trigger assert and deassert events
- normalize timing events into log-ready records

Requirements:

- no timer reset, step, or live discipline during recording
- timing-critical state remains in internal RAM

### 2. UART Capture

Responsibilities:

- configure one hardware UART per enabled sensor port
- receive bytes via interrupt-driven service
- store bytes into per-port internal-RAM ring buffers

Requirements:

- ISR work stays minimal
- UART capture remains independent from SD write latency
- no parsing or framing in the ISR hot path

### 3. Sync Capture And Trigger Output

For `sync`:

- configure GPIO interrupt mode per port using `sync_edge_mode`
- timestamp edges immediately in ISR context
- record actual edge polarity in the event record

For `trigger`:

- represent each pulse as two scheduled edge events: assert and deassert
- support per-port period and pulse width
- prefer hardware-assisted output transitions over software-only task scheduling

### 4. Record Builder And Buffering

Responsibilities:

- merge UART chunks, sync edges, trigger events, and fault events into one binary stream
- maintain bounded staging buffers
- emit explicit loss records when overflow occurs

Requirements:

- hot-path queues and counters remain in internal RAM
- PSRAM may be used only for non-critical staging or analysis support

### 5. Storage And Session Control

Responsibilities:

- manage auto-start session lifecycle
- create session folders and files on SD
- write binary records and a compact text status log
- copy the active configuration into the session artifact set

Requirements:

- never block ISR capture paths
- detect and record SD-card backpressure and write failures explicitly

## Timing Architecture

All session records use one free-running device clock with microsecond resolution.

### Trigger Scheduling

Each trigger port uses an independent schedule referenced to the shared device clock.

Each pulse is modeled as:

- assert at `t0`
- deassert at `t0 + pulse_width_us`

The system must support up to four independently configured trigger schedules concurrently.

### Sync Input Capture

Each sync-input edge produces an event containing:

- `port_id`
- `timestamp_us`
- `edge_polarity`

The firmware must capture the actual observed polarity even when only one edge direction is configured. This helps diagnose inverted wiring, sensor misconfiguration, and unexpected pulse shapes offline.

### Clock Discipline Policy

The live clock is never phase-aligned, frequency-corrected, or stepped from GNSS or any other external source during recording.

GNSS `1PPS` and similar signals are used only for offline timestamp correlation after collection.

## Binary Log Design

The binary session log is the authoritative artifact for offline analysis.

It contains the following record classes.

### `session_start`

Fields include:

- firmware version or build identifier
- config hash
- session start time in device-clock units
- enabled port summary
- per-port UART and timing configuration summary

### `uart_data`

Fields include:

- `port_id`
- `timestamp_us`
- byte count
- raw payload bytes

Semantics:

- records contain byte chunks, not one record per byte
- `timestamp_us` is the capture time of the first byte in the chunk

### `sync_edge`

Fields include:

- `port_id`
- `timestamp_us`
- `edge_polarity`

### `trigger_event`

Fields include:

- `port_id`
- `timestamp_us`
- event kind: `assert` or `deassert`
- per-port sequence counter

These records provide proof of what the hub emitted, enabling offline comparison with sensor responses and observed sync signals.

### `fault_event`

Fields include:

- `timestamp_us`
- fault class
- severity
- affected port when applicable
- counters or detail values needed for offline diagnosis

### `session_end`

Fields include:

- stop reason
- per-port byte totals
- per-port timing-event totals
- loss counters
- storage health summary

## File Set

Each session should produce:

- one authoritative binary session log
- one small human-readable text status log
- one copied configuration snapshot

## Fault And Overload Behavior

The system must degrade explicitly rather than silently.

### Fault Classes

Port-local capture faults:

- UART hardware FIFO overflow
- per-port ring overflow

Timing faults:

- sync event queue overflow
- trigger scheduling miss
- trigger ISR latency beyond budget
- invalid trigger configuration such as `pulse_width_us >= period_us`

Storage faults:

- SD mount failure
- file open or create failure
- write error
- card removal
- sustained storage backpressure

System faults:

- staging buffer exhaustion
- internal RAM allocation failure at startup
- watchdog-related stalls
- panic or unrecoverable assertion

### Runtime Policy

- invalid configuration prevents session start
- port-local runtime faults degrade the affected port but do not automatically stop the session
- storage backpressure triggers RAM buffering while capacity remains
- if buffering is exhausted, the logger emits explicit loss records
- if authoritative binary logging or timing integrity is no longer credible, the session transitions to a faulted stop

### Session States

- `booting`
- `starting`
- `recording`
- `recording_degraded`
- `stopping`
- `faulted`

`recording_degraded` means the session remains useful but some port or storage degradation has occurred. `faulted` means the logger can no longer claim authoritative timing or durable session capture.

## Memory Placement Rules

- timing queues, UART rings, ISR counters, and other hot-path structures must be allocated in internal RAM
- PSRAM must not hold data structures that are required to meet `±1 us` timing goals
- SD writes occur only from task context

## Validation Strategy

Validation must prove both data-path correctness and timing accuracy.

### Host-Side Validation

Use host tests for:

- config parsing and validation
- trigger schedule math
- record encoding and decoding
- session-state transitions
- fault classification and degradation policy
- offline log parser behavior

### Device Validation

Use on-device tests for:

- one-port and four-port UART capture
- SD logging under sustained load
- sync-input edge capture
- trigger-output generation
- auto-start behavior
- fault handling for storage slowdown or removal

### Timing Validation

For sync-input accuracy:

- drive the input with a trusted pulse source
- compare logged timestamps against the reference generator
- measure offset and jitter across long runs

For trigger-output accuracy:

- observe output pins with a scope or logic analyzer
- compare actual edge times to scheduled edge times
- measure period accuracy, pulse-width accuracy, and jitter
- repeat under representative UART and SD load

For end-to-end sensor timing:

- issue a hub trigger to a sensor such as an IMU
- capture the resulting UART traffic and any returned timing signal
- compute latency offline from emitted trigger to observed sensor response

### Acceptance Gate

Revision 1 should not claim success unless it demonstrates:

- four-port UART logging up to `921600` baud
- independent trigger-output schedules across up to four ports
- sync-input capture on configured edge directions
- explicit recording of any loss or degradation
- measured `±1 us` timing performance at real pins under representative combined load

If real hardware measurements fail the `±1 us` requirement under representative load, that result must be treated as an architecture decision point rather than a firmware polish issue.

## Primary Engineering Risk

The main technical risk is not basic UART capture. It is proving `±1 us` trigger-output and sync-input timing performance on the ESP32-P4 Nano while multiple high-rate UART ports and SD logging are active.

The implementation plan should therefore treat early timing characterization as a go or no-go checkpoint for the selected architecture. If the platform cannot hold the timing target with the intended load, the project must either:

- relax the timing claim
- reduce concurrent load
- or move timing generation and capture to a stronger hardware path or external timing device
