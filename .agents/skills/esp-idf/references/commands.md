# ESP-IDF Command Reference

Load this file when exact command patterns are needed.

## Environment Checks

```bash
scripts/idf --version
IDF_PATH=/home/agent/esp/esp-idf scripts/idf --version
```

If `idf.py` is not found, check the repo-local `scripts/idf` wrapper and the configured `IDF_EXPORT_SCRIPT` path before changing shell startup files.
If flashing fails with `Permission denied` on `/dev/ttyACM*` or `/dev/ttyUSB*`, treat it as a host access problem first.

## Project Discovery

```bash
rg --files -g 'CMakeLists.txt' -g 'sdkconfig*' -g 'partitions.csv' -g 'idf_component.yml'
```

## Build

```bash
scripts/idf build
scripts/idf app
scripts/idf reconfigure
scripts/idf fullclean
scripts/idf fullclean build
scripts/idf set-target esp32
scripts/idf set-target esp32s3
scripts/idf set-target esp32c6
scripts/idf set-target esp32p4
```

## Flash And Monitor

List candidate serial ports:

```bash
ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
```

Common flows:

```bash
scripts/idf -p /dev/ttyUSB0 flash
scripts/idf -p /dev/ttyUSB0 monitor
scripts/idf -p /dev/ttyUSB0 flash monitor
scripts/idf -p /dev/ttyUSB0 app-flash monitor
scripts/idf -p /dev/ttyUSB0 -b 460800 flash
```

Useful monitor controls:

- `Ctrl+]`: exit monitor
- `Ctrl+T`, then `Ctrl+R`: reset target from monitor
- `idf.py monitor` needs a TTY; use a PTY-backed terminal session if non-interactive execution fails.

## Debug

Serial-first debugging:

```bash
scripts/idf -p /dev/ttyUSB0 monitor
```

Typical JTAG flow:

```bash
openocd -f board/<board-config>.cfg
scripts/idf gdb
```

If `idf.py gdb` is unavailable in the environment, use the toolchain GDB on the built ELF and connect to OpenOCD manually.

## Testing

```bash
scripts/idf test
```

Use only when the project includes ESP-IDF unit/integration test targets.

## Clean Rebuild After Configuration Drift

```bash
scripts/idf fullclean
scripts/idf set-target <chip>
scripts/idf build
```

## High-Risk Commands

Avoid these unless the user explicitly asks for them:

```bash
idf.py erase-flash
espefuse.py ...
parttool.py ...
```
