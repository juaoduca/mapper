Project: ECM / mapper

C++ ORM/DB toolkit using clean OOP + Visitor pattern to turn JSON Schema into SQL DDL/DML for PostgreSQL & SQLite.
CMake-based repo: include/, src/, tests/, data/ (example-schema.json), main.cpp demo, nginx_sample.conf, build.sh.
Depends on nlohmann-json. Part of ECM project to provide ORM FastCGI module with single source of truth for schema & migrations.

Current Focus

Bootstrap catalog + DML layer implementation

Progress

✅ Test suite compiles, CMake build OK.

✅ Catalog bootstrap sequence under construction.

✅ SQLConnection now owns transaction control:

begin(), commit(), rollback().

✅ Storage::addSchema() implemented:

Adds schema to in-memory catalog.

Calls Storage::insert() to add to DB if conn provided.

✅ Two versions of Storage::insert():

insert(schemaName, data, trackinfo) → acquires connection from pool, manages transaction.

insert(conn, schema, data, trackinfo) → uses acquired connection, generates DML via DMLVisitor, handles arrays/objects, IDs, audit, notify.

✅ SQLConnection::prepare() implemented in both SQLite and Postgres backends.

✅ SQLiteConnection::bind() implemented covering all OrmField data types.

⏳ PostgresConnection::bind() not yet implemented (PgStatement skeleton in progress).

Data Type Handling

JSON Schema → SQL types

SQLite: maps to TEXT, INTEGER, REAL, BOOLEAN, BLOB, DATE, TIME, TIMESTAMP, etc.

PostgreSQL: maps to TEXT, INTEGER, NUMERIC, BOOLEAN, JSON, DATE, TIME, TIMESTAMP, TIMESTAMP WITH TIME ZONE, BYTEA.

Bind() coverage:

SQLite: fully covered (string, int, number, bool, json, binary, date/time).

PostgreSQL: bind function still TODO.

Next Steps

Implement PgStatement + PgConnection::bind() mirroring SQLite pattern (using libpq).

Finish DMLVisitor for Insert / Upsert / Update / Delete (Postgres + SQLite).

Hook Storage::insert() into catalog bootstrap (init_catalog).

Extend tests for DML path (insert/upsert/update/delete).

Add JSON ↔️ object serialization with NLOHMANN_DEFINE_TYPE_INTRUSIVE/NON_INTRUSIVE.