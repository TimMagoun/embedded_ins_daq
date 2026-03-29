# ESP32-P4-NANO Capability Reference

**Purpose:** Implementation-oriented reference for the Waveshare `ESP32-P4-NANO` board and the underlying `ESP32-P4` SoC.
**Last Reviewed:** 2026-03-13
**Use This For:** Pin budgeting, peripheral selection, storage design, timing strategy, firmware architecture, and bring-up planning.

______________________________________________________________________

## 1. Board Summary

The `ESP32-P4-NANO` is a development board built around the `ESP32-P4NRW32` and an onboard `ESP32-C6-MINI-1`.

Board-level features relevant to implementation:

- `ESP32-P4NRW32` main MCU
- onboard `32 MB` PSRAM in package with the P4
- onboard `16 MB` NOR flash
- onboard `ESP32-C6-MINI-1` for `Wi-Fi 6` and `Bluetooth 5/BLE`
- onboard TF card slot using `SDIO 3.0`
- `USB-C` port for power, programming, and debug
- `USB 2.0 OTG High Speed` Type-A port
- onboard `100M Ethernet` RJ45
- `2 x 13` GPIO headers exposing `28` programmable GPIOs
- `BOOT` and `RESET` buttons
- `USER-LED`
- RTC battery header
- microphone and speaker connections
- optional PoE module/header support

Important practical takeaway:

- This is a feature-rich dev board, but only `28` GPIOs are broken out on headers.
- For products that need multiple external connectors, power distribution, protection, and clean signal routing, plan on using the Nano with an external breakout/carrier rather than as the final wiring surface.

______________________________________________________________________

## 2. SoC Summary

The `ESP32-P4` is a high-performance dual-core RISC-V SoC with an additional LP core.

Implementation-relevant SoC characteristics:

- dual high-performance RISC-V cores up to `400 MHz`
- one LP core up to `40 MHz`
- single-precision FPU and AI instruction extensions
- `768 KB` on-chip HP SRAM
- `8 KB` zero-wait TCM RAM
- `55` programmable GPIOs on the SoC itself
- rich peripheral set including UART, GPIO matrix, timers, ETM, SDMMC, USB, Ethernet, I2C, SPI, TWAI, RMT, LEDC, MCPWM, ADC, PCNT, Dedicated GPIO

Practical meaning:

- There is enough compute headroom for concurrent capture, logging, and non-critical parsing.
- There is enough memory for substantial buffering, especially with onboard PSRAM, but ISR-critical structures should still stay in internal RAM until measured otherwise.

______________________________________________________________________

## 3. Memory and Storage Resources

## 3.1 Internal and External Memory

Verified board/SoC memory details:

- `768 KB` HP SRAM on the ESP32-P4
- `8 KB` TCM RAM
- `32 MB` package PSRAM on the `ESP32-P4NRW32`
- `16 MB` onboard NOR flash on the board

Implementation guidance:

- keep ISR queues, timing metadata, and hot circular-buffer control structures in internal RAM
- consider placing large bulk log buffers in PSRAM only after throughput testing
- store firmware, configuration defaults, and static assets in NOR flash

## 3.2 TF / SD Card

The board includes a TF card slot, and ESP-IDF documents two SDMMC host slots on ESP32-P4:

- `SDMMC_HOST_SLOT_1` is routed through the GPIO matrix and is suitable for standard SD-card use
- `SDMMC_HOST_SLOT_0` is dedicated to UHS-I mode, which ESP-IDF notes is not yet supported in the driver

Supported SDMMC speed modes in the driver include:

- Default Speed `20 MHz`
- High Speed `40 MHz`
- some UHS-I/eMMC modes, though not all are relevant for this board/use case

Implementation guidance:

- prefer the onboard TF slot in native `SDMMC` mode over SPI mode
- for this logger project, target standard `3.3 V` SD-card operation rather than UHS-I
- verify SD I/O voltage and pull-up requirements during hardware bring-up
- benchmark several real cards because card latency behavior matters more than peak advertised throughput

______________________________________________________________________

## 4. UART Capabilities

ESP-IDF states that the `ESP32-P4` has:

- `5` standard UART controllers
- `1` LP UART controller
- UART DMA support

Regular UART controllers support:

- configurable baud rate
- configurable data bits
- configurable stop bits
- parity configuration
- pin assignment through the SoC's flexible routing model

