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

### ESP-Side Logging Policy

- Use `ESP_LOGx` only in target-only ESP-IDF source files with a static `TAG`.
- Keep shared code compiled by host tests free of `esp_log.h` and `ESP_LOGx`; observability there should flow through return values, status events, or interfaces.
- Use `E` for faults, `W` for recoverable abnormal states, `I` for boot and lifecycle events, `D` for detailed runtime traces, and `V` only for very noisy traces.
- Never log from an ISR.
- Proactively log task high-water marks and heap availability at meaningful lifecycle points on the target.

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

- **Style:** Use **Doxygen-style comments only** for code documentation. Use `///` or `/** ... */` with tags such as `@brief`, `@param`, `@return`, and `@note` where they clarify the contract.
- **Public Headers (`main/include/`):** Document **interface contracts only**. Detail observable behavior, ownership, preconditions, error returns, and fault-reporting. Do not expose internal algorithms or state.
- **Implementation Files (`.c` / `.cpp`):** Document the **mechanism**. Detail implementation invariants, edge-case handling, and rationale directly near the code block.
- **Mandatory rule:** Documentation is required for every non-trivial function. Public functions must have Doxygen contract comments in the header. Internal functions must have Doxygen mechanism comments at the source definition site. Non-trivial functions may not be left undocumented.
