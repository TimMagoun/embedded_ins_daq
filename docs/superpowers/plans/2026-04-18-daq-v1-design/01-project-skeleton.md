# Step 01: Project Skeleton

**Objective:** Create the firmware and host-test directory skeleton so later steps can land in stable, predictable paths.

**Files:**

- Modify: `CMakeLists.txt`
- Create: `main/CMakeLists.txt`
- Create: `main/app_main.cpp`
- Create: `main/interfaces/`
- Create: `main/core/`
- Create: `main/adapters/esp32/`
- Create: `main/adapters/host/`
- Create: `main/include/daq_types.hpp`
- Create: `main/include/daq_faults.hpp`
- Create: `main/include/daq_status.hpp`
- Create: `main/include/c_linkage.h`
- Create: `host_tests/CMakeLists.txt`
- Create: `host_tests/test_project_skeleton.cpp`

**Implementation Notes:**

- Register a `main` component with `INCLUDE_DIRS "include"`.
- Add minimal shared enums and structs for state, fault origin, record type, and port identity.
- Establish repo-level directory rules immediately: `core/` for algorithms, `interfaces/` for contracts, `adapters/esp32/` for target bindings, and `adapters/host/` for test doubles and host implementations.
- Keep `app_main()` in `main/app_main.cpp` and declare it inside a narrow `extern "C"` scope because ESP-IDF expects C linkage for the entry point.
- Make host tests build independently from ESP-IDF by compiling C++ modules and headers without requiring target driver objects.
- Add one repository-wide linkage helper header pattern so later steps can include C HAL headers safely from C++.

**Native Verification:**

- Host Validation Gate:
  - Use the Host Validation Gate from [AGENT.md](/home/agent/workspace/embedded_ins_daq/AGENT.md#6-testing--quality-gates).
  - Use `ctest --test-dir build_host --output-on-failure -R project_skeleton` for the step-specific filter.
- Expected result:
  - Host test binary builds.
  - `project_skeleton` confirms shared C++ headers compile cleanly in host mode.
  - `app_main.cpp` exposes a valid `extern "C" void app_main(void)` symbol.

**On-Device Hardware Verification:**

- Build and flash:
  - `idf.py build`
  - `idf.py -p /dev/ttyACM0 flash monitor`
- Expected result:
  - Board boots on ESP32-P4 v1.3 settings.
  - Serial console shows a single DAQ boot banner.
  - No watchdog resets or component init failures.

**Exit Criteria:**

- The repo has a stable source tree.
- Both host and target builds succeed before any functional behavior is added.
