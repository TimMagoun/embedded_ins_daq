# Product Requirements Document
## Multi-Sensor Data Acquisition & Logging Hub

**Version:** 0.8  
**Status:** Active  
**Last Updated:** 2026-03-12

---

## 1. Overview

The device is an embedded sensor hub that acquires data from multiple UART sensors simultaneously, captures SYNC/trigger/clock timestamps with microsecond precision, logs data to an SD card, and streams data over Wi-Fi and Ethernet. It also receives RTCM correction streams from an NTRIP caster and forwards them to connected GNSS sensors.

**Primary use cases:** GNSS/IMU fusion, robotics sensor synchronization, and field data logging.

---

## 2. Sensor Ports

The device shall provide at least **4 identical sensor ports**. Each port exposes a 5-pin interface:

| Pin | Signal | Description |
|-----|--------|-------------|
| 1 | VCC | 3.3 V supply, up to 200 mA continuous |
| 2 | GND | Ground |
| 3 | RX | Hub receive (sensor TX) |
| 4 | TX | Hub transmit (sensor RX) |
| 5 | SYNC | Trigger line, configurable as input or output per port |

All ports shall be functionally identical and capable of operating concurrently.

---

## 3. UART

- **REQ-UART-01:** Each port shall support asynchronous UART with configurable baud rate (9600 – 921600 bps), data bits (7/8), parity (none/even/odd), and stop bits (1/2).
- **REQ-UART-02:** Incoming bytes shall be captured into memory without stalling the CPU. Each port's receive buffer shall hold at least 500 ms of data at the configured baud rate without overflow.
- **REQ-UART-03:** Buffer overrun events shall be detected, logged, and reported in the telemetry stream.
- **REQ-UART-04:** The firmware shall support user-implemented sensor packet parsers. Adding a new parser shall not require modifying core firmware. Built-in parsers for NMEA 0183 and UBX binary shall be provided as reference implementations.
- **REQ-UART-05:** A parser shall be assignable to each port before a session starts, without a firmware recompile or device reboot.

---

## 4. Synchronization & Timestamping

### Sync Input

- **REQ-SYNC-01:** The device shall capture timestamps of rising and/or falling edges on any SYNC input line with ≤ 1 µs precision, without relying on software polling.
- **REQ-SYNC-02:** Each SYNC edge event shall be logged as an independent record containing port index, edge polarity, and timestamp.
- **REQ-SYNC-03:** The firmware shall associate each incoming UART packet with a SYNC event at capture time, based on a configured nominal packet rate for the port. Per-packet receive timestamping is optional and not required for compliance. If a packet is not received within the expected window, the firmware shall resynchronize the association sequence and log the gap.

### Trigger Output

- **REQ-TRIG-01:** Any SYNC output line shall generate trigger pulses at a configurable rate up to 2 kHz, with configurable pulse width.
- **REQ-TRIG-02:** Cycle-to-cycle jitter shall be ≤ 5 µs under all operating conditions, including concurrent SD writes and network I/O.
- **REQ-TRIG-03:** Each trigger pulse shall be timestamped with ≤ 1 µs precision and logged.

---

## 5. Timing & Clock

- **REQ-CLK-01:** The device shall maintain a single monotonically increasing microsecond-resolution clock. All logged timestamps shall be derived from this clock.
- **REQ-CLK-02:** The clock source shall follow this priority order:

  | Priority | Source | Condition |
  |----------|--------|-----------|
  | 1 | GNSS SYNC pulse | A port is designated as clock master and a consistent periodic SYNC input is present at configured period |
  | 2 | On-board RTC | RTC is present and holds a valid time |
  | 3 | Free-running counter | Fallback; epoch is arbitrary but consistent within a session |

- **REQ-CLK-03:** When locked to a GNSS SYNC signal, the device shall correct clock drift using slew-rate adjustment — the clock shall never jump backward. The maximum slew rate and expected SYNC period for the clock master shall be configurable.
- **REQ-CLK-04:** Clock drift relative to the GNSS SYNC signal shall be measured and logged at each pulse.
- **REQ-CLK-05:** Clock source transitions shall be logged. Each session log file shall record the clock source and epoch reference at session start.

---

## 6. SD Card Logging

- **REQ-SD-01:** The device shall log all data to SD card (SDHC/SDXC, 32 GB or larger). SD writes shall not stall sensor acquisition, timestamping, or UART reception. If SD consumption lags beyond the available in-memory retention window, the device shall log an error and continue best-effort SD logging without ending the active session.
- **REQ-SD-02:** The following shall be logged in a structured binary format: timestamped data packets per port, clock status records, and device error/status records.
- **REQ-SD-03:** The binary log format shall be self-describing and versioned, with a session header and a published specification sufficient to implement a reader without access to firmware source.
- **REQ-SD-04:** Device errors and status events shall be written to a separate human-readable log file on the SD card.
- **REQ-SD-05:** Log filenames shall incorporate a wall-clock timestamp or monotonic session counter to be unique across power cycles.
- **REQ-SD-06:** On unclean power loss, all records fully written before the event shall remain intact.

