#include <stdexcept>
 #include <vector>
#include <sstream>
#include <string>
#include <cstring>
#include "storage.hpp"
#include "ulid.hpp"
#include "sqlconnection.hpp"
#include <nlohmann/json.hpp>
#include <ddl_visitor.hpp>
#include <dml_visitor.hpp>
#include <dbpool.hpp>
#include <orm.hpp>
#include <bootstrap.hpp>

// Forward declarations for connection factories
PSQLConnection make_sqlite_connection();
#if HAVE_POSTGRESQL
PSQLConnection make_postgres_connection();
#endif

using nlohmann::json;

namespace {
    const OrmField* find_pk_field(const OrmSchema& s) {
        for (const auto& f : s.fields) if (f.is_id) return &f;
        for (const auto& f : s.fields) if (f.name == "id") return &f;
        return nullptr;
    }

    // JSON -> string param (kept simple/portable)
    std::string to_param(const json& v) {
        if (v.is_string())  return v.get<std::string>();
        if (v.is_boolean()) return v.get<bool>() ? "true" : "false";
        if (v.is_number())  return v.dump();            // 123 / 1.23
        if (v.is_null())    return std::string{};       // treat as empty
        return v.dump();                                // object/array -> JSON text
    }

    // Insert/Upsert params: present fields in schema order
    std::vector<std::string> build_params_insert_like(const OrmSchema& s, const json& obj) {
        std::vector<std::string> out; out.reserve(s.fields.size());
        for (const auto& f : s.fields) {
            if (obj.contains(f.name)) out.push_back(to_param(obj.at(f.name)));
        }
        return out;
    }

    // Generate ID if client must provide one (non-serial kinds)
    std::string maybe_generate_id(IdKind kind) {
        switch (kind) {
            case IdKind::UUIDv7:
            case IdKind::HighLow:
            case IdKind::Snowflake:
                return ULID::generate(); // monotonic, sortable; OK until we add exact generators
            case IdKind::DBSerial:
            case IdKind::TBSerial:
            default:
                return {};
        }
    }

    // Select the first object (array -> first elem)
    const json& first_obj(const json& data) {
        if (data.is_array()) {
            if (data.empty()) throw std::runtime_error("insert: empty JSON array");
            if (!data.front().is_object()) throw std::runtime_error("insert: first array element is not an object");
            return data.front();
        }
        if (!data.is_object()) throw std::runtime_error("insert: JSON must be an object or array of objects");
        return data;
    }
}

Storage::Storage(const std::string &db_path, Dialect dialect): catalog(OrmSchemaMap())
{
    pool::AcquirePolicy pol;
    pol.acquire_timeout = std::chrono::milliseconds(1500);
    pol.max_lease_time  = std::chrono::milliseconds(0); // no auto-expire

    switch (dialect)
    {
    case Dialect::SQLite:
        ddlVisitor_ = std::make_unique<SqliteDDLVisitor>();
        dmlVisitor_ = std::make_unique<SqliteDMLVisitor>();
        // qryVisitor_ = std::make_unique<SqliteQRYVisitor>();
        // db_path should be the SQLite filename/URI
        // std::string db_path = "/path/to/sqlite.db";
        dbpool_ = std::make_shared<DbPool>(/*capacity*/1, db_path, make_sqlite_connection, pol);
        break;
    case Dialect::Postgres:
#if HAVE_POSTGRESQL
        ddlVisitor_ = std::make_unique<PgDDLVisitor>();
        dmlVisitor_ = std::make_unique<PgDMLVisitor>();
        // qryVisitor_ = std::make_unique<SqliteQRYVisitor>();
        // db_path should be a full PG DSN, e.g.:
        // "host=127.0.0.1 port=5432 dbname=ecm user=ecm password=ecm"
        // std::string db_path = "host=localhost port=5432 dbname=ecm user=ecm password=ecm"
        dbpool_ = std::make_shared<DbPool>(/*capacity*/10, db_path, make_postgres_connection, pol);
#else
        throw std::runtime_error("PostgreSQL support not built in");
#endif
        break;
    default:
        throw std::runtime_error("Unsupported dialect");
    }

    // Connect to database
    //dbpool_->connect(); perform the real connection to database
}

