# Design Document
## ESP32-P4 Nano Robust UART Sensor Logger

**Version:** 0.1  
**Status:** Draft  
**Last Updated:** 2026-03-12

---

## 1. Purpose

This document defines a concrete system design for implementing the UART sensor logger described in [PRD_embedded_sensor_hub.md](/Users/timmagoun/Projects/embedded_ins_daq/PRD_embedded_sensor_hub.md) using an ESP32-P4 Nano as the main controller.

The design is intentionally biased toward reliable capture, deterministic timing, and recoverable SD logging. Optional parsing and convenience features are isolated from the capture path so they cannot compromise data preservation.

---

## 2. Design Goals

The system shall:

- capture UART traffic from at least 4 sensors concurrently
- timestamp SYNC edges against one shared microsecond device clock
- generate per-port trigger outputs with bounded jitter
- preserve raw UART bytes even when framing is disabled
- write authoritative binary session logs to SD card
- surface data loss and storage faults explicitly
- operate fully without network connectivity

---

## 3. Key Assumptions

- The target platform is the Waveshare `ESP32-P4-NANO` development board, built around an `ESP32-P4NRW32` and an onboard `ESP32-C6` companion chip.
- The ESP32-P4 is the main MCU for capture, timing, local control, and storage. The onboard ESP32-C6 is not required for this product revision and shall remain unused unless a future revision adds wireless features.
- The board exposes `28` programmable GPIOs on its headers rather than the full SoC pin set. The design therefore targets the `ESP32-P4-NANO` as a development platform and assumes a simple external sensor-interface board or harness rather than direct production deployment from the dev board alone.
- SD logging uses the onboard TF card slot in native `SDMMC` mode instead of SPI SD mode to maximize sustained write bandwidth and reduce CPU load.
- The design uses 4 of the ESP32-P4's 5 standard UART controllers for sensors and reserves `UART0` for console/download during development unless bring-up shows a different mapping is preferable.
- GPIO assignment shall be finalized only after reconciling the exposed header pins with the board's onboard peripherals and the final sensor connector scheme.

---

## 4. System Overview

### 4.1 Functional Blocks

The system is divided into six major blocks:

1. **Capture front end**
   Receives UART data and SYNC edges from 4 sensor ports.
2. **Timebase and trigger engine**
   Maintains the microsecond device clock and schedules trigger pulses.
3. **Capture buffering**
   Holds UART bytes and event records in RAM so capture is not blocked by SD latency.
4. **Persistent logging**
   Writes a binary session log and a human-readable status log to SD card.
5. **Control and configuration**
   Loads configuration from SD, validates it, and starts/stops sessions locally.
6. **Sensor preparation**
   Applies per-sensor initialization, readiness checks, and arming steps before recording begins.

### 4.2 Design Principle

The capture path is strictly separated from optional interpretation:

- raw UART bytes are always captured first
- framing runs only on copied data after capture
- SD writes happen from dedicated log buffers
- status and diagnostics never run in interrupt context
- sensor initialization is completed before the session enters active recording
- sensor-specific setup logic never bypasses the common session-start gate

This directly supports the PRD requirement that the logger remain a logger first.

---

## 5. Hardware Architecture

## 5.1 Main Processing Board

The reference implementation uses the `ESP32-P4-NANO` development board as the main processing and storage platform.

The overall system still requires external sensor wiring and power distribution for:

- 4 sensor ports
- 5 V power input
- 3.3 V regulated sensor rail
- session control button if the onboard buttons are not reused
- status LED if the onboard LED is not reused
- USB connection for firmware loading and local console

The ESP32-P4 Nano is responsible for:

- UART receive/transmit handling
- GPIO interrupt handling for SYNC inputs
- trigger scheduling and output
- SD card file management
- configuration parsing
- status reporting

On the actual Waveshare `ESP32-P4-NANO` board, several required functions already exist as onboard resources:

