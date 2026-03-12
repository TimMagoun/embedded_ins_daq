# Design Document
## Multi-Sensor Data Acquisition & Logging Hub — ESP32-P4-Nano

**Version:** 0.5
**Status:** Draft
**Last Updated:** 2026-03-12
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
| HP CPU | Dual-core RISC-V, 360 MHz (max 400 MHz), FPU + DSP |
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
- **GPIO interrupt latency is sub-microsecond** on the HP cores, sufficient for the ≤1 µs SYNC timestamp requirement without PCNT complexity.
- **Dual-core 360 MHz RISC-V** with 32 MB PSRAM provides ample compute and memory for concurrent parsing, logging, and networking.
- **Onboard Ethernet PHY and Wi-Fi coprocessor** satisfy the dual-network requirement without external hardware.
- **SDIO 3.0 microSD slot** provides high-throughput SD card logging.

### Key Constraints

- **Wi-Fi is indirect:** All Wi-Fi traffic passes through the ESP32-C6 over SDIO via ESP-Hosted. This adds latency (~1–2 ms) but is transparent to the application via the standard `esp_wifi` API.
- **28 exposed GPIOs:** Pin planning is critical. The 4 sensor ports require 12 GPIOs (4× TX, RX, SYNC), plus UART0 is consumed by the USB-UART bridge for debug console.
- **No battery-backed absolute time in firmware:** The device clock is intentionally free-running only. If GNSS or RTC correlation is needed, it is performed offboard against captured device timestamps.

---

## 2. Hardware Architecture

### 2.1 Pin Allocation

