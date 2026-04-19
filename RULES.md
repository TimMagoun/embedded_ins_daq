# Repository Rules

## Tests

- Write atomic tests that cover one behavior each.
- Name every test `Should<Postcondition>Given<Precondition>`.
- Pick names that tell the reader exactly what contract is being checked without opening the test body.
- Keep boundary and fault-path cases explicit in the name rather than hiding them behind generic words like `Works` or `Handles`.

## Host Tooling

- Never add tests when writing host tooling, this bloats the repository
- Host quality gates must work from the repository without requiring a separately installed GoogleTest package.
- All python scripts must be invoked through the repository virtual environment (`./.venv/bin/python` or `uv run python`).
- Shared code that is compiled by host tests must remain free of ESP-IDF logging calls. Keep `ESP_LOGx` and `esp_log.h` in target-only source files.
