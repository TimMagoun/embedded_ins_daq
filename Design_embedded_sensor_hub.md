# Design Document
## Multi-Sensor Data Acquisition & Logging Hub — ESP32-P4-Nano

**Version:** 0.1
**Status:** Draft
**Last Updated:** 2026-03-10
**Target Platform:** Waveshare ESP32-P4-Nano
**SDK:** ESP-IDF v5.4+

---

## Table of Contents

1. [Platform Overview](#1-platform-overview)
2. [Hardware Architecture](#2-hardware-architecture)
3. [Software Architecture](#3-software-architecture)
4. [Module Designs](#4-module-designs)
5. [Binary Log Format](#5-binary-log-format)
6. [Configuration Schema](#6-configuration-schema)
7. [Requirements Traceability](#7-requirements-traceability)

---

## 1. Platform Overview

### ESP32-P4-Nano Key Specifications

| Resource | Detail |
|----------|--------|
| HP CPU | Dual-core RISC-V, 400 MHz, FPU + DSP |
| LP CPU | Single-core RISC-V, 40 MHz |
| Internal SRAM | 768 KB (L2MEM) + 32 KB (LP SRAM) |
| PSRAM | 32 MB (in-package, QSPI/OPI) |
| Flash | 16 MB NOR (QSPI) |
| HP UARTs | 5 (UART0–UART4), 128-byte FIFO, up to 5 Mbps |
| LP UART | 1 (LP_UART0), 16-byte FIFO |
| GP Timers | 4 (2 groups × 2), 54-bit counters |
| PCNT Units | 4 (2 channels each, 8 channels total) |
| MCPWM Groups | 2 (3 timers, 3 operators each) |
| GPIO | 55 on chip; 28 exposed on board headers |
| Ethernet | 100 Mbps via onboard IP101GRI PHY (RMII) |
| Wi-Fi | 802.11ax via onboard ESP32-C6-MINI-1 over SDIO (ESP-Hosted) |
| SD Card | microSD slot, SDIO 3.0 (4-bit) |
| USB | Type-C (programming/debug), Type-A (OTG 2.0 HS) |

### Platform Suitability

The ESP32-P4-Nano is well-suited to this project:

- **5 HP UARTs** provide exactly the peripherals needed for 4 sensor ports plus a debug console, each with 128-byte hardware FIFOs and DMA capability.
- **4 GP timers** with 54-bit counters provide the microsecond-resolution monotonic clock and trigger generation.
- **4 PCNT units** provide hardware edge capture on SYNC lines without CPU polling.
- **Dual-core 400 MHz RISC-V** with 32 MB PSRAM provides ample compute and memory for concurrent parsing, logging, and networking.
- **Onboard Ethernet PHY and Wi-Fi coprocessor** satisfy the dual-network requirement without external hardware.
- **SDIO 3.0 microSD slot** provides high-throughput SD card logging.

### Key Constraints

- **Wi-Fi is indirect:** All Wi-Fi traffic passes through the ESP32-C6 over SDIO via ESP-Hosted. This adds latency (~1–2 ms) but is transparent to the application via the standard `esp_wifi` API.
- **28 exposed GPIOs:** Pin planning is critical. The 4 sensor ports require 12 GPIOs (4× TX, RX, SYNC), plus UART0 is consumed by the USB-UART bridge for debug console.
- **No built-in RTC with battery backup:** The board has an RTC battery header, but the ESP32-P4's internal RTC is a low-accuracy RC oscillator. An external RTC (e.g., DS3231 over I2C) is recommended for REQ-CLK-02 priority 2.

---

## 2. Hardware Architecture

### 2.1 Pin Allocation

Pins consumed by onboard peripherals (fixed, cannot be reassigned):

| Function | Pins |
|----------|------|
| Ethernet RMII | GPIO28–31, 34–35, 49–52 |
| SD Card (SDIO) | GPIO39–44 |
| Audio Codec (I2S) | GPIO9–13, 53 |
| I2C Bus | GPIO7 (SDA), GPIO8 (SCL) |
| USB-UART (Console) | UART0 via USB-C bridge |

Available header GPIOs for sensor ports (from the 28 exposed):

| Sensor Port | UART Peripheral | TX Pin | RX Pin | SYNC Pin |
|-------------|----------------|--------|--------|----------|
| Port 0 | UART1 | GPIO14 | GPIO15 | GPIO16 |
| Port 1 | UART2 | GPIO17 | GPIO18 | GPIO19 |
| Port 2 | UART3 | GPIO20 | GPIO21 | GPIO22 |
| Port 3 | UART4 | GPIO23 | GPIO24 | GPIO25 |

> **Note:** Exact GPIO assignments must be validated against the Waveshare schematic and the ESP32-P4 GPIO matrix capabilities. The ESP32-P4's GPIO matrix allows any UART signal to be routed to any GPIO, so assignments are flexible. The above are initial assignments chosen from available header pins.

### 2.2 Physical Connector

Each sensor port is a 5-pin JST-GH or Molex Picoblade connector wired:

| Pin | Signal | Source |
|-----|--------|--------|
| 1 | VCC (3.3 V) | Regulated 3.3 V rail, 200 mA per port via polyfuse |
| 2 | GND | Common ground |
| 3 | RX (hub ← sensor) | Mapped to UART peripheral RX |
| 4 | TX (hub → sensor) | Mapped to UART peripheral TX |
| 5 | SYNC | GPIO, direction configured per port |

### 2.3 External RTC

An external DS3231 RTC module connects via the onboard I2C bus (GPIO7/GPIO8). This provides:
- ±2 ppm accuracy crystal oscillator
- Battery-backed timekeeping across power cycles
- Temperature-compensated output

### 2.4 Power Budget

| Consumer | Estimated Current (3.3 V) |
|----------|--------------------------|
| ESP32-P4 (active, dual-core, PSRAM) | ~250 mA |
| ESP32-C6 (Wi-Fi active) | ~120 mA |
| IP101GRI Ethernet PHY | ~50 mA |
| SD Card (write bursts) | ~100 mA |
| 4× Sensor ports (200 mA each max) | ~800 mA |
| DS3231 RTC | ~1 mA |
| **Total (worst case)** | **~1.3 A** |

USB-C supply must provide ≥ 2 A at 5 V. A dedicated 5 V / 3 A supply is recommended for field use.

---

## 3. Software Architecture

### 3.1 Frameworks & Libraries

| Component | Library / Module | Source |
|-----------|-----------------|--------|
| RTOS | FreeRTOS (SMP) | Built into ESP-IDF |
| UART Driver | `driver/uart.h` | ESP-IDF |
| GP Timer | `driver/gptimer.h` | ESP-IDF |
| PCNT | `driver/pulse_cnt.h` | ESP-IDF |
| MCPWM | `driver/mcpwm_timer.h`, `mcpwm_oper.h`, `mcpwm_gen.h` | ESP-IDF |
| GPIO | `driver/gpio.h` | ESP-IDF |
| SD/MMC | `driver/sdmmc_host.h`, `esp_vfs_fat.h` | ESP-IDF |
| Ethernet | `esp_eth.h`, PHY: `esp_eth_phy_new_ip101()` | ESP-IDF |
| Wi-Fi | `esp_wifi.h` via ESP-Hosted (SDIO to ESP32-C6) | ESP-IDF + esp_hosted component |
| TCP/IP | `esp_netif.h`, lwIP | ESP-IDF |
| HTTP Server | `esp_http_server.h` | ESP-IDF |
| NTRIP Client | Custom (TCP socket over lwIP) | Project-specific |
| JSON Config | `cJSON` | Bundled with ESP-IDF |
| Watchdog | `esp_task_wdt.h` | ESP-IDF |
| I2C (RTC) | `driver/i2c_master.h` | ESP-IDF |
| Logging | `esp_log.h` | ESP-IDF |

### 3.2 Task Architecture

The firmware runs on FreeRTOS SMP across the two HP cores. Tasks are pinned to cores to isolate real-time operations from I/O-heavy work.

```
┌─────────────────────────────────────────────────────────────────┐
│                        CORE 0 (Real-Time)                       │
│                                                                 │
│  ┌──────────────┐  ┌──────────────┐  ┌───────────────────────┐ │
│  │  Clock Mgr   │  │ Trigger Gen  │  │   SYNC Edge Capture   │ │
│  │  (highest)   │  │   (high)     │  │   (ISR + deferred)    │ │
│  └──────────────┘  └──────────────┘  └───────────────────────┘ │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │              UART RX Handler (4 ports)                   │   │
│  │         ISR → DMA → Ring Buffer → Parse Task            │   │
│  └──────────────────────────────────────────────────────────┘   │
│                                                                 │
├─────────────────────────────────────────────────────────────────┤
│                        CORE 1 (I/O + App)                       │
│                                                                 │
│  ┌──────────────┐  ┌──────────────┐  ┌───────────────────────┐ │
│  │   SD Logger   │  │ UDP Streamer │  │   NTRIP Client       │ │
│  │   (medium)   │  │   (medium)   │  │   (low)              │ │
│  └──────────────┘  └──────────────┘  └───────────────────────┘ │
│  ┌──────────────┐  ┌──────────────┐                            │
│  │  HTTP API    │  │  Config Mgr  │                            │
│  │   (low)      │  │  (startup)   │                            │
│  └──────────────┘  └──────────────┘                            │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 3.3 Task Table

| Task | Core | Priority | Stack | Description |
|------|------|----------|-------|-------------|
| `clock_mgr` | 0 | 24 (highest) | 4 KB | Manages monotonic clock, GNSS discipline, drift tracking |
| `trigger_gen` | 0 | 22 | 2 KB | Configures MCPWM for trigger output, timestamps pulses |
| `sync_capture` | 0 | 22 | 4 KB | Deferred processing of SYNC edge ISR events |
| `uart_rx[0-3]` | 0 | 20 | 4 KB each | Reads DMA ring buffers, invokes parser, timestamps packets |
| `sd_logger` | 1 | 12 | 8 KB | Drains log queue to SD card in large sequential writes |
| `udp_streamer` | 1 | 12 | 8 KB | Drains log queue to UDP sockets (Wi-Fi + Ethernet) |
| `ntrip_client` | 1 | 10 | 8 KB | TCP connection to NTRIP caster, RTCM forwarding, GGA upload |
| `http_api` | 1 | 8 | 6 KB | REST API for parser assignment and session control |
| `config_mgr` | 1 | 6 | 4 KB | Reads and validates SD card config at startup |

All task stacks are allocated in PSRAM to conserve internal SRAM for DMA buffers and critical data structures.

### 3.4 Inter-Task Communication

```
                    ┌─────────────────────────┐
                    │    Central Log Queue     │
                    │  (FreeRTOS Queue in      │
                    │   PSRAM, ~2 MB)          │
                    └─────┬──────────┬─────────┘
                          │          │
              ┌───────────▼──┐  ┌───▼───────────┐
              │  SD Logger   │  │  UDP Streamer  │
              └──────────────┘  └────────────────┘

  Producers:
    UART RX tasks  ──→  Log Queue  (timestamped data records)
    SYNC Capture   ──→  Log Queue  (edge event records)
    Trigger Gen    ──→  Log Queue  (trigger event records)
    Clock Mgr      ──→  Log Queue  (clock status records)
    NTRIP Client   ──→  Log Queue  (RTCM status records)
```

The log queue uses a **multi-reader pattern**: each consumer (SD Logger, UDP Streamer) maintains its own read pointer into a shared ring buffer in PSRAM. This avoids duplicating data and allows each consumer to proceed at its own pace.

**RTCM data path** (separate from log queue):
```
  NTRIP Client ──TCP──→ RTCM Buffer ──UART TX──→ Designated sensor port(s)
```

RTCM data is written directly to the target UART TX FIFOs via `uart_write_bytes()`, which is non-blocking when the hardware FIFO has space. A dedicated 4 KB ring buffer per target port absorbs bursts.

### 3.5 Memory Map

| Region | Size | Usage |
|--------|------|-------|
| Internal SRAM (768 KB) | ~200 KB | UART DMA buffers (4× 32 KB), ISR stacks, critical data |
| | ~100 KB | FreeRTOS kernel, heap metadata, driver state |
| | ~468 KB | Reserved / headroom |
| PSRAM (32 MB) | ~2 MB | Central log ring buffer |
| | ~1 MB | UDP retransmit buffers (500 KB × 2 interfaces) |
| | ~512 KB | Task stacks |
| | ~256 KB | HTTP server buffers, cJSON workspace |
| | ~28 MB | Available headroom |
| Flash (16 MB) | ~2 MB | Firmware image |
| | ~14 MB | OTA partition, NVS, etc. |

---

## 4. Module Designs

### 4.1 UART Subsystem

**Satisfies:** REQ-UART-01 through REQ-UART-05

#### Initialization

Each of the 4 sensor ports maps to a hardware UART (UART1–UART4). UART0 is reserved for the debug console over USB.

```c
uart_config_t uart_cfg = {
    .baud_rate = port_config->baud_rate,   // 9600–921600
    .data_bits = port_config->data_bits,   // 7 or 8
    .parity    = port_config->parity,      // NONE, EVEN, ODD
    .stop_bits = port_config->stop_bits,   // 1 or 2
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
};
uart_driver_install(uart_num, rx_buf_size, tx_buf_size, event_queue_size, &queue, 0);
uart_param_config(uart_num, &uart_cfg);
uart_set_pin(uart_num, tx_pin, rx_pin, -1, -1);
```

#### RX Buffer Sizing (REQ-UART-02)

The requirement is 500 ms of data at the configured baud rate. At 921600 bps with 8N1 (10 bits/byte): 921600 / 10 × 0.5 = 46,080 bytes. Each port's DMA ring buffer is set to **48 KB** (rounded up), allocated in internal SRAM for DMA access.

Total UART DMA memory: 4 × 48 KB = 192 KB of internal SRAM.

At lower baud rates, the same buffer provides even more headroom (e.g., at 115200 bps, the buffer holds ~4 seconds).

#### Buffer Overrun Detection (REQ-UART-03)

The ESP-IDF UART driver reports `UART_BUFFER_FULL` and `UART_FIFO_OVF` events through the event queue. The `uart_rx` task monitors these events and, on detection:
1. Logs an overrun record to the central log queue (record type `REC_ERROR`, subtype `ERR_UART_OVERRUN`, with port index and timestamp).
2. Flushes the UART RX FIFO to resynchronize.
3. Increments a per-port overrun counter reported in telemetry.

#### Parser Framework (REQ-UART-04, REQ-UART-05)

Parsers are implemented as a **vtable-based plugin interface**:

```c
typedef struct {
    const char *name;                          // e.g., "nmea", "ubx"
    void *(*create)(const parser_config_t *cfg);
    parse_result_t (*feed)(void *ctx, const uint8_t *data, size_t len);
    void (*destroy)(void *ctx);
} parser_type_t;
```

- `feed()` is called incrementally as bytes arrive. It returns `PARSE_INCOMPLETE`, `PARSE_COMPLETE` (with a pointer to the parsed packet), or `PARSE_ERROR`.
- Parsers are registered at compile time in a `parser_registry[]` array. Adding a new parser requires only: (1) implement the three functions, (2) add an entry to the registry.
- Parser assignment per port is stored in the runtime config and can be changed via the HTTP API before a session starts.

**Built-in parsers:**
- **NMEA 0183:** Scans for `$` start, accumulates until `\r\n`, validates checksum. Emits complete sentences as opaque byte blobs (no field parsing on-device).
- **UBX Binary:** State machine scanning for `0xB5 0x62` sync bytes, reads class/ID/length, accumulates payload, validates Fletcher-16 checksum.

Both parsers annotate each complete packet with:
- Receive timestamp (from the monotonic clock at the moment the first byte of the packet entered the DMA buffer, interpolated from the byte position and baud rate).
- Associated SYNC event index (see §4.3).

### 4.2 Clock Subsystem

**Satisfies:** REQ-CLK-01 through REQ-CLK-05

#### Monotonic Clock (REQ-CLK-01)

A dedicated GP Timer (Timer Group 0, Timer 0) runs as a free-running 64-bit microsecond counter. It is configured at startup:

```c
gptimer_config_t timer_cfg = {
    .clk_src = GPTIMER_CLK_SRC_XTAL,   // 40 MHz crystal — stable, independent of CPU freq
    .direction = GPTIMER_COUNT_UP,
    .resolution_hz = 1000000,           // 1 µs resolution
};
```

This timer is **never reset** during a session. All timestamps across all subsystems call `gptimer_get_raw_count()` to read this counter.

The 54-bit counter at 1 MHz overflows after ~571 years — effectively infinite.

#### Clock Discipline (REQ-CLK-02, REQ-CLK-03, REQ-CLK-04)

The `clock_mgr` task implements a clock discipline loop:

1. **GNSS SYNC lock (Priority 1):** When a port is designated as clock master, its SYNC input edges are captured by the PCNT/ISR subsystem (§4.3). The `clock_mgr` receives these edge timestamps and compares successive intervals against the expected 1 PPS period (1,000,000 µs).
   - Drift = measured_interval − 1,000,000 µs.
   - The timer's prescaler is not adjusted (no hardware slew). Instead, a **software offset accumulator** applies a fractional correction to each timestamp read. This ensures the clock never jumps backward (REQ-CLK-03).
   - The correction is applied as a slew: a configurable maximum rate (default: ±500 ns/s) limits how fast the software offset changes.
   - Drift measurements are logged as `REC_CLOCK_STATUS` records (REQ-CLK-04).

2. **RTC fallback (Priority 2):** At startup, if no GNSS SYNC is present, the DS3231 RTC is read over I2C. The monotonic counter epoch is set to the RTC's UTC time (converted to microseconds since midnight). The RTC is read once at boot; it does not discipline the running counter.

3. **Free-running fallback (Priority 3):** If no RTC is present or its time is invalid (oscillator-stop flag set), the counter starts from zero. The epoch is arbitrary but consistent within the session.

#### Clock Source Transitions (REQ-CLK-05)

Transitions between clock sources (e.g., free-running → GNSS locked) are logged as `REC_CLOCK_STATUS` records. The session header (§5) records the initial clock source and epoch reference.

### 4.3 Synchronization Subsystem

**Satisfies:** REQ-SYNC-01 through REQ-SYNC-03

#### SYNC Edge Capture (REQ-SYNC-01, REQ-SYNC-02)

Each port's SYNC pin is configured as a GPIO interrupt source when operating as an input:

```c
gpio_config_t sync_cfg = {
    .pin_bit_mask = (1ULL << sync_pin),
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_ENABLE,
    .intr_type = GPIO_INTR_ANYEDGE,
};
gpio_isr_handler_add(sync_pin, sync_isr, (void *)port_index);
```

The ISR reads the GP Timer counter immediately on entry, achieving ≤ 1 µs precision (ISR entry latency on the ESP32-P4 at 400 MHz is ~200–500 ns). The ISR pushes a lightweight event struct to a FreeRTOS queue:

```c
typedef struct {
    uint64_t timestamp_us;
    uint8_t  port_index;
    uint8_t  edge;          // 0 = falling, 1 = rising
} sync_event_t;
```

The `sync_capture` task dequeues these events and writes `REC_SYNC_EDGE` records to the central log queue.

#### Packet–SYNC Association (REQ-SYNC-03)

Each port maintains a SYNC-association state machine:

- The port's configured nominal packet rate (e.g., 10 Hz) defines an expected window between SYNC edges and packet arrivals.
- A running sequence counter is incremented on each SYNC edge.
- When a UART packet completes parsing, the parser annotates it with the current SYNC sequence number for that port.
- If the time gap between the expected and actual packet arrival exceeds 1.5× the nominal period, the state machine enters **resync mode**: it waits for the next SYNC edge that is followed by a packet within the expected window, then re-locks. A `REC_SYNC_GAP` record is logged.

### 4.4 Trigger Output

**Satisfies:** REQ-TRIG-01 through REQ-TRIG-03

Trigger output uses the **MCPWM** peripheral rather than software timers, providing hardware-guaranteed timing with sub-microsecond jitter:

```c
mcpwm_timer_config_t timer_cfg = {
    .group_id = 0,
    .clk_src = MCPWM_TIMER_CLK_SRC_XTAL,  // 40 MHz crystal
    .resolution_hz = 1000000,               // 1 µs resolution
    .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    .period_ticks = 1000000 / trigger_rate_hz,
};
```

- The MCPWM timer generates the repetitive waveform entirely in hardware.
- Pulse width is configured via the MCPWM comparator: the generator drives the SYNC output pin high on timer zero, low on compare match.
- At 2 kHz, the period is 500 µs. The XTAL-derived 1 µs resolution provides 500 discrete steps within each period — more than sufficient.
- Cycle-to-cycle jitter is determined by the XTAL stability (~±20 ppm = ±0.01 µs at 500 µs period), well within the ≤ 5 µs requirement (REQ-TRIG-02).

**Trigger timestamping (REQ-TRIG-03):** The MCPWM peripheral fires an interrupt on each timer overflow (period start). The ISR reads the GP Timer counter and pushes a `REC_TRIGGER` event to the log queue. ISR latency (~500 ns) is negligible relative to the 1 µs precision requirement.

### 4.5 SD Card Logger

**Satisfies:** REQ-SD-01 through REQ-SD-06

#### Mount and Filesystem

The SD card is mounted via the SDMMC host in 4-bit SDIO mode using the ESP-IDF VFS FAT layer:

```c
sdmmc_host_t host = SDMMC_HOST_DEFAULT();
host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;  // 40 MHz
sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
slot.width = 4;
esp_vfs_fat_sdmmc_mount("/sd", &host, &slot, &mount_cfg, &card);
```

FAT32 is used for broad compatibility. Files are pre-allocated to avoid fragmentation-induced write stalls.

#### Write Strategy (REQ-SD-01)

The `sd_logger` task is the sole writer to the SD card. It operates in a **batch-write loop**:

1. Wait on the central log ring buffer until at least 16 KB of data is available, or 100 ms has elapsed (whichever comes first).
2. Copy the available records into a local write buffer (up to 64 KB).
3. Call `fwrite()` with the entire buffer in one operation.
4. Call `fsync()` every 1 second to flush the FAT metadata.

This pattern ensures:
- SD writes are large and sequential, maximizing throughput (typical: 5–10 MB/s on a Class 10 card in 4-bit mode).
- The 100 ms maximum latency ensures data reaches the card promptly even at low data rates.
- Sensor acquisition, timestamping, and UART reception run on Core 0 and are never blocked by SD I/O on Core 1.

#### File Naming (REQ-SD-05)

```
/sd/sessions/20260310_143025_001.bin    # Binary log (wall-clock timestamp + session counter)
/sd/sessions/20260310_143025_001.log    # Human-readable status log
```

If no wall-clock time is available (free-running clock), filenames use a monotonic boot counter stored in NVS:

```
/sd/sessions/session_00042.bin
/sd/sessions/session_00042.log
```

#### Power-Loss Resilience (REQ-SD-06)

- The binary log file is pre-allocated (e.g., 256 MB) at session start, so the FAT directory entry and cluster chain are stable.
- Data is written sequentially; `fsync()` is called every 1 second.
- Each record is self-delimiting (length-prefixed with a type tag), so a reader can scan forward past any partial record at the end of a truncated file.
- The session header (written first) includes a `session_complete` flag, initially `false`. It is overwritten to `true` only on clean session stop. A reader can detect unclean shutdowns.

#### Human-Readable Status Log (REQ-SD-04)

A separate `.log` file in the same session directory contains timestamped plain-text lines:

```
[000000.000000] SESSION START clock_source=gnss_sync epoch=2026-03-10T14:30:25.000000Z
[000001.234567] NTRIP connected to rtk2go.com:2101/MOUNTPT
[000005.678901] UART2 overrun: 128 bytes lost
[000312.456789] CLOCK drift=+3us source=gnss_sync
```

### 4.6 Network Subsystem

**Satisfies:** REQ-NET-01 through REQ-NET-08

#### Interface Initialization (REQ-NET-01)

**Ethernet:**
```c
eth_mac_config_t mac_cfg = ETH_MAC_DEFAULT_CONFIG();
eth_phy_config_t phy_cfg = ETH_PHY_DEFAULT_CONFIG();
phy_cfg.phy_addr = 1;
phy_cfg.reset_gpio_num = 51;
esp_eth_mac_t *mac = esp_eth_mac_new_esp32(/* RMII config */);
esp_eth_phy_t *phy = esp_eth_phy_new_ip101(&phy_cfg);
```

**Wi-Fi (via ESP-Hosted):**
The ESP32-C6 companion chip runs ESP-Hosted slave firmware. The ESP32-P4 communicates over SDIO and uses the standard `esp_wifi` API transparently. The ESP-Hosted component is added to the project's `idf_component.yml`.

Both interfaces support DHCP (default) and static IP, configured via the SD card config file. Each interface gets its own `esp_netif` instance.

#### UDP Streaming (REQ-NET-02, REQ-NET-03)

The `udp_streamer` task maintains two independent UDP sockets (one per interface). Each socket has:
- A configurable destination IP and port.
- A configurable MTU / payload size (default: 1400 bytes).
- An independent read pointer into the central log ring buffer.

Records are batched into UDP datagrams up to the configured payload size. The datagram format is identical to the binary log format (§5), prefixed with a sequence number for gap detection on the receiver side.

#### Resilience (REQ-NET-05 through REQ-NET-08)

Each interface tracks its state as one of: `STREAMING`, `BACKLOGGED`, `DISCONNECTED`.

- **Disconnect detection (REQ-NET-05):** `sendto()` failure increments a failure counter. After 3 consecutive failures, the interface transitions to `DISCONNECTED`. On reconnection (next successful `sendto()`), the interface transitions to `BACKLOGGED` and begins streaming from its retained buffer position.
- **Retransmit buffer (REQ-NET-06):** Each interface maintains a 500 KB circular buffer in PSRAM. When `DISCONNECTED` or `BACKLOGGED`, incoming records are written to this buffer. When it fills, the oldest data is overwritten and a `REC_NET_GAP` record is injected. The SD card log is never affected.
- **SD independence (REQ-NET-07):** The SD logger has its own independent read pointer and runs on a separate task. Network state has no effect on SD writes.
- **State reporting (REQ-NET-08):** Interface state changes are logged as `REC_NET_STATUS` records in both the SD log and the UDP stream.

### 4.7 NTRIP Client

**Satisfies:** REQ-RTCM-01 through REQ-RTCM-04

#### Connection

The `ntrip_client` task opens a TCP socket to the configured NTRIP caster and sends an HTTP-style request:

```
GET /MOUNTPT HTTP/1.1\r\n
Host: rtk2go.com\r\n
Ntrip-Version: Ntrip/2.0\r\n
User-Agent: NTRIP EmbeddedHub/1.0\r\n
Authorization: Basic <base64(user:pass)>\r\n
\r\n
```

On success (ICY 200 OK), the task enters a receive loop.

#### RTCM Forwarding (REQ-RTCM-02)

Incoming TCP data is buffered in a 4 KB ring buffer. As soon as data is available, it is forwarded to the designated UART port(s) via `uart_write_bytes()`. The UART TX FIFO (128 bytes) and driver's internal TX ring buffer absorb bursts.

Latency budget: TCP receive (~10–50 ms network) + memcpy to UART TX buffer (~10 µs) + UART transmission (at 115200 bps, 1 KB takes ~90 ms). Total well within 100 ms for typical RTCM message sizes (< 500 bytes).

RTCM forwarding uses `uart_write_bytes()` which is non-blocking when buffer space is available, so it does not interfere with concurrent RX on any port (RX and TX are independent hardware paths in the UART peripheral).

#### GGA Upload (REQ-RTCM-03)

The `ntrip_client` task periodically reads the latest NMEA GGA sentence from a designated GNSS sensor port. The NMEA parser on that port stores the most recent GGA sentence in a shared buffer (protected by a mutex). The NTRIP client reads this buffer at the configured interval (default: 10 seconds) and sends it to the caster over the existing TCP connection.

#### Reconnection (REQ-RTCM-04)

On TCP disconnection, the task:
1. Logs a `REC_NTRIP_STATUS` event (disconnected).
2. Waits for the configured retry interval (default: 5 seconds).
3. Attempts reconnection.
4. On success, logs a reconnection event and resumes forwarding.

### 4.8 HTTP API

**Satisfies:** REQ-CFG-03

The HTTP server runs on Core 1 using the ESP-IDF `httpd` component, listening on port 80 on both network interfaces.

#### Endpoints

| Method | Path | Description | Session Constraint |
|--------|------|-------------|-------------------|
| GET | `/api/status` | Device status, clock source, uptime, per-port stats | Always available |
| GET | `/api/parsers` | List available parser types | Always available |
| GET | `/api/config` | Current configuration | Always available |
| PUT | `/api/ports/{id}/parser` | Assign parser to port | Read-only during session |
| POST | `/api/session/start` | Start a logging session | No-op if already active |
| POST | `/api/session/stop` | Stop the active session | No-op if not active |

Request and response bodies are JSON, parsed/generated with `cJSON`.

During an active session, `PUT` endpoints return HTTP 409 Conflict.

### 4.9 Configuration

**Satisfies:** REQ-CFG-01, REQ-CFG-02

#### Config File

The configuration file is `/sd/config.json`, read at startup. See §6 for the full schema.

#### Validation (REQ-CFG-02)

At startup, `config_mgr`:
1. Reads `/sd/config.json`.
2. Parses with `cJSON`.
3. Validates each field against allowed ranges.
4. Logs errors for invalid fields to both the console and the status log file.
5. For each invalid parameter, falls back to a documented default value and continues. The device does not halt unless the config file is unparsable (malformed JSON).

### 4.10 Watchdog & Recovery

**Satisfies:** REQ-NF-02

- The ESP-IDF Task Watchdog Timer (`esp_task_wdt`) monitors all tasks. If any task fails to reset the watchdog within 5 seconds, the system reboots.
- On boot, the firmware checks the reset reason via `esp_reset_reason()`. If it was a watchdog or panic reset, a `REC_ERROR` record with subtype `ERR_WATCHDOG_RESET` is written as the first record of the new session.
- Each session starts with a new log file, so data from the previous session remains intact.

---

## 5. Binary Log Format

**Satisfies:** REQ-SD-02, REQ-SD-03, REQ-NET-03

### 5.1 Session Header

Written once at the start of each log file. The format is self-describing and versioned.

```
Offset  Size  Field
0       4     Magic: 0x44415148 ("DAQH")
4       2     Format version (uint16, currently 1)
6       1     Clock source at session start (0=free-running, 1=RTC, 2=GNSS)
7       1     Reserved (0x00)
8       8     Epoch reference (uint64, microseconds since Unix epoch, or 0 if free-running)
16      4     Number of ports (uint32)
20      N     Per-port descriptors (see below)
...     1     session_complete flag (0x00 = incomplete, 0x01 = complete)
```

Per-port descriptor (16 bytes each):
```
Offset  Size  Field
0       1     Port index
1       4     Baud rate (uint32)
5       1     Data bits
6       1     Parity (0=none, 1=even, 2=odd)
7       1     Stop bits
8       8     Parser name (null-terminated, max 8 chars)
```

### 5.2 Record Format

All records after the header follow a uniform TLV (type-length-value) structure:

```
Offset  Size  Field
0       1     Record type (uint8)
1       2     Payload length (uint16, little-endian, max 65535)
3       8     Timestamp (uint64, microseconds, little-endian)
11      N     Payload (type-specific)
11+N    1     Checksum (XOR of bytes 0 through 10+N)
```

### 5.3 Record Types

| Type ID | Name | Payload |
|---------|------|---------|
| 0x01 | `REC_DATA` | port_index(1) + sync_seq(4) + raw_packet_bytes(N) |
| 0x02 | `REC_SYNC_EDGE` | port_index(1) + edge(1: 0=fall,1=rise) |
| 0x03 | `REC_TRIGGER` | port_index(1) + pulse_width_us(2) |
| 0x04 | `REC_CLOCK_STATUS` | source(1) + drift_ns(4, signed) + slew_rate_ppb(4, signed) |
| 0x05 | `REC_NET_STATUS` | interface(1: 0=eth,1=wifi) + state(1: 0=streaming,1=backlogged,2=disconnected) |
| 0x06 | `REC_NTRIP_STATUS` | state(1: 0=disconnected,1=connected) + bytes_forwarded(4) |
| 0x07 | `REC_SYNC_GAP` | port_index(1) + missed_count(2) |
| 0x08 | `REC_NET_GAP` | interface(1) + bytes_lost(4) |
| 0xFE | `REC_ERROR` | subtype(1) + port_index(1) + detail_string(N, null-terminated) |
| 0xFF | `REC_STATUS` | message_string(N, null-terminated) |

The format is identical for SD card and UDP streaming (REQ-NET-03). UDP datagrams contain one or more complete records, prefixed with a 4-byte datagram sequence number.

---

## 6. Configuration Schema

File: `/sd/config.json`

```json
{
  "ports": [
    {
      "index": 0,
      "uart": {
        "baud_rate": 115200,
        "data_bits": 8,
        "parity": "none",
        "stop_bits": 1
      },
      "parser": "ubx",
      "sync_direction": "input",
      "nominal_rate_hz": 10,
      "clock_master": true,
      "gga_source": false,
      "rtcm_target": true
    },
    {
      "index": 1,
      "uart": {
        "baud_rate": 460800,
        "data_bits": 8,
        "parity": "none",
        "stop_bits": 1
      },
      "parser": "nmea",
      "sync_direction": "input",
      "nominal_rate_hz": 5,
      "clock_master": false,
      "gga_source": true,
      "rtcm_target": false
    }
  ],
  "trigger": {
    "port_index": 2,
    "rate_hz": 100,
    "pulse_width_us": 10
  },
  "network": {
    "ethernet": {
      "enabled": true,
      "mode": "dhcp",
      "static_ip": null,
      "udp_dest_ip": "192.168.1.100",
      "udp_dest_port": 5000,
      "udp_payload_size": 1400
    },
    "wifi": {
      "enabled": true,
      "ssid": "FieldNetwork",
      "password": "secretpass",
      "mode": "dhcp",
      "static_ip": null,
      "udp_dest_ip": "192.168.2.100",
      "udp_dest_port": 5001,
      "udp_payload_size": 1400
    }
  },
  "ntrip": {
    "enabled": true,
    "host": "rtk2go.com",
    "port": 2101,
    "mountpoint": "MOUNTPT",
    "username": "user",
    "password": "pass",
    "gga_interval_s": 10,
    "reconnect_interval_s": 5
  },
  "clock": {
    "max_slew_rate_ppb": 500
  }
}
```

**Defaults** (applied when a field is missing or invalid):

| Parameter | Default |
|-----------|---------|
| baud_rate | 115200 |
| data_bits | 8 |
| parity | none |
| stop_bits | 1 |
| parser | none (raw passthrough) |
| sync_direction | input |
| nominal_rate_hz | 1 |
| trigger rate_hz | 0 (disabled) |
| trigger pulse_width_us | 10 |
| network mode | dhcp |
| udp_payload_size | 1400 |
| ntrip enabled | false |
| gga_interval_s | 10 |
| reconnect_interval_s | 5 |
| max_slew_rate_ppb | 500 |

---

## 7. Requirements Traceability

| Requirement | Design Section | Implementation Mechanism |
|-------------|---------------|--------------------------|
| REQ-UART-01 | §4.1 | ESP-IDF UART driver, per-port `uart_param_config()` |
| REQ-UART-02 | §4.1 | 48 KB DMA ring buffer per port in internal SRAM |
| REQ-UART-03 | §4.1 | UART event queue monitoring for `UART_BUFFER_FULL` / `UART_FIFO_OVF` |
| REQ-UART-04 | §4.1 | Parser vtable interface with `parser_registry[]` |
| REQ-UART-05 | §4.1, §4.8 | Runtime parser assignment via config file or HTTP API |
| REQ-SYNC-01 | §4.3 | GPIO ISR reads GP Timer on edge; ≤ 500 ns ISR latency |
| REQ-SYNC-02 | §4.3 | `REC_SYNC_EDGE` records with port, polarity, timestamp |
| REQ-SYNC-03 | §4.3 | Nominal-rate association state machine with resync and gap logging |
| REQ-TRIG-01 | §4.4 | MCPWM hardware timer, configurable rate up to 2 kHz |
| REQ-TRIG-02 | §4.4 | MCPWM driven by XTAL clock; jitter < 1 µs |
| REQ-TRIG-03 | §4.4 | MCPWM overflow ISR timestamps each pulse via GP Timer |
| REQ-CLK-01 | §4.2 | GP Timer Group 0, Timer 0: free-running 1 µs counter |
| REQ-CLK-02 | §4.2 | Three-priority clock source selection at startup |
| REQ-CLK-03 | §4.2 | Software slew-rate offset accumulator; clock never jumps backward |
| REQ-CLK-04 | §4.2 | Drift measured at each GNSS PPS edge, logged as `REC_CLOCK_STATUS` |
| REQ-CLK-05 | §4.2 | Source transitions logged; session header records initial source |
| REQ-SD-01 | §4.5 | Batch writes on Core 1; sensor acquisition on Core 0 |
| REQ-SD-02 | §5 | Binary TLV format: data packets, clock records, error records |
| REQ-SD-03 | §5 | Self-describing header with version; published record type table |
| REQ-SD-04 | §4.5 | Separate `.log` file with timestamped plain-text lines |
| REQ-SD-05 | §4.5 | Wall-clock timestamp or NVS boot counter in filenames |
| REQ-SD-06 | §4.5 | Pre-allocated file, periodic `fsync()`, self-delimiting records |
| REQ-NET-01 | §4.6 | IP101GRI Ethernet + ESP32-C6 Wi-Fi; DHCP/static via config |
| REQ-NET-02 | §4.6 | Dual independent UDP sockets, configurable destination and payload size |
| REQ-NET-03 | §5 | Same binary TLV format for SD and UDP |
| REQ-NET-04 | §4.6 | Independent `esp_netif` instances, no failover logic |
| REQ-NET-05 | §4.6 | 3-failure disconnect detection, resume on next success |
| REQ-NET-06 | §4.6 | 500 KB PSRAM ring buffer per interface; oldest-overwrite with gap logging |
| REQ-NET-07 | §4.6 | SD logger has independent read pointer, unaffected by network state |
| REQ-NET-08 | §4.6 | `REC_NET_STATUS` records logged to SD and UDP |
| REQ-RTCM-01 | §4.7 | Custom TCP NTRIP client with configurable caster parameters |
| REQ-RTCM-02 | §4.7 | Direct `uart_write_bytes()` forwarding; < 100 ms latency |
| REQ-RTCM-03 | §4.7 | Periodic GGA read from designated parser, sent over TCP |
| REQ-RTCM-04 | §4.7 | Reconnect loop with configurable interval and event logging |
| REQ-CFG-01 | §4.9, §6 | `/sd/config.json` with all operational parameters |
| REQ-CFG-02 | §4.9 | JSON validation at startup; per-field defaults; errors logged |
| REQ-CFG-03 | §4.8 | HTTP API on port 80; read-only during active session |
| REQ-NF-01 | §3.2 | Core 0 dedicated to real-time tasks; Core 1 handles I/O |
| REQ-NF-02 | §4.10 | Task WDT, reset-reason check, new session file on recovery |
| REQ-NF-03 | §4.1, §4.2 | Vtable interfaces and pure-function modules enable host-side unit testing |

---

## Revision History

| Version | Date | Notes |
|---------|------|-------|
| 0.1 | 2026-03-10 | Initial design targeting ESP32-P4-Nano |
