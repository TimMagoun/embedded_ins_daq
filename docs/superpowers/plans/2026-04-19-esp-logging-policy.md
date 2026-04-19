# ESP Logging Policy Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make ESP-side runtime logging explicit and consistent while keeping host-test-visible code free of `ESP_LOGx`.

**Architecture:** Target-only entry points and ESP-IDF adapters own runtime logging. Shared `main/` code that is compiled by host tests remains log-free and reports status through return values or injected interfaces. Documentation will state the boundary clearly so future changes do not leak ESP logging into shared code.

**Tech Stack:** C++, ESP-IDF logging macros, Doxygen docs, host CMake build, `uv` tooling.

______________________________________________________________________

### Task 1: Update rules and design docs

**Files:**

- Modify: `AGENT.md`

- Modify: `RULES.md`

- Modify: `README.md`

- Modify: `docs/superpowers/plans/2026-04-18-daq-v1-design/README.md`

- [ ] **Step 1: Add the logging contract to the docs**

```text
Target-only code may use ESP_LOGx with a static TAG.
Shared code compiled by host tests must not include esp_log.h or call ESP_LOGx.
```

- [ ] **Step 2: Spell out log-level intent**

```text
E = faults and unrecoverable runtime failures
W = recoverable abnormal states or redundant lifecycle requests
I = boot, validation, lifecycle transitions, and peripheral bring-up
D = detailed trace for bring-up and postmortem debugging
V = very noisy traces that stay off by default
```

- [ ] **Step 3: Make the host-test boundary explicit**

```text
Shared code must report observability through interfaces, status events, or return values.
```

### Task 2: Add target-only runtime logging

**Files:**

- Modify: `main/app_main.cpp`

- Modify: `main/adapters/esp32/esp32_clock_adapter.cpp`

- [ ] **Step 1: Add boot and validation trace logging**

```cpp
ESP_LOGI(kTag, "DAQ boot");
ESP_LOGD(kTag, "Validating default config...");
ESP_LOGE(kTag, "Config validation failed: origin=%u detail=%u", ...);
ESP_LOGI(kTag, "Config validation succeeded");
```

- [ ] **Step 2: Add adapter lifecycle logging**

```cpp
if (timer_ != nullptr) {
  ESP_LOGW(kTag, "GPTimer initialize called twice; reusing existing timer");
  return ESP_OK;
}
ESP_LOGI(kTag, "GPTimer started at 1 MHz");
```

- [ ] **Step 3: Keep ISR and shared code unchanged**

```text
Do not add ESP logging to headers, host adapters, shared state machines, or ISR paths.
```

### Task 3: Verify target and host behavior

**Files:**

- Test: `build_host/`

- Test: `build/`

- [ ] **Step 1: Build host tests**

```bash
cmake -S host_tests -B build_host
cmake --build build_host
ctest --test-dir build_host --output-on-failure
```

- [ ] **Step 2: Build the ESP-IDF app**

```bash
source ./tools/setup.sh
idf.py build
```

- [ ] **Step 3: Confirm no ESP logging leaked into host-visible code**

```bash
rg -n "ESP_LOG|esp_log.h" main host_tests
```
