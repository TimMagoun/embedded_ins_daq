# embedded_ins_daq

Native `ESP-IDF` Revision 1 firmware for the **Waveshare ESP32-P4-NANO** (`esp32p4`). Python tooling is managed via `uv`.

## 1. Environment Setup & Activation

**Shell Activation (Run in EVERY new shell):**

```bash
source ./tools/setup.sh
```

**Initial Workspace Setup (Run Once):**

```bash
# Install uv workspace
uv sync
# Install pre-commit hooks
uv run pre-commit install
# Install LLVM 22 for clangd
sudo bash -c "$(wget -O - https://apt.llvm.org/llvm.sh)" -- 22 all
# Set target to ESP32-P4 v1.3
idf.py set-target esp32p4
```

## 2. Standard Build & Device Workflow

Use standard ESP-IDF commands for local development:

- **Build:** `idf.py build`
- **Flash & Monitor:** `idf.py -p /dev/ttyACM0 flash monitor`
- **Generate combined compile commands:** `python3 tools/generate_compile_commands.py`
- **Run clangd diagnostics:** `uv run python3 tools/run_clangd_checks.py`

`tools/run_clangd_checks.py` expects `clangd` on `PATH` and uses the `clangd.arguments` array in `.vscode/settings.json` exactly as written.

*Note: `UART0` is strictly reserved for the console, boot logs, and panic output.*

## 3. Pre-Commit Quality Gates

Run these gates sequentially before creating a commit. Tests must verify interface contracts, boundary conditions, and fault-handling paths—line coverage alone is insufficient.

```bash
uv run pre-commit run --all-files
uv run python3 tools/check_host_coverage.py # Required if C/C++ code changed
uv run ./tools/run_cppcheck.py --strict # Required if C/C++ code changed
uv run python3 tools/run_clangd_checks.py # Required if C/C++ code changed
```

`host_tests/` fetches GoogleTest automatically during CMake configuration, so no separate system GTest install is required.
`tools/check_host_coverage.py` also writes an HTML report to `build_host_coverage/coverage/index.html` with the overall percentage and per-file coverage breakdown.
`tools/serve_host_coverage.py` runs a webserver that allows inspection of the HTML coverage report.

## 5. Documentation Rules

- **Public Headers (`main/include/`):** Document **interface contracts only**. Detail observable behavior, ownership, preconditions, error returns, and fault-reporting. Do not expose internal algorithms or state.
- **Implementation Files (`.c`):** Document the **mechanism**. Detail implementation invariants, edge-case handling, and rationale directly near the code block.