Storage::~Storage() = default;

//execute DDL without transaction
bool Storage::execDDL(std::string sql)
{
    pool::IDbPool::AcquireResult ac = dbpool_->acquire(pool::DbIntent::Write, std::chrono::milliseconds(1000));
    if (!ac.ok) return false;
    pool::Lease& lease = ac.lease; // when lease get out of scope - release connection
    SQLConnection& conn = lease.conn();
    auto stmt = conn.prepare(sql);
    return stmt->exec();
}

//execute DDL without transaction
int Storage::execDML(std::string sql, const std::vector<std::string>& params)
{
    pool::IDbPool::AcquireResult ac = dbpool_->acquire(pool::DbIntent::Write, std::chrono::milliseconds(1000));
    if (!ac.ok) return 0;
    pool::Lease& lease = ac.lease; // when lease get out of scope - release connection
    SQLConnection& conn = lease.conn();
    auto stmt = conn.prepare(sql);
    return stmt->exec();
}

bool Storage::addSchema(OrmSchema& schema, SQLConnection* conn /*=nullptr*/)
{
    if (schema.name.empty()) {
        return false; // invalid
    }

    // 1) Add to in-memory catalog if not exists
    if (catalog.find(schema.name) == catalog.end()) {
        // move the caller's instance into the catalog to avoid copying a non-copyable type
        auto key = schema.name;                // take key before move
        catalog.emplace(std::move(key), std::make_unique<OrmSchema>(std::move(schema)));
    }

    // 2) Persist to DB via Storage::insert() if a connection is provided
    if (!conn) return true; // in-memory done

    // Find meta-schemas for catalog and versions
    auto itCat = catalog.find("schema_catalog");
    auto itVer = catalog.find("schema_versions");
    if (itCat == catalog.end() || !itCat->second) return false;
    if (itVer == catalog.end() || !itVer->second) return false;

    OrmSchema& catSchema = *itCat->second; // table: schema_catalog
    OrmSchema& verSchema = *itVer->second; // table: schema_versions

    std::string track = "";

    // Build catalog row (unique by name); rely on DB defaults for timestamps/PK
    JSONData cat_job = {
        {"name", schema.name},
        {"version", schema.version},
    };

    // Insert/Upsert into schema_catalog
    int cat_rows = insert(*conn, catSchema, cat_job, track);
    if (cat_rows <= 0) return false;

    // If client- or app-generated PKs were used, our JSON may have received an ID;
    // DB-serial PKs won't appear here (we didn't SELECT/RETURNING).
    int schema_id = 0;
    if (cat_job.contains("id")) {
        // if your schema defines integer IDs
        try {
            schema_id = cat_job.at("id").get<int>();
        } catch (...) {
            // ignore; keep 0 if not numeric
        }
    }

    // Build versions row; if we don't have the numeric FK yet (serial PK case),
    // we still insert with schema_id=0 (or you can skip until you implement SELECT/RETURNING).
    JSONData ver_job = {
        {"schema",  schema_id},
        {"version", schema.version},
        {"applied", false},
        {"json",    schema.json},
    };

    int ver_rows = insert(*conn, verSchema, ver_job, track);
    return ver_rows > 0;
}


bool Storage::init_catalog()
{
    std::vector<std::string> schemas = {SCHEMA_CATALOG_JSON, SCHEMA_VERSIONS_JSON};
    // Acquire a write connection
    auto ac = dbpool_->acquire(pool::DbIntent::Write, std::chrono::milliseconds(2000));
    if (!ac.ok) return false;
    pool::Lease& lease = ac.lease;
    SQLConnection& conn = lease.conn();
    conn.begin(); // start transaction
    try{
        for (const auto& s : schemas) {
            OrmSchema schema = OrmSchema{};
            if (OrmSchema::from_json(json::parse(s), schema) ) {
                std::string ddl = ddlVisitor_->visit(schema);
                auto stmt = conn.prepare(ddl);
                if (stmt->exec() < 0) {
                    std::stringstream s; s << "DDL Failed: " << ddl;
                    throw std::runtime_error(s.str());
                }
            }
        }
        //after create the tables can add the records to both tables
        // and add schemas to catalog
        for (const auto& s : schemas) {
            OrmSchema schema = OrmSchema{};
            if (OrmSchema::from_json(json::parse(s), schema) ) {
                addSchema(schema, &conn);
            }
        }
        if (!conn.commit()) {
            conn.rollback();
            return false;
        }
        return true;
    } catch (...) {
        conn.rollback();
        return false;
    }
}