- TF card slot using `SDIO/SDMMC`
- USB-C UART/programming port
- USB OTG Type-A port
- 100 Mb Ethernet
- onboard `16 MB` NOR flash
- onboard `32 MB` package PSRAM
- an onboard `ESP32-C6` connected as a wireless companion

These onboard features are helpful for development, but Ethernet, Wi-Fi, and display/camera features remain out of scope for this logger revision.

## 5.2 Sensor Ports

Each of the 4 sensor ports exposes:

- `VCC` = 3.3 V sensor supply
- `GND`
- `RX` = hub receive from sensor transmit
- `TX` = hub transmit to sensor receive
- `SYNC` = configurable input or output

Recommended connector labeling is `PORT1` through `PORT4`, with silk indicating UART direction from the hub point of view.

## 5.3 UART Physical Design

Preferred implementation:

- use 4 of the ESP32-P4's 5 hardware UART controllers for sensor ports
- route each channel directly at 3.3 V logic level
- use interrupt-driven RX with FIFO service in revision 1
- reserve the remaining standard UART for local console, flashing, or service access during development

Because the ESP32-P4 already provides 5 standard UART controllers, an external multi-UART bridge should not be the default architecture. It becomes a fallback only if the `ESP32-P4-NANO` header pin exposure or peripheral conflicts prevent routing 4 sensor UARTs plus the required SYNC and control lines cleanly.

## 5.4 SYNC Input Design

Each `SYNC` line shall support:

- input mode with edge interrupt
- output mode for trigger generation
- protection and series resistance appropriate for external cabling

For input mode, the hardware path shall avoid RC filtering that would materially degrade edge timing. Any input conditioning must preserve sub-microsecond edge detect behavior at the MCU pin.

Because the ESP32-P4 supports a GPIO matrix, UART and timing-related signals can be routed flexibly. Even so, the final pin map should favor:

- short and clean routing for SYNC inputs
- isolation from noisy high-speed interfaces such as Ethernet, USB, and display signals
- avoiding pins already committed to the TF slot or other onboard functions

## 5.5 Trigger Output Design

Each `SYNC`-as-output line shall be driven by a push-pull 3.3 V GPIO path. Trigger outputs are intended for logic-level timing signals, not for directly driving heavy loads.

## 5.6 Power Design

The system power design shall generate a regulated 3.3 V rail for sensors and any required interface circuitry. This design defines:

- **Per-port current limit:** 250 mA
- **Total sensor power budget:** 1000 mA continuous across all ports

Implementation details:

- per-port current limiting using load switches or eFuse devices
- fault indication available to firmware if practical
- bulk capacitance placed near the sensor power distribution region

These limits satisfy the PRD requirement to define and document the available power budget. Final schematic review shall verify regulator thermals, derating, and any required aggregate-current protection behavior.

## 5.7 Storage Interface

The SD card shall use the board's onboard TF slot in native `SDMMC` mode. ESP32-P4 documentation indicates that the SoC provides two `SDMMC` host slots, and at least one slot is routable through the GPIO matrix for standard SD-card use. This is the preferred storage path because it offers:

- higher sustained throughput
- lower CPU overhead than SPI SD
- better margin during worst-case UART traffic bursts

The design does not require UHS-I operation. Standard 3.3 V SD-card operation is sufficient for this logger.

---

## 6. Timing Architecture

## 6.1 Device Clock

The firmware maintains one monotonic 64-bit device clock with 1 us resolution for the entire session.

Design choice:

- use an ESP32-P4 hardware timer service such as `GPTimer` or `SYSTIMER` as the single source of truth
- expose timestamps as microseconds since boot or since timer initialization
- never adjust, discipline, or reset the timer during an active session

All binary log timestamps derive from this clock.

## 6.2 SYNC Edge Timestamping

For any port configured as SYNC input:

- GPIO interrupt fires on configured edge type
- ISR reads the current device clock immediately
- ISR pushes a compact edge event into a lock-free or interrupt-safe event queue

Each event records:

- port index
- edge polarity
- device-clock timestamp

