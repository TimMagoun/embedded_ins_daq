---
name: cmake-hygiene
description: "Use when editing CMakeLists.txt or build layout in this embedded repo. Enforces explicit source lists, narrow include paths, target-scoped settings, and clean component boundaries."
---

# CMake Hygiene

Use this skill whenever you add or modify `CMakeLists.txt`, component layout, or build-target structure in this repository.

## Goal

Keep the build explicit, modular, and easy to reason about in an embedded project.

## Rules

1. **List sources explicitly**
   - Put concrete files in `SRCS`.
   - Do not use `file(GLOB ...)` for production sources.
   - Do not add a whole folder as a source entry.

2. **Keep include paths narrow**
   - Add only the directories a target actually needs in `INCLUDE_DIRS` or `target_include_directories()`.
   - Never add the whole `main/` folder just to make includes work.
   - Prefer directories that match the code boundary: `include/`, `interfaces/`, `core/`, or a specific adapter subdirectory.

3. **Scope settings to the target**
   - Put compile options, definitions, and include paths on the specific library or executable target.
   - Avoid global build changes unless they are required by the toolchain bootstrap.

4. **Split by responsibility**
   - If a folder starts mixing shared logic, hardware adapters, and tests, split it into separate subdirectories with their own `CMakeLists.txt`.
   - Keep host test CMake separate from firmware component CMake.

5. **Favor small components**
   - Create a new component only when it has a clear boundary and its own ownership.
   - Do not create a catch-all component that collects unrelated files.

6. **Preserve clarity**
   - Keep the top-level project `CMakeLists.txt` minimal.
   - Keep component `CMakeLists.txt` files readable enough to show the structure at a glance.

## Anti-patterns to avoid

- `INCLUDE_DIRS "."`
- `INCLUDE_DIRS "main"`
- `SRCS main/*.cpp`
- `file(GLOB ...)` for source discovery
- Global warning or include settings that should belong to one component
- One large component that mixes adapters, core logic, and test helpers

## Preferred structure

- Top-level `CMakeLists.txt`: project bootstrap only
- `main/CMakeLists.txt`: firmware component registration
- Subdirectories: separate `core/`, `interfaces/`, and `adapters/` when the codebase needs them
- `host_tests/CMakeLists.txt`: host-only test build logic

## When in doubt

Choose the smaller, more explicit target layout. If a CMake change would make the graph easier to understand only because it is broader, it is usually the wrong change.
