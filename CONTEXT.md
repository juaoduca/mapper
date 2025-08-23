Project: ECM / mapper

C++ ORM/DB toolkit using clean OOP + Visitor pattern to turn JSON Schema into SQL DDL/DML for PostgreSQL & SQLite.
CMake-based repo: include/, src/, tests/, data/ main.cpp demo, build.sh.
Depends on RapidJSON. Part of ECM project to provide ORM FastCGI module with single source of truth for schema & migrations.

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

✅ PostgresConnection::bind() implemented covering all OrmField data types.

Data Type Handling

JSON Schema → SQL types

SQLite: maps to TEXT, INTEGER, REAL, BOOLEAN, BLOB, DATE, TIME, TIMESTAMP, etc.

PostgreSQL: maps to TEXT, INTEGER, NUMERIC, BOOLEAN, JSON, DATE, TIME, TIMESTAMP, TIMESTAMP WITH TIME ZONE, BYTEA.

Bind() coverage:

SQLite: fully covered (string, int, number, bool, json, binary, date/time).

PostgreSQL: fully covered (string, int, number, bool, json, binary, date/time).

Next Steps

- Finish DMLVisitor for Insert / Upsert / Update / Delete (Postgres + SQLite).

- Hook Storage::insert() into catalog bootstrap (init_catalog).

- implement Schema update model - instead of change a schema add new version

- Schema life cicle:
    - Only process schema on demand (in a CRUD operation)
    - Process schema means:
        - create/update DB table (cumulative version application)
        - mark the version as applied=true in DB and in-memory
        - in-memory only deliver the last applied version to client threads
        - mark old applied version as inactive
        - only loads schemas from DB that is >= the last applied version

- implement SchemaBoss and SchemaUpdater

- SchemaBoss:
    - handle the insert of schemas in system(DB memo)
    - handle the update (insert new versions)
    - handle the client threads requests for read access to a schema
        - delivers the last version applied
        - when a new version is applied
            - stop deliver old ones

- SchemaUpdater
    - trigger on CRUD op
    - Check the versions availables
    - if no version aplied
        - apply the last one
        else
        - if exist new versions greater then the last applied
            - for each next version registered
                - compare both for changes
                - apply the changes to DB
                - mark schema as applied
                - stop deliver the old applied
                - start deliver the new applied

Full ride test for schemas and data -:
    - create a schema string covering all fields types
    - register the schema (pass the string, create OrmSchema, register in DB && In-Memory)
    - create a data JSON String to insert in the schema table
    - parse this JSon string with RapidJSON
    - insert data to schema table
    - retrive data from schema table - test if match
    - create an update json string data
    - update data
    - retrive data updated - test if match
    - delete data from DB
    - check if deleted



Extend tests for DML path (insert/upsert/update/delete).