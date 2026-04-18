# Step 05: Binary Record Format

**Objective:** Define and verify the file header, UART record, timing-event record, and checksums used by the storage path.

**Files:**

- Create: `main/core/record_format_types.hpp`
- Create: `main/core/record_format_core.hpp`
- Create: `main/core/record_format_core.cpp`
- Create: `main/core/record_checksum_core.hpp`
- Create: `main/core/record_checksum_core.cpp`
- Create: `main/include/record_format.hpp`
- Create: `main/include/record_checksum.hpp`
- Create: `host_tests/test_record_format_core.cpp`
- Create: `host_tests/test_record_checksum_core.cpp`

**Implementation Notes:**

- Encode a fixed file header with:
  - magic
  - version
  - header length
  - session start timestamp
  - decoder-required config summary
  - checksum
- Encode two record schemas only:
  - UART data
  - timing event
- Keep serialization logic pure and byte-order explicit so host tests can assert exact binary output.
- Fail serialization on invalid input lengths or enum values instead of silently coercing them.
- Put immutable record/header definitions in `record_format_types.hpp`.
- Keep binary packing and checksum algorithms fully inside `core/` with no filesystem or ESP-IDF dependencies.
- Keep the public includes as thin forwarding headers if higher layers should not include `core/` paths directly.

**Native Verification:**

- Host tests for:
  - exact serialized header bytes
  - exact UART record bytes with payload
  - exact UART record bytes for zero-length payload if allowed, or explicit rejection if forbidden
  - exact UART record bytes at maximum supported payload length
  - exact timing event bytes for rising and falling sync edges
  - checksum generation and checksum mismatch detection
  - header length/version compatibility rules
  - invalid enum value rejection
  - truncated buffer rejection
  - header checksum failure and record checksum failure are distinguishable
  - round-trip decode helper tests from structured value -> bytes -> structured value for supported versions
- Host Validation Gate:
  - Use the Host Validation Gate from [AGENT.md](/home/agent/workspace/embedded_ins_daq/AGENT.md#6-testing--quality-gates).
  - Use `ctest --test-dir build_host --output-on-failure -R 'record_format|record_checksum'` for the step-specific filter.

**On-Device Hardware Verification:**

- Flash a debug build that writes one synthetic session file to SD on boot.
- Remove the card and inspect the file with `hexdump -C`.
- Decode the same file with a lightweight host-side inspection tool or test helper.
- Expected result:
  - Header fields match the configured values and session timestamp.
  - A synthetic UART record and timing event record decode byte-for-byte as expected.
  - Corrupting one byte in a copied test file causes checksum validation to fail in the inspection helper.

**Exit Criteria:**

- The binary contract is frozen before live data enters the storage path.
