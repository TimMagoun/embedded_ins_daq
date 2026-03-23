# ESP32-P4-NANO Pin Planning Checklist

**Purpose:** Working checklist for assigning pins and verifying how the `ESP32-P4-NANO` interfaces with onboard and external peripherals.
**Use This For:** Carrier-board planning, firmware bring-up, connector mapping, and avoiding board-level pin conflicts.
**Companion Reference:** [ESP32_P4_NANO_capabilities.md](/Users/timmagoun/Projects/embedded_ins_daq/ESP32_P4_NANO_capabilities.md)

______________________________________________________________________

## 1. How To Use This Checklist

Before locking any hardware or firmware pin map:

- check the official `ESP32-P4-NANO` schematic
- check the official board pinout image
- check the latest `ESP-IDF` examples for the selected peripheral
- record both the SoC peripheral used and the physical header pin used

Rule of thumb:

- do not treat a SoC capability as available until the signal is confirmed on the actual board headers or intentionally routed through an onboard device

______________________________________________________________________

## 2. Board-Level Verification

Confirm these first:

- which header pins are actually exposed on the `2 x 13` headers
- which exposed pins are already shared with onboard peripherals
- which pins are input-only, output-capable, or have electrical caveats
- which pins have boot, strapping, reset, JTAG, or console implications
- which onboard interfaces consume non-header pins and therefore do not affect external routing
- whether any pins are reserved by the board support package or default examples

Record here during bring-up:

- board revision:
- schematic source:
- pinout source:
- ESP-IDF version:

______________________________________________________________________

## 3. Core External Interface Budget

For the sensor logger, the minimum external signal budget is:

- `4 x UART RX`
- `4 x UART TX`
- `4 x SYNC`
- `1 x session control input`
- `1 x status LED` if not using onboard LED
- optional per-port power-fault inputs
- optional spare debug GPIO

Minimum baseline external pin count:

- `8` pins for UART
- `4` pins for SYNC
- `1-3` pins for controls/status/debug

This means the project is pin-feasible on the dev board, but only if the final map avoids conflicts with the onboard TF slot and any already-claimed board functions.

______________________________________________________________________

## 4. Peripheral Planning Checklist

## 4.1 UART

Target use:

- 4 sensor UART ports
- 1 console / debug UART

Checklist:

- identify the exact `UART` controller instances available in ESP-IDF for ESP32-P4
- keep `UART0` available for boot logs and recovery unless there is a strong reason not to
- assign `4` regular UARTs to sensor ports
- confirm each chosen TX/RX signal can be routed to exposed header pins
- confirm selected pins are not already tied to TF, Ethernet, USB, or other onboard interfaces
- confirm electrical direction labeling for each external connector
- verify whether RTS/CTS are unnecessary and can be omitted
- verify baud-rate targets up to `921600` on the chosen pins
- verify whether DMA is supported and beneficial for the selected driver path

Record mapping:

- console UART:
- sensor port 1 UART:
- sensor port 2 UART:
- sensor port 3 UART:
- sensor port 4 UART:

Questions to answer:

- Are all 4 sensor UARTs available on exposed pins at the same time?
- Does using 4 sensor UARTs force the console onto a less convenient path?
- Are any selected UART pins shared with strapping or boot-critical functions?

## 4.2 SYNC Inputs / Trigger Outputs

Target use:

- 4 per-port `SYNC` lines, each configurable as input or output

Checklist:

- identify `4` clean GPIO-capable pins for SYNC use
- confirm each selected pin supports interrupt-driven edge detection
- confirm each selected pin can also operate as a push-pull output when used as trigger output
- prefer pins with short, simple routing on the external carrier
- avoid pins that are noisy or physically adjacent to high-speed interfaces if possible
- document whether any selected SYNC pins need external protection or series resistors
- verify input threshold and output drive suitability for connected sensors
- confirm firmware can remap SYNC role per port without board rework

Record mapping:

- port 1 SYNC:
- port 2 SYNC:
- port 3 SYNC:
- port 4 SYNC:

Questions to answer:

- Are the selected SYNC pins fully bidirectional in the board design?
- Do any selected pins have board-level pull-ups/pull-downs that could interfere with timing?
- Is there a better subset of pins for lower-noise edge capture?

## 4.3 SD / TF Card

Target use:

- binary session log
- human-readable status log
- startup configuration file

Checklist:

- confirm the onboard TF slot wiring and whether it uses `SDMMC_HOST_SLOT_1`
- confirm the TF signals do not consume header pins needed for external interfaces
- verify the board support package or example code for the correct slot assignment
- confirm `3.3 V` SD-card operation
- confirm card-detect behavior if present
- benchmark write throughput and worst-case latency with several cards
- verify filesystem and flush strategy under power-loss scenarios

Record details:

- SDMMC slot used:
- card detect available:
- expected filesystem:
- tested card types:

Questions to answer:

- Does the onboard TF slot conflict with any desired external pins?
- What is the worst-case observed write stall with realistic cards?

## 4.4 Console / Debug Path

Target use:

- flashing
- boot logs
- field diagnostics

Checklist:

- identify the default programming/debug UART path on the board
- confirm whether it is always accessible through `USB-C`
- keep that path free during initial bring-up
- document any alternative console path if `UART0` is repurposed later
- verify boot messages do not interfere with attached external devices during development

