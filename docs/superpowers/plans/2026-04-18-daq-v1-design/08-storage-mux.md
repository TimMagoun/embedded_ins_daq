# Step 08: Storage Mux

**Objective:** Drain per-source queues, serialize records, and assemble fixed SD write blocks without touching the filesystem.

**Files:**

- Create: `main/core/storage_mux_core.hpp`
- Create: `main/core/storage_mux_core.cpp`
- Create: `main/include/storage_mux.hpp`
- Create: `main/storage_mux.cpp`
- Modify: `main/include/daq_types.hpp`
- Create: `host_tests/test_storage_mux_core.cpp`

**Implementation Notes:**

- Accept UART and timing-event record descriptors from all source queues.
- Serialize records with `record_format.cpp` into fixed-capacity write blocks.
- Preserve dequeue order; do not reorder records by timestamp.
- Flush partial blocks on stop-session.
- If the SD-writer queue cannot accept a block immediately, raise a fatal fault.
- Put dequeue ordering, block assembly, and partial-block flush logic in `storage_mux_core.cpp`.
- Keep `storage_mux.cpp` as a small wrapper that pulls from injected queue interfaces and forwards blocks to the writer-side interface.
- This step should remain ESP-IDF-free at the algorithm layer and have no `extern "C"` boundary.
- Keep the wrapper block-oriented. Adapters should forward fully assembled fixed blocks to the writer side rather than fragmenting them into smaller calls.
- Do not add adapter-side record reordering, extra serialization passes, or scratch-buffer copies.

**Native Verification:**

- Host tests for:
  - mixed UART and timing events serialize in dequeue order
  - block boundary handling across multiple records
  - partial block flush on stop
  - invalid record input faults
  - writer queue saturation faults
  - block output remains identical regardless of adapter queue-drain batch size
  - records that exactly fill a block
  - records that leave one byte free before the next record forces a new block
  - empty stop flush does not emit a spurious block
  - multi-port interleaving preserves dequeue order without timestamp reordering
  - post-fault no further blocks are emitted
- Host Validation Gate:
  - Use the Host Validation Gate from [AGENT.md](/home/agent/workspace/embedded_ins_daq/AGENT.md#6-testing--quality-gates).
  - Use `ctest --test-dir build_host --output-on-failure -R storage_mux_core` for the step-specific filter.

**On-Device Hardware Verification:**

- Run with synthetic sources first, then the real mock rig.
- Execute explicit scenarios:
  - UART-only traffic
  - timing-event-only traffic
  - mixed UART plus timing events
  - block-size boundary stress with message sizes chosen to straddle block edges
  - induced writer-queue saturation in a debug build
- Expected result:
  - Mixed-source records appear in the file in the same order the mux consumed them.
  - No malformed packets are seen at block boundaries.
  - Artificially shrinking writer-queue depth causes a deterministic fault.
  - No silent record loss occurs before the deliberate saturation point.

**Exit Criteria:**

- Record serialization and SD write preparation are correct before any filesystem code is layered in.
