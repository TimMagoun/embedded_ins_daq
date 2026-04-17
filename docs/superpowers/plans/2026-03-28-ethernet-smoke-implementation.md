# Ethernet Smoke Implementation Plan

1. Add a host-tested `ethernet_smoke` config API and confirm the new test fails first.
1. Implement the ESP-IDF Ethernet smoke module and wire it into `app_main`.
1. Add the minimum sdkconfig defaults for internal EMAC RMII on ESP32-P4.
1. Add a host-side `pipeflush` helper for USB NIC setup and ping validation.
1. Build, flash, run monitor, and validate host-to-ESP ping over the direct link.
