---
name: esp-idf
description: "Use for ESP-IDF embedded firmware work: setting up an ESP-IDF shell, inspecting or creating ESP-IDF projects, building firmware with idf.py, flashing boards, opening a serial monitor, running tests, cleaning build artifacts, changing targets, and debugging with OpenOCD or GDB. Trigger when the user mentions ESP32, ESP-IDF, idf.py, sdkconfig, flashing firmware, serial monitor logs, JTAG, or embedded microcontroller development workflows."
---

# ESP-IDF

Use this skill to drive day-to-day ESP-IDF development from the terminal with the least risky command sequence for the current project and board.

## Start Here

1. Confirm the project shape before editing or building.
2. Verify the ESP-IDF environment is loaded before running `idf.py`.
3. Prefer the narrowest command that advances the task: build, flash, monitor, test, or debug.
4. Read project-specific instructions in local repo files such as `AGENT.md`, `README.md`, `sdkconfig.defaults`, and board notes before changing configuration.

## Inspect The Project

- Check for `CMakeLists.txt`, `main/`, `components/`, `sdkconfig*`, `partitions.csv`, and `idf_component.yml`.
- Read any local agent instructions first; hardware revisions, flash settings, and test procedures are often project-specific.
- Infer the target from `sdkconfig`, `sdkconfig.defaults`, or prior build output before running `idf.py set-target`.
- Avoid changing `sdkconfig` unless the task requires configuration changes.

## Verify The Environment

- In this workspace, prefer the repo-local wrapper `scripts/idf`, which sources `/home/agent/esp/esp-idf/export.sh` and then runs `idf.py`.
- Use `scripts/idf --version` to confirm the tool is available before running build, flash, monitor, or test commands.
- If `idf.py` is missing, inspect whether the wrapper path or export script path is wrong before changing shell startup files.
- Check `echo $IDF_PATH` when the environment looks incomplete.
- Do not guess the ESP-IDF installation path if the repo already documents it; read local instructions first.
- On sandboxed hosts, `idf.py flash` and `idf.py monitor` may fail with permission errors on `/dev/ttyACM*` or `/dev/ttyUSB*`; separate host permission issues from firmware issues before changing code.

For common commands, load [references/commands.md](./references/commands.md).

## Build Workflow

- Use `scripts/idf build` for the normal edit-build loop in this workspace.
- Use `idf.py app` when only the application image is needed and the project supports it.
- Use `idf.py fullclean build` only when the build directory is stale, the target changed, or configuration drift is suspected.
- Use `idf.py reconfigure` after editing CMake or component metadata when a full clean is unnecessary.
- When the task changes chip family, run `idf.py set-target <chip>` before rebuilding.

## Flash And Monitor

- Identify the serial port explicitly when more than one device may be attached.
- Use `scripts/idf -p <port> flash monitor` when the user wants both programming and immediate logs.
- Use `idf.py -p <port> app-flash` if only the app image should be updated.
- Remember that `idf.py monitor` requires a real TTY; if plain pipe execution fails, rerun it in a PTY-backed terminal session.
- Exit the monitor cleanly instead of killing the process abruptly so the tty is released.
- Capture the first boot errors, panic output, and reset reason before changing code.

## Debug Workflow

- Distinguish serial-log debugging from JTAG debugging before choosing commands.
- For runtime issues, start with `idf.py monitor` and decode panics before escalating to JTAG.
- For breakpoint debugging, prefer the project’s documented OpenOCD and GDB flow.
- If the repo does not define one, use the standard ESP-IDF flow: start OpenOCD for the board, then launch the ELF with GDB through `idf.py gdb` or the matching Xtensa/RISC-V GDB binary.
- Confirm the `.elf` from the latest build matches the flashed image before debugging symbols.

## Testing And Validation

- Use `idf.py test` only if the project is set up for ESP-IDF unit tests.
- If hardware is required, state clearly what could not be verified in the sandbox.
- After config changes, rebuild and summarize the effective target, port, and command sequence used.

## Safety Rules

- Never erase flash or modify efuses unless the user explicitly requests it.
- Treat `menuconfig` as a deliberate config change; prefer file edits or existing defaults when the task is deterministic.
- Do not overwrite `sdkconfig.defaults` or board-specific settings without checking for hardware revision constraints.
- When flashing fails, separate port problems, boot mode problems, and image/configuration problems instead of retrying blindly.

## Troubleshooting Checklist

- `idf.py` missing: environment not exported or ESP-IDF not installed.
- Build breaks after target change: run `idf.py fullclean set-target <chip> build`.
- Serial port busy: close monitor sessions and identify competing tools.
- Flash timeout: verify USB permissions, cable quality, boot mode, and selected port.
- Boot log warns that detected flash is larger than the image header: fix `CONFIG_ESPTOOLPY_FLASHSIZE_*` in `sdkconfig` or `sdkconfig.defaults` before reflashing.
- Panic or reset loop: capture full monitor output before modifying firmware.