Pins consumed by onboard peripherals (fixed, cannot be reassigned). Sources: [Waveshare ESP32-P4-Nano schematic](https://files.waveshare.com/wiki/ESP32-P4-NANO/ESP32-P4-NANO-schematic.pdf) and [Waveshare Wiki](https://www.waveshare.com/wiki/ESP32-P4-Nano):

| Function | Pins |
|----------|------|
| Ethernet RMII | GPIO28–31, 34–35, 49–52 |
| SD Card (SDIO) | GPIO39–44 |
| Audio Codec (I2S) | GPIO9–13, 53 |
| I2C Bus | GPIO7 (SDA), GPIO8 (SCL) |
| USB-UART (Console) | UART0 via USB-C bridge |

Available header GPIOs for sensor ports (from the 28 exposed). Initial assignments use sequential GPIO groups from the available header pins; these are provisional and must be cross-referenced against the Waveshare schematic before PCB layout:

| Sensor Port | UART Peripheral | TX Pin | RX Pin | SYNC Pin |
|-------------|----------------|--------|--------|----------|
| Port 0 | UART1 | GPIO14 | GPIO15 | GPIO16 |
| Port 1 | UART2 | GPIO17 | GPIO18 | GPIO19 |
| Port 2 | UART3 | GPIO20 | GPIO21 | GPIO22 |
| Port 3 | UART4 | GPIO23 | GPIO24 | GPIO25 |

The ESP32-P4's GPIO matrix allows any UART signal to be routed to any GPIO, so assignments can be adjusted to match harness routing without firmware restructuring.

### 2.2 Physical Connector

Each sensor port uses a **keyed panel-mount circular connector** with a flying lead harness to a 5-pin header on the PCB. The circular connector provides robust field mating and polarity protection. The 5 signals are wired:

| Pin | Signal | Source |
|-----|--------|--------|
| 1 | VCC (3.3 V) | Regulated 3.3 V rail, 200 mA per port via polyfuse |
| 2 | GND | Common ground |
| 3 | RX (hub ← sensor) | Mapped to UART peripheral RX |
| 4 | TX (hub → sensor) | Mapped to UART peripheral TX |
| 5 | SYNC | GPIO, direction configured per port |

### 2.3 Power Budget

| Consumer | Estimated Current (3.3 V) |
|----------|--------------------------|
| ESP32-P4 (active, dual-core, PSRAM) | ~250 mA |
| ESP32-C6 (Wi-Fi active) | ~120 mA |
| IP101GRI Ethernet PHY | ~50 mA |
| SD Card (write bursts) | ~100 mA |
| 4× Sensor ports (200 mA each max) | ~800 mA |
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
| Logging | `esp_log.h` | ESP-IDF |

### 3.2 Task Architecture

The firmware runs on FreeRTOS SMP across the two HP cores. Tasks are pinned to cores to isolate real-time operations from I/O-heavy work.

```
┌─────────────────────────────────────────────────────────────────┐
│                        CORE 0 (Real-Time)                       │
│                                                                 │
│  ┌──────────────┐  ┌───────────────────────┐                   │
│  │ Trigger Gen  │  │   SYNC Edge Capture   │                   │
│  │   (high)     │  │   (ISR + deferred)    │                   │
│  └──────────────┘  └───────────────────────┘                   │
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
| `trigger_gen` | 0 | 22 | 2 KB | Configures MCPWM for trigger output, timestamps pulses |
| `sync_capture` | 0 | 22 | 4 KB | Deferred processing of SYNC edge ISR events |
| `uart_rx[0-3]` | 0 | 20 | 4 KB each | Reads DMA ring buffers, invokes parser, timestamps packets |
| `sd_logger` | 1 | 12 | 8 KB | Drains central log buffer to SD card in large sequential writes |
| `udp_streamer` | 1 | 12 | 8 KB | Drains central log buffer to UDP sockets (Wi-Fi + Ethernet) |
| `ntrip_client` | 1 | 10 | 8 KB | TCP connection to NTRIP caster, RTCM forwarding, GGA upload |
| `http_api` | 1 | 8 | 6 KB | REST API for parser assignment and session control |
| `config_mgr` | 1 | 6 | 4 KB | Reads and validates SD card config at startup |

Core 0 real-time task stacks (`trigger_gen`, `sync_capture`, `uart_rx[]`) are allocated in **internal SRAM** for deterministic latency. Core 1 I/O/application task stacks are allocated in PSRAM.

### 3.4 Inter-Task Communication

```
                    ┌─────────────────────────┐
                    │   Central Log Buffer     │
                    │ (custom SPMC ring in     │
                    │  PSRAM, ~2 MB)           │
                    └─────┬──────────┬─────────┘
                          │          │
              ┌───────────▼──┐  ┌───▼───────────┐
              │  SD Logger   │  │  UDP Streamer  │
              └──────────────┘  └────────────────┘

  Producers:
    UART RX tasks  ──→  Log Buffer  (timestamped data records)
    SYNC Capture   ──→  Log Buffer  (edge event records)
    Trigger Gen    ──→  Log Buffer  (trigger event records)
    NTRIP Client   ──→  Log Buffer  (RTCM status records)
```

The central log buffer is a custom **single-producer, multi-consumer (SPMC) ring** in PSRAM. A dedicated `log_mux` task is the only producer into this ring; all source tasks push records into small per-source SPSC mailboxes consumed by `log_mux`. This avoids multi-producer contention in ISR paths while allowing independent consumer read pointers.

- The SPMC capacity is the designed retention window for all consumers (SD + UDP). At worst-case ingress (~400 KB/s including metadata), a 2 MB ring provides ~5 seconds of history.
- If the SD consumer lags beyond available SPMC history, firmware logs `REC_ERROR/ERR_SD_BACKPRESSURE` and continues best-effort logging (session is not force-stopped).
- UDP interfaces use the same SPMC retention window (no dedicated retransmit ring). If a UDP consumer lags past retention, dropped stream data is reported as INFO-level status (`REC_STATUS` / `udp_drop`).

**RTCM data path** (separate from central log buffer):
```
  NTRIP Client ──TCP──→ RTCM Buffer ──UART TX──→ Designated sensor port(s)
```

RTCM forwarding uses a non-blocking TX path (`uart_tx_chars()` in bounded chunks) with a dedicated 4 KB software ring buffer per target port. If the ring is full, oldest pending RTCM bytes are dropped and `REC_ERROR/ERR_RTCM_TX_OVERRUN` is logged (required behavior).

### 3.5 Memory Map

| Region | Size | Usage |
|--------|------|-------|
| Internal SRAM (768 KB) | ~192 KB | UART DMA buffers (4× 48 KB) |
| | ~14 KB | Core 0 real-time task stacks |
| | ~260 KB | ESP-IDF driver/runtime overhead (Ethernet + SDMMC + ESP-Hosted + lwIP + interrupts), measured target |
| | ~298 KB | FreeRTOS kernel, heap metadata, control structures, guard headroom |
| PSRAM (32 MB) | ~2 MB | Central log ring buffer (shared SD + UDP retention window) |
| | ~494 KB | Core 1 task stacks and application heaps |
| | ~256 KB | HTTP server buffers, cJSON workspace |
| | ~29 MB | Available headroom |
| Flash (16 MB) | ~2 MB | Firmware image |
| | ~14 MB | OTA-reserved partition (future feature), NVS, config, logs metadata |

Internal SRAM estimates are verified in a representative `sdkconfig` by initializing Ethernet + SDMMC + ESP-Hosted + 4 UARTs and recording `heap_caps_get_free_size(MALLOC_CAP_INTERNAL)` before and after each subsystem bring-up.

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
1. Logs an overrun record to the central log buffer (record type `REC_ERROR`, subtype `ERR_UART_OVERRUN`, with port index and timestamp).
2. Flushes the UART RX FIFO to resynchronize.
3. Increments a per-port overrun counter reported in telemetry.

#### Parser Framework (REQ-UART-04, REQ-UART-05)

**Design options considered:**

| Approach | Pros | Cons |
| -------- | ---- | ---- |
| **Vtable (function-pointer struct)** — chosen | Idiomatic C, zero overhead, trivially unit-testable, no dynamic dispatch framework needed | Parsers registered at compile time (link-time); no runtime plugin loading |
| Compile-time switch/enum dispatch | Simple, no indirection | `switch` must be modified in core firmware to add a parser — violates REQ-UART-04 |
| Dynamic shared library (.so) | True runtime loading | No filesystem exec support on ESP32; impractical on bare-metal RTOS |
| Scripting engine (Lua/MicroPython) | User-writable parsers without reflash | High memory overhead, nondeterministic timing, complex integration |

The vtable approach is selected. It satisfies REQ-UART-04 (adding a parser does not modify core firmware, only a new translation unit is linked) and REQ-UART-05 (parser assignment is runtime-configurable) with minimal overhead:

```c
typedef struct {
    const char *name;                          // e.g., "nmea", "ubx"
    void *(*create)(const parser_config_t *cfg);
    parse_result_t (*feed)(void *ctx, const uint8_t *data, size_t len);
    void (*destroy)(void *ctx);
} parser_type_t;
```

- `feed()` is called as bytes arrive from the DMA ring buffer. It is a **streaming state machine**: it accepts an arbitrary chunk of bytes, advances its internal state, and returns `PARSE_INCOMPLETE`, `PARSE_COMPLETE` (with a pointer to the assembled packet in the parser's internal buffer), or `PARSE_ERROR`.
- **Split-boundary handling:** Because `feed()` receives raw byte chunks from the DMA ring buffer, a chunk may contain the tail of one packet and the head of the next — or a packet may be split across multiple calls. This is handled correctly by the streaming state machine model: the parser holds state between calls and will finish the current packet from wherever it left off, then immediately begin accumulating the next. No framing is lost at chunk boundaries.
- Parsers are registered at compile time in a `parser_registry[]` array. Adding a new parser requires only: (1) implement the three functions, (2) add an entry to the registry.
- Parser assignment per port is stored in the runtime config and can be changed via the HTTP API before a session starts.

**Built-in parsers:**
- **NMEA 0183:** Scans for `$` start, accumulates until `\r\n`, validates checksum. Emits complete sentences as opaque byte blobs (no field parsing on-device).
- **UBX Binary:** State machine scanning for `0xB5 0x62` sync bytes, reads class/ID/length, accumulates payload, validates Fletcher-16 checksum.

Both parsers annotate each complete packet with the associated SYNC event index (see §4.3). Per-packet receive timestamp interpolation is not required in this revision; SYNC timing is the source of truth.

### 4.2 Clock Subsystem

**Satisfies:** REQ-CLK-01 through REQ-CLK-03

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

#### Free-Running Only Behavior (REQ-CLK-02, REQ-CLK-03)

- The device clock is **never disciplined or adjusted** from GNSS SYNC, an RTC, or any network time source.
- SYNC inputs are used only to timestamp edges and associate packets; they do not redefine the device clock.
- Session logs and UDP streams carry only device-clock timestamps from the shared GP Timer.
- If an operator needs absolute-time alignment to GNSS or RTC time, that correlation is performed offboard against recorded device timestamps.

### 4.3 Synchronization Subsystem

**Satisfies:** REQ-SYNC-01 through REQ-SYNC-03

#### SYNC Edge Capture (REQ-SYNC-01, REQ-SYNC-02)

Each port's SYNC pin is configured as a GPIO interrupt source when operating as `sync_direction: "input"`:

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

The `sync_capture` task dequeues these events and writes `REC_SYNC_EDGE` records to the central log buffer.

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
- Trigger capability is per-port and independent: any subset of ports may be configured as trigger outputs simultaneously.
- A port is mutually exclusive between SYNC input and trigger output (`sync_direction` is either `input` or `output`).
- UART RX/TX functionality remains available when a port is a trigger output because MCPWM and UART use different GPIO matrix routes.

**Trigger timestamping (REQ-TRIG-03):** The MCPWM peripheral fires an interrupt on each timer overflow (period start). The ISR reads the same shared GP Timer counter used by the SYNC edge ISR (§4.3) and pushes a `REC_TRIGGER` event to the central log buffer. This is a **separate ISR** from the GPIO SYNC edge handler — MCPWM overflow and GPIO edge are distinct interrupt sources. They share only the GP Timer as a common timebase, which is what ensures trigger timestamps and SYNC edge timestamps are directly comparable in the log. ISR latency (~500 ns) is negligible relative to the 1 µs precision requirement.

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
- The maximum no-drop window is defined by the SPMC ring depth at current ingress rate (configured design target: ~5 seconds at worst-case ingress).

#### SD Card Removal / I/O Failure

If any SD write/fsync call fails during an active session:
1. Log `REC_ERROR/ERR_SD_IO`.
2. Attempt a single unmount/remount sequence.
3. If remount fails, transition to UDP-only degraded mode for the remainder of boot and expose this in `/api/status`.
4. No auto-resume is attempted on later card reinsertion; operator must reboot or start a new session after card service.

#### File Naming (REQ-SD-05)

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

A separate `.log` file in the same session directory contains timestamped plain-text lines. Unlike the binary log, the `.log` file is **not pre-allocated** — status events are infrequent (a few lines per second at most) so write amplification from pre-allocation is not worthwhile. Corruption protection is provided by:

- Each line is a complete, self-contained plain-text record terminated by `\n`; a partial write at power loss leaves at most one truncated line at the end, which is readable up to that point.
- `fsync()` is called on the `.log` file descriptor after every write, flushing the FAT cluster chain.
- The binary log's `session_complete` flag (§5.1) serves as the authoritative session integrity marker; the `.log` file is treated as advisory.

```
[000000.000000] SESSION START session=session_00042
[000001.234567] NTRIP connected to rtk2go.com:2101/MOUNTPT
[000005.678901] UART2 overrun: 128 bytes lost
[000312.456789] UDP eth state=backlogged
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

**SDIO host separation:**
ESP32-P4 has two SDMMC hosts. This design assigns:
- `SDMMC_HOST_0` to microSD storage
- `SDMMC_HOST_1` to ESP-Hosted (C6 SDIO link)
This prevents Wi-Fi traffic from contending with SD card traffic on the same host controller.

Both interfaces support DHCP (default) and static IP, configured via the SD card config file. Each interface gets its own `esp_netif` instance.

#### UDP Streaming (REQ-NET-02, REQ-NET-03)

The `udp_streamer` task maintains two independent UDP sockets (one per interface). Each socket has:
- A configurable destination IP and port.
- A configurable MTU / payload size (default: 1400 bytes).
- An independent read pointer into the central log ring buffer.

Records are batched into UDP datagrams up to the configured payload size. The datagram format is identical to the binary log format (§5), prefixed with a sequence number for gap detection on the receiver side.

#### Resilience (REQ-NET-05 through REQ-NET-08)

Each interface tracks its state as one of: `STREAMING`, `BACKLOGGED`, `DISCONNECTED`.

- **Disconnect detection (REQ-NET-05):** Primary link state comes from ESP-IDF events (`ETH_EVENT_DISCONNECTED`, `IP_EVENT_ETH_LOST_IP`, `WIFI_EVENT_STA_DISCONNECTED`). `sendto()` errors are secondary indicators. A target receiver is also considered failed if optional application heartbeat ACKs are absent for `target_timeout_s` while link is up.
- **Retention window (REQ-NET-06):** No per-interface retransmit ring is used. UDP consumers read from the same SPMC central log ring used by SD. If a UDP consumer falls behind beyond SPMC retention, stream data is dropped and an INFO-level status record is emitted (`REC_STATUS` with `udp_drop` details). The SD writer continues independently.
- **SD independence (REQ-NET-07):** The SD logger has its own independent read pointer and runs on a separate task. Network state has no effect on SD writes.
- **State reporting (REQ-NET-08):** Interface state changes are logged as `REC_NET_STATUS` records in both the SD log and the UDP stream.

### 4.7 NTRIP Client

**Satisfies:** REQ-RTCM-01 through REQ-RTCM-04

#### Connection

The `ntrip_client` task opens a TCP socket to the configured NTRIP caster using the configured `network_interface` (`ethernet`, `wifi`, or `auto`) and sends an HTTP-style request:

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

Incoming TCP data is buffered in a 4 KB ring buffer. Data is forwarded to designated UART port(s) with non-blocking chunked TX (`uart_tx_chars()`). The UART TX FIFO (128 bytes) plus software ring absorb bursts.

Latency budget: TCP receive (~10–50 ms network) + memcpy to UART TX buffer (~10 µs) + UART transmission (at 115200 bps, 1 KB takes ~90 ms). Total well within 100 ms for typical RTCM message sizes (< 500 bytes).

If UART TX capacity is exhausted, excess RTCM bytes are dropped (oldest first in the per-port RTCM TX ring), and `REC_ERROR/ERR_RTCM_TX_OVERRUN` is logged. RX and TX remain independent hardware paths, so RX capture continues unaffected.

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
| GET | `/api/status` | Device status, uptime, current device-clock count, per-port stats | Always available |
| GET | `/api/parsers` | List available parser types | Always available |
| GET | `/api/config` | Current configuration (active + pending if staged) | Always available |
| PUT | `/api/ports/{id}/parser` | Assign parser to port | Rejected (409) during active session |
| PUT | `/api/config` | Stage new config and reboot (see below) | Always available; stops active session first |
| POST | `/api/session/start` | Start a logging session | No-op if already active |
| POST | `/api/session/stop` | Stop the active session | Always available; no-op if not active |

Request and response bodies are JSON, parsed/generated with `cJSON`.

API authentication is intentionally not implemented in this revision; it is tracked as a future feature.

#### Session Stop

`POST /api/session/stop` is always accepted, including during an active session. On receipt, the logger flushes its write buffer, calls `fsync()`, writes the `session_complete` flag, and closes the session files before returning HTTP 200.

#### Config Update and Reboot (`PUT /api/config`)

Uploading a new configuration follows a **staged reboot with confirmation** flow to prevent a bad config from permanently locking the device out of the network:

1. Client sends `PUT /api/config` with the new JSON body.
2. Server validates the config. If invalid, returns HTTP 400 with error details — no change is made.
3. If valid, the server:
   - Writes the new config to `/sd/config_pending.json`.
   - Stops any active session cleanly.
   - Reboots into the new config.
4. On boot with a `config_pending.json` present, the firmware loads it and **starts a 60-second confirmation timer**.
   - If `POST /api/config/confirm` is received before the timer expires, the pending config is promoted to `/sd/config.json` and the timer is cancelled.
   - If the timer expires without confirmation, the firmware reverts to the previous `/sd/config.json` and reboots again.
5. This ensures that a config change that breaks network connectivity (wrong IP, bad SSID) self-reverts, because the client will be unable to reach the device to confirm.

### 4.9 Configuration

**Satisfies:** REQ-CFG-01, REQ-CFG-02

#### Config File

The configuration file is `/sd/config.json`, read at startup. See §6 for the full schema.

#### Validation (REQ-CFG-02)

At startup, `config_mgr`:
1. Reads `/sd/config.json`.
2. Parses with `cJSON`.
3. Validates each field against allowed ranges.
4. If any field is missing/invalid, logs an error to console and status log and marks config invalid.
5. If config is invalid, the device halts session start and exposes `config_invalid` state via HTTP API until corrected. No runtime fallback defaults are applied.

### 4.10 Watchdog & Recovery

**Satisfies:** REQ-NF-02

- The ESP-IDF Task Watchdog Timer uses differentiated supervision:
  - Core 0 real-time tasks: 1-2 s timeout; failure triggers full system reboot.
  - Core 1 I/O tasks: 5-10 s timeout; failure triggers task-level restart/session stop first (minimized blast radius), with full reboot only if restart fails repeatedly.
- On boot, the firmware checks the reset reason via `esp_reset_reason()`. If it was a watchdog or panic reset, a `REC_ERROR` record with subtype `ERR_WATCHDOG_RESET` is written as the first record of the new session.
- Each session starts with a new log file, so data from the previous session remains intact.

---

## 5. Binary Log Format

**Satisfies:** REQ-SD-02, REQ-SD-03, REQ-NET-03

### 5.1 Session Header

Written once at the start of each log file. Header size is fixed at 256 bytes so `session_complete` is always at a known offset.

```
Offset  Size  Field
0       4     Magic: 0x44415148 ("DAQH")
4       2     Format version (uint16, currently 3)
6       2     Header length (uint16, fixed 256)
8       4     Session counter (uint32)
12      8     Device clock at session start (uint64, microseconds since boot)
20      2     Number of ports (uint16)
22      N     Per-port descriptors (16 bytes each)
252     1     session_complete flag (0x00 = incomplete, 0x01 = complete)
253     3     Reserved / padding (0x00)
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

All records after the header follow a uniform TLV structure with monotonic per-record sequence numbers:

```
Offset  Size  Field
0       1     Record type (uint8)
1       2     Payload length (uint16, little-endian, max 65535)
3       8     Timestamp (uint64, microseconds, little-endian)
11      4     Record sequence number (uint32, wraps)
15      N     Payload (type-specific)
15+N    2     CRC-16-CCITT (poly 0x1021, init 0xFFFF) over bytes 0 through 14+N
```

### 5.3 Record Types

| Type ID | Name | Payload |
|---------|------|---------|
| 0x01 | `REC_DATA` | port_index(1) + sync_seq(4) + raw_packet_bytes(N) |
| 0x02 | `REC_SYNC_EDGE` | port_index(1) + edge(1: 0=fall,1=rise) |
| 0x03 | `REC_TRIGGER` | port_index(1) + pulse_width_us(2) |
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
      "sync_direction": "output",
      "nominal_rate_hz": 5,
      "trigger": {
        "enabled": true,
        "rate_hz": 100,
        "pulse_width_us": 10
      },
      "gga_source": true,
      "rtcm_target": false
    }
  ],
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
    "network_interface": "auto",
    "gga_interval_s": 10,
    "reconnect_interval_s": 5
  }
}
```

**Required fields and defaults:**

| Parameter | Default |
|-----------|---------|
| Runtime behavior for missing/invalid required fields | Config rejected; system blocks session start |
| `trigger.enabled` omitted | `false` |
| `ntrip.network_interface` omitted | `auto` |

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
| REQ-CLK-02 | §4.2 | No external time discipline or source switching in firmware |
| REQ-CLK-03 | §4.2, §5 | Logs and streams carry only device-clock timestamps; absolute-time correlation is offboard |
| REQ-SD-01 | §3.4, §4.5 | SPMC log ring defines retention window; SD overrun logged with continued best-effort writes |
| REQ-SD-02 | §5 | Binary TLV format: data packets, event records, error records |
| REQ-SD-03 | §5 | Fixed 256-byte header, deterministic completion flag offset, CRC-16 integrity |
| REQ-SD-04 | §4.5 | Separate `.log` file with timestamped plain-text lines |
| REQ-SD-05 | §4.5 | NVS-backed monotonic session counter in filenames |
| REQ-SD-06 | §4.5 | Pre-allocated file, periodic `fsync()`, self-delimiting records |
| REQ-NET-01 | §4.6 | IP101GRI Ethernet + ESP32-C6 Wi-Fi; DHCP/static via config |
| REQ-NET-02 | §4.6 | Dual independent UDP sockets, configurable destination and payload size |
| REQ-NET-03 | §5 | Same binary TLV format for SD and UDP |
| REQ-NET-04 | §4.6 | Independent `esp_netif` instances, no failover logic |
| REQ-NET-05 | §4.6 | Event-driven link detection + optional receiver heartbeat timeout |
| REQ-NET-06 | §3.4, §4.6 | UDP consumers share SPMC retention window; lag past window logs INFO drop status |
| REQ-NET-07 | §4.6 | SD logger has independent read pointer, unaffected by network state |
| REQ-NET-08 | §4.6 | `REC_NET_STATUS` records logged to SD and UDP |
| REQ-RTCM-01 | §4.7 | Custom TCP NTRIP client with configurable caster parameters |
| REQ-RTCM-02 | §4.7 | Non-blocking UART TX forwarding with explicit overflow-drop logging |
| REQ-RTCM-03 | §4.7 | Periodic GGA read from designated parser, sent over TCP |
| REQ-RTCM-04 | §4.7 | Reconnect loop with configurable interval and event logging |
| REQ-CFG-01 | §4.9, §6 | `/sd/config.json` with all operational parameters |
| REQ-CFG-02 | §4.9 | Strict JSON validation at startup; halt on any invalid config |
| REQ-CFG-03 | §4.8 | HTTP API on port 80; read-only during active session |
| REQ-NF-01 | §3.2 | Core 0 dedicated to real-time tasks; Core 1 handles I/O |
| REQ-NF-02 | §4.10 | Task WDT, reset-reason check, new session file on recovery |
| REQ-NF-03 | §4.1, §4.2 | Vtable interfaces and pure-function modules enable host-side unit testing |

---

## Revision History

| Version | Date       | Notes                                                                              |
|---------|------------|------------------------------------------------------------------------------------|
| 0.1     | 2026-03-10 | Initial design targeting ESP32-P4-Nano                                             |
| 0.2     | 2026-03-11 | PR review: CPU clock, connector, parser framework, HTTP API     |
| 0.3     | 2026-03-12 | Addressed outstanding PR comments: SPMC buffering, fixed header, CRC-16, sequence IDs, strict config validation, failure-mode handling, trigger/clock schema updates |
| 0.4     | 2026-03-12 | New PR comments: simplified UDP buffering (shared SPMC window), SD lag best-effort behavior, sync-only packet association, configurable clock master sync period |
| 0.5     | 2026-03-12 | Removed clock discipline and RTC/time-master support; all timestamps are free-running device-clock values |
