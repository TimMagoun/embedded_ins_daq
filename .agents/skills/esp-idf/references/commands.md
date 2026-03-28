# ESP-IDF Command Reference

Load this file when exact command patterns are needed.

## Environment Checks

```bash
idf.py --version
IDF_PATH=/home/agent/esp/esp-idf idf.py --version
```

If flashing fails with `Permission denied` on `/dev/ttyACM*` or `/dev/ttyUSB*`, treat it as a host access problem first.

## Project Discovery

```bash
rg --files -g 'CMakeLists.txt' -g 'sdkconfig*' -g 'partitions.csv' -g 'idf_component.yml'
```

## Build

```bash
idf.py build
idf.py app
idf.py reconfigure
idf.py fullclean
idf.py fullclean build
idf.py set-target esp32
idf.py set-target esp32s3
idf.py set-target esp32c6
idf.py set-target esp32p4
```

## Flash And Monitor

List candidate serial ports:

```bash
ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
```

Common flows:

```bash
idf.py -p /dev/ttyUSB0 flash
idf.py -p /dev/ttyUSB0 monitor
idf.py -p /dev/ttyUSB0 flash monitor
idf.py -p /dev/ttyUSB0 app-flash monitor
idf.py -p /dev/ttyUSB0 -b 460800 flash
```

Useful monitor controls:

- `Ctrl+]`: exit monitor
- `Ctrl+T`, then `Ctrl+R`: reset target from monitor
- `idf.py monitor` needs a TTY; use a PTY-backed terminal session if non-interactive execution fails.

## Debug

Serial-first debugging:

```bash
idf.py -p /dev/ttyUSB0 monitor
```

Typical JTAG flow:

```bash
openocd -f board/<board-config>.cfg
idf.py gdb
```

If `idf.py gdb` is unavailable in the environment, use the toolchain GDB on the built ELF and connect to OpenOCD manually.

## Testing

```bash
idf.py test
```

Use only when the project includes ESP-IDF unit/integration test targets.

## Clean Rebuild After Configuration Drift

```bash
idf.py fullclean
idf.py set-target <chip>
idf.py build
```

## High-Risk Commands

Avoid these unless the user explicitly asks for them:

```bash
idf.py erase-flash
espefuse.py ...
parttool.py ...
```