LP UART notes:

- it is a reduced-function peripheral
- it has much smaller RAM
- it does not support all features of the regular UART blocks

Implementation guidance:

- use regular UART controllers, not LP UART, for sensor capture
- a 4-sensor UART logger can use 4 native UARTs and still leave 1 standard UART available for console/debug
- use interrupt-driven RX and evaluate DMA where it improves burst tolerance and CPU load
- final feasibility depends on the board's exposed pin map, not just the SoC UART count

Recommended mapping strategy for this project:

- `UART0`: console / bring-up / recovery
- `UART1`-`UART4`: sensor ports
- `UART5` equivalent remaining regular controller: reserve as spare or alternate service path depending on naming in the SDK and final pin map

Note:

- Confirm exact driver numbering and exposed pins in code and schematic before locking the implementation.

______________________________________________________________________

## 5. GPIO and Signal Routing

Board vs SoC:

- SoC provides `55` programmable GPIOs
- board headers expose `28` programmable GPIOs

This difference is one of the most important implementation constraints.

Implications:

- peripheral count on the SoC is not the same as usable external signal count on the dev board
- some useful SoC functions may already be tied to onboard devices or not broken out
- final pin planning must account for TF slot, Ethernet, USB, and any other onboard consumers

Helpful routing capabilities:

- ESP32-P4 supports flexible signal routing through the GPIO matrix
- this makes UART and timing signal placement much more flexible than on fixed-function pin architectures

Implementation guidance:

- do not commit to a final multi-port connector scheme until the board pin-definition image or schematic is checked directly
- reserve the cleanest header pins for timing-sensitive inputs first
- if using the board only for development, move sensor connectors and protection circuitry onto a separate breakout board

______________________________________________________________________

## 6. Timing and Real-Time Control Features

ESP-IDF exposes several peripherals especially relevant to low-jitter logging and trigger generation:

- `GPTimer`
- `Event Task Matrix (ETM)`
- `Dedicated GPIO`
- `RMT`
- `PCNT`
- `LEDC`
- `MCPWM`

## 6.1 GPTimer

`GPTimer` is the main general-purpose high-resolution timer block documented in ESP-IDF.

Useful properties for implementation:

- free-running count source
- one-shot or periodic alarms
- event generation
- ETM integration

Good fits for this project:

- monotonic microsecond device clock
- scheduled trigger generation
- periodic health/checkpoint activity if needed

## 6.2 ETM

The `Event Task Matrix (ETM)` lets one peripheral trigger another without CPU interrupt intervention.

Example uses documented by Espressif include:

- toggling GPIO on timer alarm
- starting other peripheral actions from events

Why this matters here:

- ETM can reduce latency and jitter compared with software ISR chaining
- it is a strong candidate for precise trigger-output generation tied to timer events

## 6.3 Dedicated GPIO

`Dedicated GPIO` is exposed in ESP-IDF as a distinct peripheral class.

Why it is worth considering:

- it exists specifically to support lower-latency GPIO access patterns than generic software-driven GPIO handling
- it may be useful for trigger outputs or tightly controlled debug strobes

## 6.4 Candidate Timing Approaches

Best likely choices for this project:

- device clock: `GPTimer`
- trigger scheduling: `GPTimer` + `ETM`, or `GPTimer` ISR if ETM proves awkward
- SYNC input capture: GPIO interrupt path first, with possible ETM-assisted designs if needed later

Decision rule:

- start with the simplest path that meets jitter and timestamp requirements
- only add ETM complexity if measurements show ISR-based toggling is not good enough

______________________________________________________________________

## 7. Onboard Interfaces and Their Implementation Impact

## 7.1 USB-C Debug / Programming Port

Useful for:

- firmware flashing
- boot logs
- field diagnostics
- recovery when SD configuration is broken

Recommended use:

- keep the default console path available during early bring-up

## 7.2 USB OTG High Speed Type-A

Present on the board, but usually not needed for the sensor logger.

Keep out of scope unless a future revision needs:

- USB mass storage
- USB devices
- USB-connected instrumentation

## 7.3 Ethernet

The board includes `100M Ethernet`.

Useful for future expansion, but not required by the current PRD.

Recommendation:

- do not let Ethernet consume schedule or design attention in the initial logger revision

## 7.4 ESP32-C6 Wireless Companion

