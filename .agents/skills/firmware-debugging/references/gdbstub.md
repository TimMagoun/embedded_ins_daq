# GDBStub Quick Reference

Use this only after the repo workflow has already captured logs and decoded any panic.

## Start

Preferred:

```bash
source ./tools/setup.sh
idf.py gdb
```

Direct:

```bash
source ./tools/setup.sh
riscv32-esp-elf-gdb build/embedded_ins_daq.elf
```

## First Commands

These are the first commands to try once the debugger is attached:

```gdb
bt
frame 0
info registers
info threads
thread apply all bt
list
```

## Breakpoints And Control

```gdb
break app_main
break <function_name>
continue
next
step
finish
```

## Variables And Memory

```gdb
print variable_name
print *pointer_name
x/16wx address
x/32bx address
watch variable_name
```

## Crash-Focused Checks

- `bt` confirms the current stack and lets you compare it with the decoded panic report.
- `info registers` shows the fault context.
- `info threads` and `thread apply all bt` help inspect other tasks when task support is available.
- `frame <n>` and `list` help inspect the exact source location around the fault.

## Rules

- Always debug with the ELF that matches the flashed image.
- Prefer `idf.py gdb` before invoking the toolchain `gdb` directly.
- Do not start with GDBStub if the panic decode already explains the fault.
- Do not switch to JTAG or OpenOCD in this repo unless the debugging strategy changes explicitly.