This approach avoids polling and supports the PRD timing requirement.

## 6.3 Trigger Scheduling

For any port configured as trigger output:

- trigger pulses are driven by a hardware timer alarm schedule
- pulse start and pulse end are each explicit scheduled actions
- GPIO toggles occur in a dedicated timing ISR or equivalent low-latency handler

Each trigger emission also creates a log event containing:

- port index
- timestamp of pulse assertion

To meet the 5 us jitter requirement, trigger generation shall have higher priority than framing, status printing, and file-system formatting work.

---

## 7. Data Path Architecture

## 7.1 Capture Pipeline

Per-port UART receive flow:

1. UART hardware receives bytes without software polling.
2. RX interrupt-driven FIFO service moves bytes into a per-port circular buffer.
3. A capture worker packages buffered bytes into UART log records.
4. Log records are appended to a shared binary-log staging queue.
5. SD writer task flushes staged records to the session file.

Per-port SYNC flow:

1. Edge interrupt occurs.
2. ISR timestamps the edge.
3. Event is queued to the binary-log staging queue.

Per-port trigger flow:

1. Timer alarm asserts output.
2. ISR timestamps the assertion.
3. Trigger record is queued.
4. A later timer alarm deasserts the output.

## 7.1.1 UART Timestamp Model

Revision 1 constrains UART capture to an ISR/FIFO-driven timestamp model.

Rules:

- the UART ISR captures a timestamp when it first services a newly started RX chunk from the hardware FIFO
- that chunk-start timestamp becomes the timestamp for the resulting UART data record
- additional bytes appended to the same chunk inherit that chunk-start timestamp
- a new chunk begins only at a documented chunk boundary such as an idle gap, buffer handoff, or explicit flush boundary

This timestamp represents when firmware first serviced the chunk from the UART peripheral. It is not a claim of per-byte wire time.

## 7.2 Buffering Strategy

The design uses three buffer layers:

- **Hardware FIFO:** absorbs immediate peripheral bursts
- **Per-port circular RX buffer:** absorbs SD and scheduler latency
- **Shared log staging buffers:** decouple record production from SD file writes

### Proposed Per-Port UART Buffer Size

Worst-case UART load per port at `921600` baud with 10 bits per byte is about `92.16 kB/s`.

To retain at least `500 ms` of data per port:

- required minimum buffer per port is about `46.1 kB`

Design target:

- allocate `64 kB` RX circular buffer per port

For 4 ports, total UART RX retention is:

- `256 kB` dedicated to raw receive buffering

This gives margin above the PRD minimum and simplifies buffer sizing.

This memory budget is practical on ESP32-P4 because the SoC includes substantial on-chip RAM and the `ESP32-P4-NANO` variant includes `32 MB` of package PSRAM. Time-critical ISR data structures should remain in internal memory, while larger non-critical staging buffers may use PSRAM only after throughput testing confirms that latency remains acceptable.

### Shared Logging Buffers

Recommended initial sizing:

- `2 x 128 kB` binary log staging buffers
- `1 x 16 kB` status log buffer

Double buffering allows one binary block to be written while the next is filled.

## 7.3 Overflow Handling

If any buffer fills beyond safe limits:

- increment the affected per-port loss counter
- emit an explicit loss/status record
- continue logging subsequent data where possible

The system shall never silently discard required records.

---

## 8. Firmware Architecture

## 8.1 Task Model

The firmware is organized into a small set of clear modules:

- `config_manager`
- `session_controller`
- `sensor_manager`
- `clock_service`
- `uart_capture`
- `sync_capture`
- `trigger_engine`
- `framing_service`
- `binary_logger`
- `status_logger`
- `sd_storage`
- `health_monitor`

Recommended execution model:

- interrupts for UART RX service, SYNC edge capture, and trigger timing
- medium-high-priority sensor setup task for bounded pre-record initialization and readiness verification
- high-priority capture tasks for draining ISR queues
- medium-priority storage task for binary/status file writes
- low-priority optional framing and console/status formatting