int Storage::insert(const std::string &schemaName, JSONData &data, const std::string &trackinfo)
{
    // 1 - find schema
    auto it = catalog.find(schemaName);
    if (it == catalog.end())
        throw std::runtime_error("Schema not found: " + schemaName);
    OrmSchema &schema = *(it->second);

    // 2 - Acquire a write connection
    auto ac = dbpool_->acquire(pool::DbIntent::Write, std::chrono::milliseconds(2000));
    if (!ac.ok) return false;
    pool::Lease& lease = ac.lease;
    SQLConnection& conn = lease.conn();

    try
    {
        // 3 - begin transaction (if not already started)
        conn.begin();

        // 4 - delegate to overloaded insert
        int rows = insert(conn, schema, data, trackinfo);

        // 5 - commit
        conn.commit();
        return rows;
    }
    catch (...)
    {
        conn.rollback();
        throw;
    }
}

int Storage::insert(SQLConnection &conn, OrmSchema &schema, JSONData &data, const std::string &trackinfo)
{
    int rowsAffected = 0;

    // 1 - build SQL via DMLVisitor
    bool isUpsert = false;
    if (data.is_object()) {
        if (data.contains("id") && !data["id"].is_null()) {
            int idval = data["id"].get<int>();
            if (idval > 0)
                isUpsert = true;
        }
    }
    else if (data.is_array() && !data.empty() && data[0].is_object()){
        auto &first = data[0];
        if (first.contains("id") && !first["id"].is_null()) {
            int idval = first["id"].get<int>();
            if (idval > 0)
                isUpsert = true;
        }
    }

    std::string sql = isUpsert ? dmlVisitor_->upsert(schema, data)
                               : dmlVisitor_->insert(schema, data);

    // 2 - begin (nested allowed; conn tracks state)
    conn.begin();
    try {
        // 3 - prepare
        auto stmt = conn.prepare(sql);

        auto processOne = [&](json &obj)
        {
            // 5.1 - generate ID if needed
            if ( !isUpsert ) {
                for (auto &fld : schema.fields) {
                    if (fld.is_id) {
                        // obj["id"] = fld.generateID();
                        obj["id"] = maybe_generate_id(fld.id_kind);
                        break;
                    }
                }
            }

            // 5.2 - bind params (schema fields that exist in obj)
            int paramIndex = 1; // one base index not zero based
            for (auto &fld : schema.fields) {
                if (obj.contains(fld.name)) {
                    stmt->bind(paramIndex++, obj[fld.name], fld.type);
                }
            }

            // execute
            stmt->exec();
            rowsAffected++;

            // 5.3 - track
            if (!trackinfo.empty()) {
                // TODO: insert audit record into Track table
            }
        };

        // 4/5 - dispatch on array vs object
        if (data.is_array()) {
            for (auto &obj : data) {
                if (obj.is_object())
                    processOne(obj);
            }
        } else if (data.is_object()) {
            processOne(data);
        }

        // 6 - commit
        conn.commit();

        // 7 - notify subscribers
        //notify(schema.name, isUpsert ? "UPSERT" : "INSERT");

        return rowsAffected;
    } catch (...) {
        conn.rollback();
        throw;
    }
}
//  int Storage::insert(const std::string &name, nlohmann::json &data, const bool tr, const std::string &trackinfo){
//     // 1 - find the OrmSchema
//     auto it = catalog.find(name);
//     if (it == catalog.end() || !it->second) {
//         throw std::runtime_error("insert: schema not found: " + name);
//     }
//     const OrmSchema& schema = *(it->second);
//     const OrmField*  pk     = find_pk_field(schema);


