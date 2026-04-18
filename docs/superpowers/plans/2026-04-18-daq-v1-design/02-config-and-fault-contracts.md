# Step 02: Config And Fault Contracts

**Objective:** Define compile-time configuration, validation rules, and centralized fault/status contracts.

**Files:**

- Create: `main/include/daq_config.hpp`
- Create: `main/include/validate_config.hpp`
- Create: `main/interfaces/status_sink_interface.hpp`
- Create: `main/validate_config.cpp`
- Create: `main/status_fault_hub.cpp`
- Modify: `main/include/daq_faults.hpp`
- Modify: `main/include/daq_status.hpp`
- Create: `host_tests/test_validate_config.cpp`

**Implementation Notes:**

- Put all v1 compile-time limits in `daq_config.hpp`: enabled UART count, chunk sizes, idle-gap thresholds, queue capacities, SD block size, and trigger/sync enable masks.
- Implement `validate_config()` as a pure function returning either `DAQ_CONFIG_OK` or a specific fault code.
- Define a lightweight status/fault sink API that can accept reports from non-timing-critical code now and queue-backed transport later.
- Keep validation independent from ESP-IDF driver handles so it is fully host-testable.
- Keep this layer pure C++ with no `extern "C"` surface; it should depend on typed enums and constexpr configuration only.
- Put shared status/fault value types in headers and keep all validation algorithms free of runtime transport details.

**Native Verification:**

- Add host tests for:
  - valid configuration
  - too many UARTs
  - zero enabled UARTs if unsupported by v1 policy
  - chunk size larger than fixed buffer
  - chunk size exactly at fixed-buffer limit
  - idle-gap threshold exactly at accepted minimum
  - idle-gap threshold exactly at accepted maximum
  - zero or nonsensical queue capacities
  - idle-gap threshold outside accepted range
  - inconsistent cross-field combinations such as enabled sync lines without enabled ports
  - status/fault value objects preserve the precise first failure reason
- Commands:
  - `cmake -S host_tests -B build_host`
  - `cmake --build build_host`
  - `ctest --test-dir build_host --output-on-failure -R validate_config`

**On-Device Hardware Verification:**

- Flash a valid build and confirm the boot log reports config validation success and transitions toward `ready`.
- Temporarily set one invalid constant in `daq_config.hpp`, rebuild, and reflash.
- Repeat with at least three representative invalid configurations:
  - invalid UART count
  - invalid chunk size
  - invalid queue capacity
- Expected result:
  - Firmware enters `faulted` immediately on boot.
  - Console prints the precise validation fault code for each bad build.
  - No partial subsystem initialization occurs after config failure.
  - Reverting the bad constant restores normal boot.

**Exit Criteria:**

- All compile-time guardrails exist before runtime state or capture logic is introduced.