Since ESP32-P4 has dual high-performance cores plus an LP core, the firmware should keep all active-session capture and storage work on the HP cores. The LP core is not required for compliance and should not own any timing-critical logging path.

## 8.2 Module Responsibilities

### `config_manager`

- reads structured config file from SD card at startup
- validates all per-port settings
- rejects invalid configurations before session start

### `session_controller`

- manages `idle`, `armed`, `recording`, `stopping`, and `faulted` states
- creates unique session filenames
- writes session start and stop records
- accepts session start only after required sensors report `READY`

### `sensor_manager`

- owns sensor lifecycle state for each configured port
- maps each port to a sensor type and sensor profile
- executes probe, configure, verify-ready, arm, and teardown stages
- publishes normalized readiness results to `session_controller`
- allows ports to be marked as `raw_capture_only` when no initialization is required

The `sensor_manager` provides one shared lifecycle contract for heterogeneous devices such as GNSS receivers and IMUs. Each sensor type can use different command sequences, timeouts, and verification rules, but each must report progress through the same normalized state model:

- `UNCONFIGURED`
- `PROBING`
- `CONFIGURING`
- `WAITING_READY`
- `READY`
- `FAILED`

Session-start rule:

- every enabled sensor marked `required_for_start = true` must reach `READY` before recording begins
- any enabled port marked `raw_capture_only` may skip active initialization
- any required sensor that reaches `FAILED` prevents session start and reports a local fault/status event

### `clock_service`

- owns the monotonic microsecond timer
- provides timestamp reads to ISRs and tasks

### `uart_capture`

- configures each UART port
- owns per-port RX buffers and counters
- reports overflow conditions

### `sync_capture`

- configures SYNC GPIO direction and interrupt mode
- timestamps input edges

### `trigger_engine`

- configures active trigger outputs
- schedules periodic pulses and pulse width timing

### `binary_logger`

- packages versioned binary records
- computes record header and integrity fields
- appends to staging buffers

### `status_logger`

- emits human-readable major events and fault messages
- never blocks the capture path

### `framing_service`

- optionally identifies protocols such as NMEA or UBX
- attaches framing metadata without replacing raw payload logging

### `sd_storage`

- mounts SD card
- creates session directory/files
- flushes staged data
- reports storage faults

### `health_monitor`

- tracks queue depth, buffer watermarks, SD latency, and loss counters
- drives status LED patterns and fault transitions

---

## 9. Logging Design

## 9.1 File Set Per Session

Each session produces:

- one binary session log, for example `SESSION_YYYYMMDD_HHMMSS.bin`
- one human-readable status log, for example `SESSION_YYYYMMDD_HHMMSS.status.txt`

A boot/session counter file on SD may also be used to guarantee uniqueness across power cycles.

## 9.2 Binary Log Format

The binary log is the system of record.

Recommended file structure:

- **File header**
  - magic number
  - format version
  - device identifier
  - session creation timestamp in device-clock domain
  - configuration summary or config hash
- **Record stream**
  - fixed header plus variable payload
- **Optional end marker**
  - session clean-stop record

### Record Header

Each binary record should contain:

- record type
- record length
- timestamp in device-clock microseconds
- source port or source module identifier
- flags
- payload CRC or record CRC

### Required Record Types

- session start
- session stop
- UART raw data
- UART overflow/loss event
- SYNC edge
- trigger pulse
- configuration accepted
- configuration error
- SD/storage fault
- internal warning/status event

### UART Data Record

Recommended payload:

- port index
- UART settings snapshot or configuration ID
- byte count
- raw payload bytes
- optional framing metadata block

The timestamp for a UART data record shall be defined consistently. Recommended rule:

- timestamp each UART data record with the device-clock value captured when the UART ISR first services that chunk from the RX FIFO

This is metadata for chunk timing, not a claim of per-byte wire time.

## 9.3 Recovery and Power-Loss Robustness

To support recovery from unclean shutdown:

