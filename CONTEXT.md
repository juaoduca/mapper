# From repo root
echo '# Project Context – juaoduca/mapper

## Last Updated
2025-08-13

## Repo Sync
- Latest pointer: https://juaoduca.github.io/mapper/chat/latest.json
- Snapshot for a specific commit: https://juaoduca.github.io/mapper/chat/<SHA>/snapshot.json

## Current State
- Repo configured with “snapshot JSON” GitHub Action to sync latest commit.
- Unit tests all passing.

## Completed Work
1. **Schema name support**
   - Added `name` property to `OrmSchema`, loaded from `"name"` in JSON schema.
   - Updated Postgres and SQLite visitors to use `schema.name` in `CREATE TABLE` and index DDL.

2. **Typed defaults**
   - Added `DefaultKind` enum to `OrmField`:
     - `String` → SQL single-quoted with escaping (empty string → `DEFAULT ''`).
     - `Boolean` → `true` / `false` unquoted.
     - `Number` → numeric literal.
     - `Raw` → verbatim (e.g., `NULL`, JSON literal).
   - Updated `OrmSchema::from_json` to detect default type and set `default_kind` and `default_value`.
   - Updated `OrmSchemaVisitor::sql_default` to output correct SQL for all types.

3. **Tests**
   - Adjusted test schemas to include `"name": "users"`.
   - All tests pass after changes.

## Next Steps
- [ ] Decide on next feature or refactor.
- [ ] Keep `CONTEXT.md` updated after each block of work.

## Usage in New Chats
When starting a new chat, paste this file or give its snapshot URL so I can work from the latest state without loading old conversations.' > CONTEXT.md




## 2025-08-14 — Added tests for latest features
- Added `tests/test_defaults.cpp` to verify `sql_default` emission for String/Boolean/Number/Raw across Postgres and SQLite visitors.
- Added `tests/test_schema_name.cpp` to verify schema `name` is respected in `CREATE TABLE` for both visitors.
- Build locally requires `nlohmann_json` and Catch2 single-header already present in `tests/external/catch.hpp`.
