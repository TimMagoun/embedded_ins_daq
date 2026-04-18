# embedded_ins_daq

Native `ESP-IDF` Revision 1 firmware for the **Waveshare ESP32-P4-NANO** (`esp32p4`). Python tooling is managed via `uv`.

## 1. Environment Setup & Activation

**Shell Activation (Run in EVERY new shell):**

```bash
source ./tools/setup.sh
```

**Initial Workspace Setup (Run Once):**

```bash
uv sync --group dev
uv run --group dev pre-commit install
idf.py set-target esp32p4  # Sets ESP32-P4 v1.3 compatibility defaults
```

## 2. Standard Build & Device Workflow

Use standard ESP-IDF commands for local development:

- **Build:** `idf.py build`
- **Flash & Monitor:** `idf.py -p /dev/ttyACM0 flash monitor`
- **Generate combined compile commands:** `python3 tools/generate_compile_commands.py`

*Note: `UART0` is strictly reserved for the console, boot logs, and panic output.*

## 3. Pre-Commit Quality Gates

Run these gates sequentially before creating a commit. Tests must verify interface contracts, boundary conditions, and fault-handling paths—line coverage alone is insufficient.

```bash
uv run --group dev pre-commit run --all-files
cmake -S host_tests -B build_host
cmake --build build_host
ctest --test-dir build_host --output-on-failure
uv run --group dev python3 tools/check_host_coverage.py
./tools/run_cppcheck.sh --strict # Required if C/C++ code changed
```

## 5. Documentation Rules

- **Public Headers (`main/include/`):** Document **interface contracts only**. Detail observable behavior, ownership, preconditions, error returns, and fault-reporting. Do not expose internal algorithms or state.
- **Implementation Files (`.c`):** Document the **mechanism**. Detail implementation invariants, edge-case handling, and rationale directly near the code block.