- records are length-delimited
- each record includes integrity checking
- writes are flushed in bounded chunks
- the session is considered clean only if a stop record is present

An external reader can scan until the last valid record and detect incomplete termination.

## 9.4 Human-Readable Status Log

The status log shall contain concise entries for:

- boot
- SD mount result
- configuration validation result
- session start/stop
- UART overflow events
- buffer exhaustion
- SD write failures
- trigger enable/disable
- notable internal warnings

---

## 10. Configuration Design

## 10.1 File Format

The startup configuration file shall be JSON for simplicity and ease of editing in the field.

Suggested filename:

- `config/logger_config.json`

## 10.2 Configuration Content

Minimum configuration fields:

- global session settings
- per-port UART settings
- per-port capture mode
- per-port sensor type
- per-port sensor profile or setup recipe
- per-port sensor start policy
- per-port SYNC direction
- trigger rate
- trigger pulse width
- edge selection for SYNC input

### Example Structure

```json
{
  "session": {
    "autostart": true
  },
  "ports": [
    {
      "port": 1,
      "uart": {
        "baud": 115200,
        "data_bits": 8,
        "parity": "none",
        "stop_bits": 1
      },
      "capture_mode": "raw",
      "sensor_type": "gnss",
      "sensor_profile": "ubx_rover_default",
      "required_for_start": true,
      "sync_mode": "input",
      "sync_edges": "rising"
    },
    {
      "port": 2,
      "uart": {
        "baud": 921600,
        "data_bits": 8,
        "parity": "none",
        "stop_bits": 1
      },
      "capture_mode": "raw",
      "sensor_type": "imu",
      "sensor_profile": "imu_200hz_default",
      "required_for_start": true,
      "sync_mode": "output",
      "trigger_hz": 1000,
      "trigger_pulse_width_us": 50
    }
  ]
}
```

## 10.3 Validation Rules

Configuration validation shall reject:

- unsupported baud rate or UART format
- invalid port numbers
- unknown sensor type
- unknown sensor profile
- `sync_mode = input` combined with trigger settings
- `sync_mode = output` combined with input-edge settings
- trigger rates above 2 kHz
- zero or invalid pulse width
- malformed or duplicate port definitions

Invalid configuration prevents session start and is reported locally.

---

## 11. Session Control and Local Operation

The design supports local-only operation through:

- optional auto-start on boot when configuration is valid
- one physical button for start/stop or arm/disarm control
- USB serial console commands for service and debug
- status LED indicating `idle`, `recording`, and `fault`

On the Waveshare board, `UART0` is the natural development console path and should remain available for boot logs, field diagnostics, and recovery commands during bring-up.

Recommended behavior:

- short press toggles session state when SD, config, and required sensor readiness are valid
- long press forces stop if safe shutdown is needed
- console commands remain optional and non-authoritative during active fault states

Session startup sequence:

1. validate configuration
2. mount SD and prepare session files
3. run sensor initialization for all enabled ports
4. verify all required sensors are `READY`
5. arm trigger outputs and SYNC roles
6. enable active recording

---

## 12. Error Handling and Fault Policy

## 12.1 Fault Classes

The firmware distinguishes:

- configuration faults
- UART overflow faults
- log queue overflow faults
- SD mount/open/write faults
- trigger scheduling faults
- internal software assertion faults

## 12.2 Fault Response

Policy:

- configuration fault: do not start session
- UART overflow: log event, increment loss counter, continue if possible
- SD throughput lag: continue until RAM retention is exhausted
- RAM retention exhausted: log explicit data-loss event if the binary log is still writable, mark the session degraded, and continue best-effort capture only while enough system state remains intact to do so
- SD write failure: stop claiming healthy persistent logging immediately, then mark the session degraded or faulted according to recoverability

### Degraded Versus Faulted Behavior

`DEGRADED` means:

- the device is still operating
- at least part of the capture path or persistence path remains usable
- one or more PRD guarantees are no longer fully being met
- the firmware may continue best-effort recording to preserve whatever data it still can

