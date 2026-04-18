# Step 11: End-To-End Hardware Validation

**Objective:** Prove the full DAQ v1 prototype against the mock sensor rig and document verified behavior and remaining blind spots.

**Files:**

- Modify: `README.md`
- Create: `docs/hardware/esp32-p4-nano/daq-v1-test-rig.md`
- Create: `host_tests/test_end_to_end_contracts.cpp`

**Implementation Notes:**

- Document the rig wiring:
  - mock UART generator to `UART1`-`UART4`
  - trigger output pin
  - sync input pin(s)
  - logic analyzer probe points
  - SD card preparation
- Add a host-side contract test that verifies representative binary artifacts produced by the serialization path.
- Add or document an offline inspection tool/test helper that compares captured files against a known generator trace record-by-record.
- Capture a repeatable validation script for:
  - pre-start activity ignored
  - in-session UART coverage
  - trigger and sync record presence
  - post-stop activity ignored
  - SD removal fault
- Include one review item that explicitly checks every intended C ABI boundary is documented and limited to `extern "C"` seams only.
- Include one review item that verifies no `core/` file includes ESP-IDF or FreeRTOS headers.
- Include one review item that verifies every target-only dependency is isolated under `adapters/esp32/`.
- Include one review item that verifies adapters remain thin translation layers and do not absorb policy that belongs in `core/`.
- Include one review item that verifies hot adapters avoid per-byte virtual dispatch, repeated payload copies, and dynamic allocation.
- Include one review item that requires any future hot-path adapter abstraction change to include measurement notes or profiling evidence.

**Native Verification:**

- Run the full host suite:
  - Use the Host Validation Gate from [AGENT.md](/home/agent/workspace/embedded_ins_daq/AGENT.md#6-testing--quality-gates).
  - Use the full-host `ctest --test-dir build_host --output-on-failure` variant.
- Ensure host tests include a behavioral matrix covering:
  - all lifecycle states and illegal transitions
  - threshold boundaries
  - zero/empty/max inputs
  - repeated start/stop cycles
  - post-fault invariants
  - adapter batch-size invariance
  - deterministic multi-port interleaving
- Run repo quality gates:
  - `uv run pre-commit run --all-files`
  - Use the Host Validation Gate before the host-test portion of the gate.
  - `./tools/run_cppcheck.sh --strict`

**On-Device Hardware Verification:**

- Execute the full mock-rig campaign:
  - idle-gap chunking at low baud
  - sustained traffic up to `921600`
  - mixed four-port traffic
  - trigger pulse generation
  - rising and falling sync edges
  - start/stop boundary checks
  - SD card removal during `running`
- Extend the campaign with explicit edge/fault cases:
  - traffic arriving exactly at start boundary and stop boundary
  - active chunk present at stop
  - exact max-chunk boundary messages
  - exact block-boundary record packing patterns
  - simultaneous multi-port UART plus timing events
  - queue saturation debug builds
  - storage latency spike or throttled-card scenario if reproducible
- Expected result:
  - 100% UART message recovery in offline comparison against the known transmitter stream.
  - No pre-start or post-stop records in the file.
  - Trigger and sync edges decode correctly.
  - Timing measurements match the oscilloscope or logic analyzer within the accepted prototype budget.
  - All intentional storage faults force `faulted`.
  - Record counts, per-port byte counts, trigger counts, and sync-edge counts match the known generator truth data exactly.
  - The offline comparison tool reports zero mismatches for happy-path runs and expected deterministic failure for corrupted or faulted runs.
  - After a terminal fault, no additional valid records are appended to the file.

**Exit Criteria:**

- DAQ v1 is usable on hardware and its verified limits are explicitly documented.
