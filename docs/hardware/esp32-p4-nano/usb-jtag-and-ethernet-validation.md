# ESP32-P4 Nano USB JTAG And Ethernet Validation

Date: 2026-03-28 UTC

Preserve these repo settings for this board:

- use `/dev/ttyACM1` for the built-in Espressif USB JTAG/serial path, not `/dev/ttyACM0`
- keep the ESP32-P4 pre-`v3` compatibility branch in [sdkconfig.defaults](/home/agent/workspace/embedded_ins_daq/sdkconfig.defaults):
  `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` and `CONFIG_ESP32P4_REV_MIN_100=y`

## Summary

This host sees both expected USB devices from the attached setup:

- `Bus 002 Device 004: ID 303a:1001 Espressif USB JTAG/serial debug unit`
- `Bus 003 Device 002: ID 0bda:8153 Realtek Semiconductor Corp. RTL8153 Gigabit Ethernet Adapter`

Validation result:

- USB JTAG: validated end-to-end
- USB serial on the same Espressif debug unit: device node present and opens successfully
- Ethernet adapter enumeration: validated on the host
- Ethernet direct-link pipeline: validated end-to-end with static IPv4 and `ping`

## What Was Verified

### 1. ESPressif USB JTAG/serial debug unit

Host enumeration:

```bash
lsusb
```

Observed device:

```text
Bus 002 Device 004: ID 303a:1001 Espressif USB JTAG/serial debug unit
```

Device node mapping:

```text
/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_80:F1:B2:D0:84:A4-if00 -> ../../ttyACM1
```

Serial access check:

- `/dev/ttyACM1` opened successfully from Python `pyserial`
- no bytes were observed during a short passive read window

JTAG probe check:

```bash
source ./tools/setup.sh
openocd -f board/esp32p4-builtin.cfg -c 'init; targets; shutdown'
```

Observed result:

- OpenOCD found the `esp_usb_jtag` device with serial `80:F1:B2:D0:84:A4`
- both JTAG TAPs were detected
- both ESP32-P4 high-performance cores were examined successfully
- OpenOCD reached the point of starting a GDB server on port `3333`

This is sufficient to say the USB JTAG path is working correctly.

### 2. Realtek RTL8153 Ethernet adapter and direct-link pipeline

Host enumeration:

```bash
lsusb
```

Observed device:

```text
Bus 003 Device 002: ID 0bda:8153 Realtek Semiconductor Corp. RTL8153 Gigabit Ethernet Adapter
```

Host network interface:

```text
enxa0cec8b57b54
```

Driver binding:

```text
Driver: r8152
```

Additional host-side status after setup:

```text
State: routable
Speed: 100Mbps
Duplex: full
```

Direct-link configuration used for validation:

- host USB NIC: `192.168.1.1/24`
- ESP32 Ethernet: `192.168.1.100/24`
- gateway configured on the ESP: `192.168.1.1`

Observed ESP-side boot result:

```text
ethernet_smoke: Ethernet link up
ethernet_smoke: Configured static IPv4: ip=192.168.1.100 netmask=255.255.255.0 gateway=192.168.1.1
runtime_banner: READY: ethernet_smoke
```

Observed host-side validation result:

```text
PING 192.168.1.100 (192.168.1.100)
3 packets transmitted, 3 received, 0% packet loss
```

This is sufficient to say the USB Ethernet path and the ESP Ethernet static-IP path are working correctly together.

## How To Use The Connection

### USB JTAG and serial

Load the repo environment first:

```bash
source ./tools/setup.sh
./tools/bootstrap_env.sh
```

Identify the serial device:

```bash
ls -l /dev/serial/by-id
```

Expected Espressif mapping in this setup:

```text
/dev/ttyACM1
```

Monitor serial output:

```bash
python3 -m tools.monitor --port /dev/ttyACM1 --timeout 10
```

If the firmware is the repo bring-up image and should emit a ready banner:

```bash
python3 -m tools.monitor \
  --port /dev/ttyACM1 \
  --ready-banner "READY: platform_smoke" \
  --timeout 20
```

Run a JTAG probe:

```bash
openocd -f board/esp32p4-builtin.cfg -c 'init; targets; shutdown'
```

Start a persistent JTAG server for GDB:

```bash
openocd -f board/esp32p4-builtin.cfg
```

Then connect with the ESP-IDF GDB flow in another shell after building the firmware.

### Ethernet

List the host adapter:

```bash
ip -br link | grep enxa0cec8b57b54
networkctl status enxa0cec8b57b54
```

Configure the direct-link host address and validate the path with these commands:

```bash
sudo ip addr replace 192.168.1.1/24 dev enxa0cec8b57b54
sudo ip link set dev enxa0cec8b57b54 up
ip -br link show enxa0cec8b57b54
ip -br addr show enxa0cec8b57b54
ethtool enxa0cec8b57b54
ping 192.168.1.100
```

What to look for:

- interface state should move to `UP`
- `ethtool` should report `Link detected: yes`
- `ip addr show enxa0cec8b57b54` should show `192.168.1.1/24`
- `ping 192.168.1.100` should succeed

## Recommended Next Validation

To fully validate Ethernet end-to-end on this hardware:

1. Flash the current firmware image.
1. Wait for `READY: ethernet_smoke` on `/dev/ttyACM1`.
1. Run the host address, link, and `ping` commands above for `enxa0cec8b57b54`.
1. Confirm successful ping replies from `192.168.1.100`.
