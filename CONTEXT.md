# CONTEXT.md

## Project Overview
**Repo:** `mapper` (part of ECM project)
**Goal:** C++ ORM/DB toolkit using clean OOP + Visitor pattern to transform JSON Schema into SQL DDL for PostgreSQL/SQLite.
Eventually will serve as a FastCGI backend module with schema/migrations as the single source of truth.

## Current Structure
- **include/**
  - `orm.hpp` → core ORM structures (`OrmField`, `OrmSchema`, enums for `IdKind`, `DefaultKind`, `Dialect`, etc.)
  - `visitor.hpp` → base visitor pattern for generating dialect-specific DDL
- **src/** → implementations for schema handling, visitors, connections
- **tests/**
  - `test_dbpool.cpp`
  - `test_ddl.cpp`
  - `test_defaults.cpp`
  - `test_schema_name.cpp`
  - `test_schemamanager.cpp`
  - `external/` → vendored `catch.hpp` (Catch2 single-header test framework)
- **main.cpp** → demo entry point
- **CMakeLists.txt** (root) → builds main binary + tests
- **third_party/nlohmann/json.hpp** (fallback vendored JSON header)
- **build.sh** helper
- **nginx_sample.conf** example config
- **data/** example-schema.json

## Build System (CMake)
- Requires **C++17**
- Uses **nlohmann_json** (find or vendored header)
- Detects **PostgreSQL** (optional, builds without if not found)
- Uses **SQLite3** via `pkg-config`

### Targets
- **Main executable**:
  `orm` → `main.cpp` + all `src/*.cpp`
- **Tests**:
  Each `tests/test_*.cpp` file is built into its own executable and registered with CTest.
  Example executables:
  - `test_dbpool`
  - `test_ddl`
  - `test_defaults`
  - `test_schema_name`
  - `test_schemamanager`

CTest integrates them all:
```bash
ctest --output-on-failure
```

### Status
- ✅ All 5 tests build and pass:
  ```
  100% tests passed, 0 tests failed out of 5
  ```

## Key Points from Last Iteration
- Moved from **single `map_tests` runner** to **per-file test executables**.
- Parallel build occasionally “hangs” around 88% due to linker load; resolved by retrying or reducing `-j`.
- Verified that `ctest -N` lists all 5 test executables correctly.
- Current repo is **stable and green**.

## Next Task
- **Bootstrap Catalog**: begin implementing schema catalog bootstrap logic, ensuring the ORM can manage schema migrations in a controlled way.