//     // Representative object defines the statement shape
//     const json& probePtr = first_obj(data);
//     if (!probePtr) {
//         // Empty array -> nothing to do
//         return 0;
//     }
//     const json& probe = probePtr;
//     const bool probe_has_pk = (pk && probe.contains(pk->name));

//     // 2 - create Insert/Upsert SQL (use UPSERT if PK present; else INSERT)
//     const std::string sql =
//         probe_has_pk ? dmlVisitor_->upsert(schema, probe)
//                      : dmlVisitor_->insert(schema, probe);

//     // 3 - acquire connection (+ optional transaction)
//     auto ac = dbpool_->acquire(pool::DbIntent::Write, std::chrono::milliseconds(2000));
//     if (!ac.ok) throw std::runtime_error("insert: could not acquire DB connection");
//     SQLConnection& conn = ac.lease.conn();

//     const bool local_tr = tr;
//     if (local_tr && !conn.execDDL("BEGIN;"))
//         throw std::runtime_error("insert: BEGIN failed");

//     try {
//         int total_affected = 0;

//         // 4 - prepare (execDML does prepare/exec internally)
//         auto process_one = [&](json& obj) {
//             // 6.1 - ensure ID if required by schema (non-serial kinds)
//             if (pk && !obj.contains(pk->name)) {
//                 if (pk->id_kind != IdKind::DBSerial && pk->id_kind != IdKind::TBSerial) {
//                     if (auto gid = maybe_generate_id(pk->id_kind); !gid.empty())
//                         obj[pk->name] = gid;
//                 }
//             }

//             // 6.2 - bind params (schema-order subset present in JSON) and insert/upsert
//             std::vector<std::string> params = build_params_insert_like(schema, obj);
//             int affected = conn.execDML(sql, params);
//             if (affected <= 0) throw std::runtime_error("insert: execDML affected 0 rows");
//             total_affected += affected;

//             // 6.3 - track/audit (optional)
//             if (!trackinfo.empty()) {
//                 // TODO: insert track/audit row (keep single source of truth for audit schema)
//             }
//         };

//         // 5/6 - iterate objects
//         if (data.is_array()) {
//             for (auto& el : data) {
//                 if (!el.is_object()) throw std::runtime_error("insert: array element is not an object");
//                 process_one(el);
//             }
//         } else {
//             process_one(data);
//         }

//         // 7 - commit (or rollback on error)
//         if (local_tr && !conn.execDDL("COMMIT;")) {
//             conn.execDDL("ROLLBACK;");
//             throw std::runtime_error("insert: COMMIT failed");
//         }

//         // 8 - notify subscribers (defer until notify() API exists)
//         // TODO: notify(name, "insert");

//         return total_affected;
//     } catch (...) {
//         if (local_tr) conn.execDDL("ROLLBACK;");
//         throw;
//     }
// }

void Storage::update(const std::string &table, const json &data,
                     const std::string & /*user*/, const std::string & /*context*/)
{
    if (!data.contains("id"))
        throw std::runtime_error("Update requires PK field 'id'");
    std::string id = data["id"];

    std::ostringstream set;
    std::vector<std::string> params;
    bool first = true;
    for (auto it = data.begin(); it != data.end(); ++it)
    {
        if (it.key() == "id")
            continue;
        if (!first)
            set << ", ";
        first = false;
        set << it.key() << " = ?";
        params.push_back(it.value().is_string() ? it.value().get<std::string>() : it.value().dump());
    }
    params.push_back(id); // WHERE id = ?

    std::string sql = "UPDATE " + table + " SET " + set.str() + " WHERE id = ?;";
    // conn_->execDML(sql, params);
}

void Storage::del(const std::string &table, const json &pk_data,
                  const std::string & /*user*/, const std::string & /*context*/)
{
    if (!pk_data.contains("id"))
        throw std::runtime_error("Delete requires PK field 'id'");
    std::string id = pk_data["id"];

    std::string sql = "DELETE FROM " + table + " WHERE id = ?;";
    // conn_->execDML(sql, {id});
}


