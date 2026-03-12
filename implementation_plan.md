# Embedded Software Implementation Plan

## Summary
The software plan is organized as four milestones. The first milestone delivers a field-usable minimum viable product for synchronized sensor capture. Later milestones add durable logging, correction data, network interfaces, configuration workflows, and recovery behavior. Every software capability in the design document is assigned to a milestone.

- **Milestone 1:** Core runtime, UART capture, free-running clock, and SYNC association.
- **Milestone 2:** Trigger output, binary/session logging, and watchdog recovery.
- **Milestone 3:** Network bring-up, UDP streaming, and NTRIP correction forwarding.
- **Milestone 4:** Configuration management and HTTP API for control and observability.

## Capability Coverage
This plan covers all software capabilities defined in the design document:
- Core runtime architecture, task separation, and shared log pipeline.
- UART capture, parser framework, parser assignment, and overrun reporting.
- Monotonic free-running device clock with all packets and events timestamped against that local time base.
- SYNC edge capture, packet-to-SYNC association, sync-gap handling, and trigger output.
- SD logging, published binary log format, human-readable status log, and power-loss recovery behavior.
- Ethernet and Wi-Fi interfaces, UDP streaming, backlog/disconnect reporting, and SD/network independence.
- NTRIP connection management, RTCM forwarding, GGA upload, and reconnect behavior.
- Startup configuration, staged config updates, validation, and parser/session control via HTTP API.
- Watchdog supervision, reset recovery, and reset-event logging.

## Milestone 1: Core Runtime, UART, Clock, and SYNC MVP

### Goal
Deliver a minimum viable firmware that can acquire sensor data from the UART ports, timestamp it against the shared free-running device clock, associate packets with SYNC events, and expose enough runtime status to validate capture quality.

### Deliverables
- Firmware startup path with the planned task split between real-time capture work and background services.
- Shared monotonic microsecond device clock available to all subsystems.
- Free-running-only clock behavior with no GNSS, RTC, or network-time discipline in firmware.
- Four-port UART support with configurable serial settings.
- Built-in NMEA and UBX parsers available for assignment per port.
- Parser framework that supports adding new parser modules without changing core behavior.
- UART packet capture with buffer overrun detection and error reporting.
- SYNC edge capture on configured input ports with logged polarity and timestamp.
- Packet-to-SYNC association per port, including gap detection and resynchronization reporting.
- Session/runtime status reporting over console or internal status output for port health, parser state, device-clock values, and error counters.

### User should be able to
- Connect up to four sensors and capture UART data concurrently.
- Configure each port’s serial settings and parser assignment before running a session.
- Confirm that packets and events are stamped against a monotonic device clock that does not jump or switch sources.
- Feed a SYNC signal into a port and verify that packets are associated with the correct sync sequence.
- Detect UART overruns and sync gaps during integration testing.

### Acceptance checkpoints
- All four UART ports can ingest data at representative and worst-case rates without unexpected data loss under nominal operation.
- NMEA and UBX packets are reconstructed correctly across buffer boundaries and malformed packets do not break continued capture.
- SYNC edges are logged with correct port and polarity, and packet association follows the configured nominal rate behavior.
- Device-clock timestamps increase monotonically through startup and runtime without source switches or backward jumps.
- UART overrun and sync-gap faults generate visible error/status records without crashing the capture path.

## Milestone 2: Trigger Output, Logging, and Recovery

### Goal
Extend the MVP into a durable field logger with trigger generation, structured session files, readable event logs, and recovery behavior for storage and firmware faults.

### Deliverables
- Trigger output on configured SYNC output ports with configurable rate and pulse width.
- Trigger timestamp logging so generated pulses share the same time base as SYNC and UART data.
- Central log pipeline that accepts records from capture, timing, trigger, and service subsystems.
- SD card session lifecycle with mount, start, stop, and clean shutdown behavior.
- Binary log files with a stable session header, versioned format, record structure, and completion marker.
- Human-readable status log file written alongside the binary session log.
- Unique file naming using a monotonic session counter.
- Power-loss resilience behavior so fully written records remain recoverable.
- SD degradation behavior when writes fail or lag behind available retention.
- Watchdog supervision for real-time and background tasks.
- Reset-recovery behavior that records watchdog or panic resets into the next session.

### User should be able to
- Run a session that captures UART data, sync events, network/service events, and trigger events into SD log files.
- Inspect a readable status log to understand major session events, warnings, and failures.
- Power-cycle or interrupt the device and still recover a valid partial session up to the last flushed records.
- Enable trigger output on selected ports while keeping timestamp comparability with incoming data.
- Recognize when SD storage has failed, fallen behind, or forced degraded behavior.
- See that firmware lockups lead to recovery behavior instead of silent failure.

