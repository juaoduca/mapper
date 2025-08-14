# Project Context – juaoduca/mapper

## Last Updated
2025-08-14

## Repo Sync
- Latest pointer: https://juaoduca.github.io/mapper/chat/latest.json
- Snapshot for a specific commit: https://juaoduca.github.io/mapper/chat/<SHA>/snapshot.json

## Current State
- Repo configured with “snapshot JSON” GitHub Action to sync latest commit.
- Unit tests all passing.
- Storage layer refactored to support multiple SQL dialects via `Connection` abstraction.
- Catalog table (`schema`) initialization added to track JSONSchemas and versions.

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

4. **Catalog table for schema tracking**
   - Defined hardcoded JSONSchema for a table named `"schema"`, with fields:
     - `name` (PK, string, non-nullable)
     - `version` (integer, default 1, non-nullable)
     - `json_schema` (string, non-nullable)
   - Added `Storage::init_catalog()` to create the catalog table if it doesn’t exist using current dialect visitor.

5. **Connection abstraction**
   - Created `Connection` base class with pure virtual methods:
     - `connect(path)` / `disconnect()`
     - `execDDL(sql) -> bool`
     - `execDML(sql, params) -> int`
     - `get(sql, params) -> result` (future use)
   - Added `ConnectionSQLite` and placeholder `ConnectionPostgres` subclasses.
   - Moved DB execution logic in `Storage` to use `Connection` methods.

6. **Storage refactor**
   - `Storage` now:
     - Holds a single `OrmSchemaVisitor` instance for the chosen dialect (no repeated dialect checks).
     - Holds a `Connection` instance for SQL execution.
     - Constructor picks the correct visitor + connection based on `Dialect` enum.
   - Updated `insert`, `update`, and `delete_row` to delegate execution to `Connection`.
   - Preserved ULID auto-generation for `id` when inserting.

## Next Steps
- [ ] Implement PostgreSQL `ConnectionPostgres` using `libpq`.
- [ ] Implement `get()` in `Connection` for SELECT queries.
- [ ] Add caching and version checks when loading JSONSchemas from the catalog.
- [ ] Add unit tests for `init_catalog()` and `Storage` CRUD operations.

## Usage in New Chats
When starting a new chat, paste this file or give its snapshot URL so I can work from the latest state without loading old conversations.