Conditions that should enter `DEGRADED`:

- SD latency has consumed most retention headroom but writes are still succeeding
- a UART overflow or pipeline overflow has occurred but the binary log path remains writable
- the status log path has failed while the binary log path remains healthy
- a recoverable storage sync or transient write issue has occurred and retry policy is still active

`FAULTED` means:

- the system can no longer make a defensible claim of compliant active recording
- required authoritative persistence is unavailable, or internal state is no longer sufficiently trustworthy
- the firmware should stop an active session or refuse to start one until the fault is cleared

Conditions that should enter `FAULTED`:

- configuration is invalid
- SD mount or session-file open fails before recording starts
- storage failure is persistent enough that the authoritative binary log can no longer be written reliably
- internal assertions or unrecoverable software state corruption occur
- startup readiness for a required sensor fails and session start policy can no longer be satisfied

Operational policy:

- `DEGRADED` shall latch a sticky health flag, increment counters, drive a distinct local warning indication, and emit a binary fault or status record whenever the binary log is still writable
- `DEGRADED` may continue recording best-effort data, but status output and session metadata shall make clear that capture integrity or persistence guarantees have been compromised
- `FAULTED` shall prevent entry into `RECORDING` or force transition out of `RECORDING`
- after entering `FAULTED`, the firmware should close files and stop capture cleanly where possible, while still preferring preservation of already-buffered data over cosmetic shutdown behavior

This matches the PRD requirement that data loss and storage faults be explicit.

---

## 13. Optional Framing Support

Built-in framing is optional and must not sit in the critical capture path.

Recommended initial support:

- `raw`
- `NMEA`
- `UBX`

Framing implementation rule:

- framed metadata is derived from copied UART payload already preserved in raw form

If framing falls behind or fails, raw logging continues unchanged.

Sensor-specific initialization is distinct from framing. A sensor driver may use protocol-aware control messages before recording begins, but once the session starts, raw byte preservation remains authoritative and independent of parser success.

---

## 14. Verification Strategy

## 14.1 Unit Test Targets

The following modules shall be unit-testable in isolation:

- binary record encoder/decoder
- configuration validator
- UART buffer accounting
- overflow detection logic
- trigger schedule calculations
- framing state machines

## 14.2 Hardware/Integration Tests

Required bench tests:

- 4-port UART soak at worst-case configured throughput
- simultaneous UART capture plus 2 kHz trigger generation
- SYNC input timestamp accuracy validation
- SD latency stress with fragmented and slower cards
- power-cycle and unclean-removal recovery
- invalid-config boot behavior

## 14.3 Acceptance Metrics

The implementation passes design intent when it demonstrates:

- no software polling for UART receive or SYNC capture
- at least 500 ms of buffered UART retention per port at configured maximum rate
- trigger jitter no worse than 5 us under concurrent logging load
- explicit detection and logging of loss conditions
- readable recovery of logs after unclean power removal

---

## 15. Open Design Decisions

The following items should be finalized during schematic and firmware bring-up:

- exact header-pin assignment for 4 sensor UARTs plus 4 SYNC lines on the `ESP32-P4-NANO`
- exact SD card interface routing and signal integrity constraints
- whether per-port power fault feedback is wired to the MCU
- final button/LED behavior and service-console command set
- exact binary log record layout and CRC choice
- whether large binary staging buffers should live in internal RAM, PSRAM, or a split arrangement after benchmarking
- the initial set of supported sensor drivers and their readiness criteria
- the timeout and retry policy for required sensor initialization

None of these open items change the core architecture.

---

## 16. Summary

This design fulfills the PRD by treating the ESP32-P4 Nano as a dedicated capture and logging controller with:

- isolated real-time capture paths
- a single microsecond device clock
- explicit buffering between capture and storage
- robust binary logging with recovery support
- optional parsing that cannot endanger raw data preservation

The result is a field logger optimized for integrity, traceability, and simple offboard analysis rather than feature breadth.