### Acceptance checkpoints
- Trigger outputs meet configured rate and pulse-width behavior and continue operating correctly while UART capture and logging are active.
- Trigger event records, SYNC edge records, and data packet records align to the same shared clock domain.
- Starting and stopping a session creates the expected binary and text log artifacts with unique names.
- The binary log contains the published header and record structure needed for an external reader using device-clock timestamps only.
- Abrupt power removal leaves previously flushed records readable and incomplete sessions detectable.
- SD write failures and backlog conditions are reported and handled without blocking real-time acquisition.
- Watchdog-triggered restart behavior is observable and the subsequent session records the reset event.

## Milestone 3: Networking, UDP Streaming, and NTRIP

### Goal
Add live connectivity so the device can stream the same data it records, operate Ethernet and Wi-Fi independently, and deliver GNSS corrections to attached receivers.

### Deliverables
- Ethernet bring-up with independent interface state reporting.
- Wi-Fi bring-up through ESP-Hosted with independent interface state reporting.
- Shared-log UDP streaming over Ethernet and Wi-Fi with separate destinations and payload sizing.
- UDP stream output aligned with the published binary log format so one host-side parser can read both live and recorded data.
- Interface state handling for streaming, backlog, and disconnect conditions.
- Shared retention behavior that allows UDP lag without interfering with SD logging.
- Network status records for link changes, backlog events, and dropped stream data.
- NTRIP client connection to a configured caster.
- RTCM forwarding from NTRIP to designated UART target ports.
- Periodic GGA upload sourced from the configured GNSS port.
- NTRIP disconnect detection, reconnect attempts, and status reporting.

### User should be able to
- Stream live records over Ethernet, Wi-Fi, or both while an SD logging session continues.
- Use one downstream parser for both UDP telemetry and recorded binary logs.
- Tell whether each network interface is streaming, backlogged, or disconnected.
- Continue logging locally even when one or both network links fail.
- Configure a GNSS source for GGA and one or more RTCM target ports for correction forwarding.
- Observe NTRIP connect, disconnect, reconnection, and RTCM forwarding behavior during field tests.

### Acceptance checkpoints
- Ethernet and Wi-Fi can be brought up independently and used at the same time.
- UDP output over each interface contains the expected record types and remains compatible with the binary log reader.
- Network interruption causes the expected interface-state transitions and status records without interrupting SD logging.
- Stream lag beyond retention produces drop reporting rather than blocking or destabilizing the device.
- An end-to-end NTRIP test shows successful connection, RTCM reception, UART forwarding, GGA upload, and reconnect after loss.
- RTCM forwarding does not interfere with concurrent UART receive capture on active sensor ports.

## Milestone 4: Configuration and HTTP Control Plane

### Goal
Complete the operator workflow by making the device boot from validated configuration, support safe remote reconfiguration, and expose status and session control over HTTP.

### Deliverables
- Startup configuration loading from `/sd/config.json` covering ports, trigger, network, and NTRIP behavior.
- Strict config validation with clear invalid-config state when required fields or values are wrong.
- No-session-start behavior when the configuration is invalid.
- HTTP status endpoint for device state, uptime, current device-clock count, and per-port statistics.
- HTTP endpoint to enumerate available parser types.
- HTTP endpoint to read active and pending configuration.
- HTTP parser-assignment endpoint with rejection during active sessions.
- HTTP session start and stop endpoints with idempotent behavior.
- HTTP config update workflow with staged reboot, confirmation, and rollback on failed confirmation.
- API-visible status for degraded conditions such as SD failures and invalid config.

### User should be able to
- Prepare an SD card with a configuration file and have the device boot into the requested operating mode.
- Detect configuration errors clearly rather than seeing undefined fallback behavior.
- Query current device status and available parser types over the network.
- Start and stop sessions remotely.
- Change parser assignments before a session starts.
- Submit a new configuration remotely and recover automatically if the change breaks connectivity.

### Acceptance checkpoints
- Valid configurations boot successfully and invalid configurations are rejected with clear status exposure.
- The configuration file controls all major user-facing behaviors defined in the design.
- HTTP endpoints return the expected status, parser, config, and session-control responses.
- Parser updates are rejected during active sessions and accepted when idle.
- Session stop through HTTP produces the same clean-close behavior as a local stop path.
- A bad network configuration rolls back automatically when it is not confirmed within the allowed window.

## Cross-Milestone Validation
- Maintain a running requirements-to-milestone checklist so each design requirement is tied to a deliverable and acceptance checkpoint.
- Keep host-side validation artifacts for the published binary log format and UDP stream format as soon as the format is stable.
- Keep offboard time-correlation tooling separate from firmware and validate it against exported device-clock timestamps rather than in-device clock discipline.
- Run integrated system tests after each milestone with previously completed capabilities still enabled, not in isolation.
- Treat timing integrity, capture continuity, and fault reporting as regression gates for every subsequent milestone.

## Assumptions
- The first usable MVP is synchronized UART capture, not trigger generation or remote control.
- Trigger output is important, but it is sequenced after basic capture and timing so the first milestone stays focused on ingest and timestamp correctness.
- HTTP authentication remains out of scope for this implementation plan because the design document marks it as a future feature.
- OTA update support remains future work and is not required to satisfy the current software design.