The board includes an `ESP32-C6-MINI-1` connected over `SDIO`.

Useful for future revisions that may need:

- Wi-Fi provisioning
- wireless monitoring
- BLE control or status

Recommendation for this project:

- leave it unused in the first implementation
- do not depend on it for session control, logging, or timing

## 7.5 Audio / Camera / Display Features

The board and SoC support:

- microphone
- speaker
- MIPI camera
- MIPI display
- image/video acceleration

These are not relevant to the UART logger core and should remain out of scope.

Their main implementation relevance is negative:

- they consume pins, bandwidth, and attention if accidentally treated as in-scope

______________________________________________________________________

## 8. Power and Bring-Up Considerations

Board-level power-related features visible from vendor materials:

- `USB-C` power input
- external `5V` power header
- optional PoE path
- RTC battery header

Implementation guidance:

- for a sensor hub, do not power external sensors directly from uncertain dev-board header assumptions without checking regulator limits
- if powering multiple external sensors, add explicit load-switching and current limiting on the sensor breakout/carrier
- treat the dev board as controller logic, not as the final field-power-distribution design

______________________________________________________________________

## 9. SDK and Software Support

The main software platform is `ESP-IDF`, which provides ESP32-P4 documentation and drivers for:

- UART
- SDMMC
- GPIO
- GPTimer
- ETM
- Dedicated GPIO
- RMT
- PCNT
- USB host/device
- Ethernet-related support
- many additional peripherals

Implementation guidance:

- prefer ESP-IDF drivers first rather than direct register work during early implementation
- keep the design close to peripherals that have stable ESP-IDF support
- if timing measurements show driver overhead is too high, optimize only the hot paths

For this project, the most relevant ESP-IDF areas are:

- UART driver and DMA support
- GPTimer
- ETM
- GPIO and interrupt handling
- SDMMC host driver
- FreeRTOS tasking and memory placement

______________________________________________________________________

## 10. What This Means for the Sensor Logger

The `ESP32-P4-NANO` is a good fit for the logger because it has:

- enough native UART hardware for 4 sensor ports
- enough compute for concurrent capture and logging
- enough memory for large buffering
- a native TF/SDMMC path for better storage throughput
- useful timing peripherals for low-jitter trigger generation

The main constraints are:

- only `28` header-exposed GPIOs on the dev board
- onboard peripherals may compete for attractive pins
- the dev board still needs an external breakout/carrier for practical 4-port deployment

Recommended implementation posture:

- use the board as the controller and storage platform
- build sensor connectors, power protection, and any level protection on a companion board
- use native UARTs before considering external UART bridges
- keep capture-critical data in internal RAM
- use PSRAM for larger staging buffers only after measurement
- use the onboard TF slot in `SDMMC` mode
- preserve the console path during bring-up

______________________________________________________________________

## 11. Open Items To Verify Before Locking Hardware

These items should be checked directly against the board pinout image, schematic, and SDK examples before freezing implementation:

- exact header pins available for 4 UART RX/TX pairs
- exact header pins available for 4 SYNC lines
- whether chosen timing pins share constraints with onboard devices
- whether any selected pins have boot, strapping, or debug implications
- usable SD-card performance with realistic cards and concurrent UART load
- whether PSRAM-backed buffers meet worst-case latency goals
- actual sensor power budget available from the chosen carrier design

______________________________________________________________________

## 12. Recommended Source Set

Primary sources used for this reference:

- Espressif ESP32-P4 SoC overview
  https://www.espressif.com/en/products/socs/esp32-p4
- ESP-IDF ESP32-P4 peripherals index
  https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-reference/peripherals/index.html
- ESP-IDF ESP32-P4 UART driver docs
  https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-reference/peripherals/uart.html
- ESP-IDF ESP32-P4 SDMMC host docs
  https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-reference/peripherals/sdmmc_host.html
- Waveshare ESP32-P4-NANO wiki
  https://www.waveshare.com/wiki/ESP32-P4-NANO
- Waveshare ESP32-P4-NANO product page
  https://www.waveshare.com/esp32-p4-nano.htm

Recommended follow-up references when implementation starts:

- ESP32-P4 Technical Reference Manual
- `ESP32-P4-NANO` schematic and pinout files
- latest ESP-IDF examples for UART, GPTimer, ETM, and SDMMC