Record details:

- default console path:
- alternate console path:

## 4.5 User Controls and Indicators

Target use:

- local start/stop control
- visible status indication

Checklist:

- identify onboard `BOOT`, `RESET`, and `USER-LED` connections
- determine whether onboard controls can be reused by the application
- verify whether the `USER-LED` is on a pin safe to use during active logging
- decide whether an external button or LED is needed on the carrier board
- confirm button inputs do not overlap with strapping or boot-sensitive pins

Record details:

- onboard LED pin:
- onboard user input pin:
- external LED needed:
- external button needed:

## 4.6 Ethernet

Target use:

- not required for the initial logger revision

Checklist:

- identify whether Ethernet consumes any exposed header pins
- avoid assigning external signals to pins required by onboard Ethernet
- do not allocate firmware effort to Ethernet unless the product scope changes

Record details:

- Ethernet pin conflict notes:

## 4.7 USB OTG

Target use:

- not required for the initial logger revision

Checklist:

- identify whether OTG-related pins are exposed or internally dedicated
- avoid accidental conflicts with any desired external signal routing
- keep out of the first-pass implementation

Record details:

- USB OTG pin conflict notes:

## 4.8 ESP32-C6 Companion

Target use:

- unused in initial logger revision

Checklist:

- confirm whether `ESP32-C6` communication lines are fully internal to the board
- confirm they do not consume needed external headers
- do not allocate critical functions to the C6 in revision 1

Record details:

- C6 pin conflict notes:

______________________________________________________________________

## 5. Timing Peripheral Checklist

These items are not mainly about external pins, but they strongly affect implementation choices.

## 5.1 Device Clock

Checklist:

- choose whether the main device clock uses `GPTimer` or another verified microsecond-capable timer source
- verify the selected timer has stable API support in the target ESP-IDF version
- confirm timestamp reads are safe and low-latency from ISR context

Record choice:

- selected timer source:

## 5.2 Trigger Generation

Checklist:

- decide whether trigger generation uses `GPTimer` ISR, `ETM`, `Dedicated GPIO`, or a hybrid
- confirm the chosen trigger output pins support the required output behavior
- verify jitter under concurrent UART + SD load
- document whether pulse width is generated by hardware chaining or software scheduling

Record choice:

- trigger method:
- trigger output timing notes:

## 5.3 SYNC Capture

Checklist:

- confirm edge interrupts work cleanly on the chosen SYNC pins
- measure timestamp latency and variation from edge to logged event
- verify no polling is used anywhere in the final design

Record choice:

- SYNC capture method:

______________________________________________________________________

## 6. Memory Placement Checklist

Checklist:

- place ISR queues in internal RAM
- place UART circular-buffer metadata in internal RAM
- decide whether raw byte buffers live in internal RAM, PSRAM, or split placement
- benchmark PSRAM-backed staging buffers under worst-case SD and UART load
- verify no critical low-latency path depends on slow or bursty memory access

Record decisions:

- ISR queue memory:
- UART circular buffer memory:
- binary staging buffer memory:
- status buffer memory:

______________________________________________________________________

## 7. External Carrier / Breakout Checklist

If using the `ESP32-P4-NANO` with an external sensor board, verify:

- connector type and pin order for each sensor port
- per-port `3.3 V` power distribution
- per-port current limiting or load switch implementation
- UART and SYNC protection components
- clean ground return and shield strategy if cable runs are long
- whether the carrier passes through the onboard console path or exposes a separate service header

Record details:

- connector family:
- per-port power limit:
- protection approach:
- service header approach:

______________________________________________________________________

## 8. Final Sign-Off Checklist

Before implementation is locked, confirm all of the following:

- 4 sensor UARTs are mapped to real exposed pins
- 4 SYNC lines are mapped to real exposed pins
- chosen pins do not conflict with TF, Ethernet, USB, or boot-critical functions
- console/debug path remains available for bring-up
- onboard features not used by the logger are explicitly left out of scope
- SD logging path is benchmarked on the onboard TF slot
- memory placement has been tested, not assumed
- external carrier power limits are documented
- firmware pin map, schematic pin map, and connector pinout all match

______________________________________________________________________

## 9. Bring-Up Table

Use this table as a working summary during implementation.

| Function | SoC Peripheral | Board Pin / Header Pin | Direction | Onboard Conflict? | Verified? | Notes |
|---|---|---|---|---|---|---|
| Console UART TX | | | Out | | | |
| Console UART RX | | | In | | | |
| Sensor 1 UART RX | | | In | | | |
| Sensor 1 UART TX | | | Out | | | |
| Sensor 1 SYNC | | | In/Out | | | |
| Sensor 2 UART RX | | | In | | | |
| Sensor 2 UART TX | | | Out | | | |
| Sensor 2 SYNC | | | In/Out | | | |
| Sensor 3 UART RX | | | In | | | |
| Sensor 3 UART TX | | | Out | | | |
| Sensor 3 SYNC | | | In/Out | | | |
| Sensor 4 UART RX | | | In | | | |
| Sensor 4 UART TX | | | Out | | | |
| Sensor 4 SYNC | | | In/Out | | | |
| Session Button | | | In | | | |
| Status LED | | | Out | | | |
| Spare Debug GPIO | | | In/Out | | | |