---

## 7. Network Interfaces & UDP Streaming

- **REQ-NET-01:** The device shall include Wi-Fi (802.11 b/g/n or better) and wired Ethernet (10/100 Mbps) interfaces. Each shall support DHCP and static IP, configurable via the SD card config file.
- **REQ-NET-02:** The device shall stream all logged record types over UDP on both interfaces simultaneously. Each interface may be configured with a different destination address and port. UDP payload size shall be configurable (default: 1400 bytes).
- **REQ-NET-03:** The UDP stream format shall be identical to the SD card binary log format, enabling a single host-side parser for both live and recorded data.
- **REQ-NET-04:** Wi-Fi and Ethernet shall operate independently with no automatic failover between them.

### Network Resilience

- **REQ-NET-05:** An interface shall be declared disconnected after 3 consecutive UDP send failures. On reconnection, streaming shall resume from the point of interruption.
- **REQ-NET-06:** Unstreamed data shall be retained in a shared in-memory retention window (central log ring) used by all consumers. No per-interface retransmit ring is required. If a UDP consumer lags behind this window, the dropped stream data shall be logged as INFO-level status. Data on the SD card shall never be deleted or overwritten.
- **REQ-NET-07:** SD card logging shall continue uninterrupted during any network outage.
- **REQ-NET-08:** The current streaming state per interface (streaming / backlogged / disconnected) shall be reported in the SD card status log and the UDP telemetry stream.

---

## 8. RTCM / GNSS Corrections

- **REQ-RTCM-01:** The device shall connect to an NTRIP caster over TCP and receive an RTCM 3.x correction stream. Caster hostname, port, mountpoint, and credentials shall be configurable.
- **REQ-RTCM-02:** Received RTCM data shall be forwarded to one or more designated sensor ports over UART within ≤ 100 ms of receipt. RTCM forwarding shall not interfere with concurrent UART reception or logging on any port.
- **REQ-RTCM-03:** The device shall periodically send NMEA GGA sentences to the NTRIP caster, sourced from a user-designated GNSS sensor port. The transmission interval shall be configurable.
- **REQ-RTCM-04:** If the NTRIP connection is lost, the device shall log the event, attempt reconnection at a configurable interval, and resume forwarding on reconnection.

---

## 9. Configuration

- **REQ-CFG-01:** All operational parameters shall be configurable via a structured file on the SD card, read at startup. This includes: per-port UART settings, parser assignments, and SYNC direction; trigger rate and pulse width; network addresses and IP mode; NTRIP parameters; GNSS clock master port; clock-master SYNC period; GGA source port and interval; and clock slew rate limit.
- **REQ-CFG-02:** Configuration shall be validated at startup. Errors shall be logged. The device shall halt or fall back to documented defaults per parameter.
- **REQ-CFG-03:** The device shall expose an HTTP API over its network interfaces allowing parser assignment per port and session start/stop. The API shall be read-only during an active session and shall enumerate available parser types.

---

## 10. Non-Functional Requirements

- **REQ-NF-01:** SYNC timestamp capture and trigger generation shall never be delayed by SD I/O, network I/O, or packet parsing.
- **REQ-NF-02:** The device shall automatically recover from a firmware lockup, resume logging to a new session file, and record the reset event on startup.
- **REQ-NF-03:** Core firmware modules shall be unit-testable in isolation.

---

## 11. Revision History

| Version | Date | Notes |
|---------|------|-------|
| 0.1 | 2026-03-09 | Initial draft |
| 0.2 | 2026-03-09 | Incorporated clarification responses Q1–Q20 |
| 0.3 | 2026-03-09 | Incorporated clarification responses Q-A1–Q-A5 |
| 0.4 | 2026-03-09 | Removed software interface definitions and hardware prescriptions |
| 0.5 | 2026-03-09 | Added GGA forwarding to NTRIP caster |
| 0.6 | 2026-03-09 | Consolidated and simplified; eliminated redundant requirements; restructured for clarity |
| 0.7 | 2026-03-09 | REQ-SYNC-03: association at capture time with nominal rate resync; REQ-SD-02: simplified to timestamped data packets; REQ-NET-06: in-memory buffer capped at 500 KB per interface |
| 0.8 | 2026-03-12 | Updated requirements per new PR comments: shared retention window for UDP, SD lag best-effort behavior, SYNC association as primary packet timing, configurable clock-master SYNC period |
