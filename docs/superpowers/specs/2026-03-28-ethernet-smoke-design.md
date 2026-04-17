# Ethernet Smoke Design

## Goal

Add a minimal Ethernet bring-up path for the ESP32-P4 Nano that:

- configures the onboard Ethernet MAC/PHY
- assigns static IP `192.168.1.100/24`
- expects the directly attached host USB NIC at `192.168.1.1/24`
- logs link and IP readiness
- supports host-side validation with a repeatable `pipeflush` helper

## Approach

Use a focused `ethernet_smoke` module called from `app_main` after the existing clock smoke.

The module owns:

- static Ethernet configuration constants
- Ethernet driver and `esp_netif` setup
- link and IPv4 event handling
- wait-until-ready logic for the smoke path

The host side uses a small helper tool to:

- configure the USB Ethernet NIC to `192.168.1.1/24`
- verify link state
- ping `192.168.1.100`

## Board Assumptions

Use the ESP-IDF ESP32-P4 internal EMAC/IP101 reference wiring:

- PHY: `IP101`
- MDC GPIO: `31`
- MDIO GPIO: `52`
- PHY reset GPIO: `51`
- PHY address: `1`

## Validation

- host test coverage for the static configuration helper
- `idf.py build`
- flash and monitor on hardware
- host `pipeflush` validation through the USB Ethernet adapter
