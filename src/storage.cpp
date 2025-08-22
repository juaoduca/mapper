#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstring>
#include "storage.hpp"
#include "sqlconnection.hpp"
#include "ulid.hpp"
#include "bootstrap.hpp"
#include "dbpool.hpp"
#include "ddl_visitor.hpp"
#include "dml_visitor.hpp"

// Forward declarations for connection factories
PSQLConnection make_sqlite_connection();
#if HAVE_POSTGRESQL
PSQLConnection make_postgres_connection();
#endif

namespace {

    const OrmProp* find_pk_field(const OrmSchema& s) {
        OrmProp p;
        for (const auto& it : s.fields) {
            auto p = it.second;
            if (p.is_id || p.name == "id")
                return &it.second;
        }
        return nullptr;
    }

    // JSON -> string param (kept simple/portable)
    std::string to_param(const jval& v) {
        if (v.IsString () ) return v.GetString() ;
        if (v.IsNumber () ) return v.GetString() ;
        if (v.IsInt    () ) return v.GetString() ;
        if (v.IsFloat  () ) return v.GetString() ;
        if (v.IsNull   () ) return std::string {NULL};
        if (v.IsBool   () ) return v.GetBool()? "true" : "false";
        return v.GetString(); // is_object() | .IsArray( -> JSON text
    }

    // // Select the first object (array -> first elem)
    // const jval& first_obj(const jval& value) {
    //     if (value.IsArray()) {
    //         if (value.Empty()) throw std::runtime_error("empty JSON array");
    //         auto& v = value.MemberBegin()->value; // json obj do iterator
    //         if (!v.IsObject()) throw std::runtime_error("array elements must be objects");
    //         return v;
    //     }
    //     if (!value.IsObject()) throw std::runtime_error("JSON must be an object or array of objects");
    //     return value;
    // }

    bool has_id(const jval& value) {
        if (value.HasMember("id")) {
            const jit& v = value.FindMember("id");
            if (!v->value.IsNull()) {
                if (v->value.IsString()) {
                    return !std::string(v->value.GetString()).empty();
                } else {
                    return v->value.GetInt() > 0;
                }
            }
        }
        return false;
    }
}

Storage::Storage(const std::string& db_path, Dialect dialect)
    : catalog_(OrmSchemaMap())
    , snowflake_(SnowflakeIdGenerator(21, 7)) {
    pool::AcquirePolicy pol;
    pol.acquire_timeout = std::chrono::milliseconds(1500);
    pol.max_lease_time = std::chrono::milliseconds(0); // no auto-expire

    switch (dialect) {
    case Dialect::SQLite:
        ddlVisitor_ = std::make_unique<SqliteDDLVisitor>();
        dmlVisitor_ = std::make_unique<SqliteDMLVisitor>();
        // qryVisitor_ = std::make_unique<SqliteQRYVisitor>();
        // db_path should be the SQLite filename/URI
        // std::string db_path = "/path/to/sqlite.db";
        dbpool_ = std::make_unique<DbPool>(/*capacity*/ 1, db_path, make_sqlite_connection, pol);
        break;
    case Dialect::Postgres:
#if HAVE_POSTGRESQL
        ddlVisitor_ = std::make_unique<PgDDLVisitor>();
        dmlVisitor_ = std::make_unique<PgDMLVisitor>();
        // qryVisitor_ = std::make_unique<SqliteQRYVisitor>();
        // db_path should be a full PG DSN, e.g.:
        // "host=127.0.0.1 port=5432 dbname=ecm user=ecm password=ecm"
        // std::string db_path = "host=localhost port=5432 dbname=ecm user=ecm password=ecm"
        dbpool_ = std::make_unique<DbPool>(/*capacity*/ 10, db_path, make_postgres_connection, pol);
#else
        throw std::runtime_error("PostgreSQL support not built in");
#endif
        break;
    default:
        throw std::runtime_error("Unsupported dialect");
    }

    // Connect to database
    // dbpool_->connect(); perform the real connection to database
    // create control tables eg.: sqlite_sequence
}

bool Storage::execDDL(std::string sql) {
    pool::IDbPool::AcquireResult ac = dbpool_->acquire(pool::DbIntent::Write, std::chrono::milliseconds(1000));
    if (!ac.ok) return false;
    pool::Lease& lease = ac.lease; // when lease get out of scope - release connection
    SQLConnection& conn = lease.conn();
    auto stmt = conn.prepare(sql);
    return stmt->exec();
}

int Storage::execDML(std::string sql, const std::vector<std::string>& params) {
    pool::IDbPool::AcquireResult ac = dbpool_->acquire(pool::DbIntent::Write, std::chrono::milliseconds(1000));
    if (!ac.ok) return 0;
    pool::Lease& lease = ac.lease; // when lease get out of scope - release connection
    SQLConnection& conn = lease.conn();
    auto stmt = conn.prepare(sql); // when stmt get out of scope - freemem
    return stmt->exec();
}

bool Storage::addSchema(OrmSchema& schema, SQLConnection* conn /*=nullptr*/) {
    if (schema.name.empty()) {
        return false; // invalid
    }

    // 1) Add to in-memory catalog_ if not exists - do not return
    if (catalog_.find(schema.name) == catalog_.end()) {
        // move the caller's instance into the catalog_ to avoid copying a non-copyable type
        // auto key = schema.name;                // take key before move
        // catalog_.emplace(std::move(key), std::make_unique<OrmSchema>(std::move(schema)));
        catalog_[schema.name] = std::make_shared<OrmSchema>(schema);
    }

    // 2) Persist to DB via Storage::insert()
    if (!conn) return true; // in-memory done

    // Find meta-schemas for catalog_ and versions
    auto itCat = catalog_.find("schema_catalog");
    auto itVer = catalog_.find("schema_versions");
    if (itCat == catalog_.end() || !itCat->second) return false;
    if (itVer == catalog_.end() || !itVer->second) return false;

    OrmSchema& catSchema = *itCat->second; // table: schema_catalog
    OrmSchema& verSchema = *itVer->second; // table: schema_versions

    std::string track = "";

    if (schema.id > 0) {
    }
    // Build catalog_ row (unique by name); rely on DB defaults for timestamps/PK
    // TODO: change to Facade constructor when ready
    std::string json_str = "{"
        "\"id\":" + std::to_string(schema.id) + ","
        "\"name\":\"" + schema.name + "\","
        "\"version\":" + std::to_string(schema.version) +
    "}";

    bool ok; std::string err;
    jdoc doc;
    doc.Parse(json_str.c_str());
    const jval& cat_job = doc;

    // TODO: change to Facade constructor when ready

    // Insert/Upsert into schema_catalog
    int cat_rows = insert(*conn, catSchema, doc, track);
    if (cat_rows <= 0) return false;

    // If client- or app-generated PKs were used, our JSON may have received an ID;
    // DB-serial PKs won't appear here (we didn't SELECT/RETURNING).
    int schema_id = 0;
    if (cat_job.HasMember("id")) {
        // if your schema defines integer IDs
        try {
            schema_id = cat_job.FindMember("id")->value.GetInt(); //.get<int>();
        } catch (...) {
            // ignore; keep 0 if not numeric
        }
    }

    // Build versions row; if we don't have the numeric FK yet (serial PK case),
    // we still insert with schema_id=0 (or you can skip until you implement SELECT/RETURNING).
    // TODO: change to Facade constructor when ready
     std::string json_str2 = "{"
        "\"schema\": " + std::to_string(schema.id) + ", " +
        "\"applied\": false, " +
        "\"version\": "+ std::to_string(schema.version) + ", "+
        "\"json\": " + schema.json + "}";

    bool ok2; std::string err2;
    jdoc doc2; doc2.Parse(json_str2.c_str());
    jval& ver_job = doc2;
    // TODO: change to Facade constructor when ready

    int ver_rows = insert(*conn, verSchema, ver_job, track);
    return ver_rows > 0;
}

bool Storage::init_catalog() {
    std::vector<std::string> schemas = { SCHEMA_CATALOG_JSON, SCHEMA_VERSIONS_JSON };
    // with_conn(
    //     pool::DbIntent::Write,
    //     [&](SQLConnection& conn) {
    //         conn.begin();
    //     }
    // );
    /*-----------Acquire a write connection------*/
    auto ac = dbpool_->acquire(pool::DbIntent::Write, std::chrono::milliseconds(2000));
    if (!ac.ok) return false;
    pool::Lease& lease = ac.lease;
    SQLConnection& conn = lease.conn();
    /*-----------Acquire a write connection------*/
    conn.begin(); // start transaction
    try {
        for (const auto& s : schemas) {
            OrmSchema schema = OrmSchema {};
            jdoc d; d.Parse(s.c_str());
            if (OrmSchema::from_json(d, schema)) {
                // create the table
                std::string ddl = ddlVisitor_->visit(schema);
                auto stmt = conn.prepare(ddl);
                if (stmt && stmt->exec() < 0) {
                    std::stringstream s;
                    s << "DDL Failed: " << ddl;
                    throw std::runtime_error(s.str());
                }
                // add to in-memory catalog_
                addSchema(schema, nullptr);
            }
        }
        // after create the tables and add schemas to catalog_
        // insert the schemas on DB
        for (const auto& s : schemas) {
            OrmSchema schema = OrmSchema {};
            jdoc dc; dc.Parse(s.c_str());
            if (OrmSchema::from_json(dc, schema)) {
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

int Storage::insert(const std::string& schemaName, jval& doc, const std::string& trackinfo) {
    // 1 - find schema
    auto it = catalog_.find(schemaName);
    if (it == catalog_.end())
        throw std::runtime_error("Schema not found: " + schemaName);
    OrmSchema& schema = *(it->second);
    // call insert withing a trx control - aouto release connection
    auto rowsaff = with_conn(pool::DbIntent::Write,
        [&](SQLConnection& conn) {
            try {
                // 3 - begin transaction (if not already started)
                conn.begin();

                // 4 - delegate to overloaded insert
                int rows = insert(conn, schema, doc, trackinfo);

                // 5 - commit
                if (!conn.commit()) {
                    conn.rollback();
                    throw std::runtime_error("commit fail! transaction rolled back");
                };
                return rows;
            } catch (...) {
                conn.rollback();
                throw;
            }
        }
    );
    return rowsaff ? *rowsaff: 0;
}

int Storage::insert(SQLConnection& conn, OrmSchema& schema, jval& data, const std::string& trackinfo) {

    const jval& job = jhlp::first_obj(data); // object or array; first_object semantics for DML
    const OrmProp& pkField = schema.idprop();

    // Build both SQLs from the first object shape; we will choose per row.
    // const std::pair<std::string, int> dmlVisitor_->insert(schema, job);
    const dml_pair sqlInsert = dmlVisitor_->insert(schema, job);
    const dml_pair sqlUpsert = dmlVisitor_->upsert(schema, job);

    // Prepare once; reuse per row.
    auto stmtInsert = conn.prepare(sqlInsert.first);
    auto stmtUpsert = conn.prepare(sqlUpsert.first);

    int rowsAffected = 0;

    // //called for each object
    // auto pk_is_present = [&](const jval& obj) -> bool {
    //     return obj.IsObject() && obj.HasMember(pkField.name.c_str());
    // };

    //called for each object
    auto pk_is_valid = [&](const jval& obj) -> bool {
        if (!has_id(obj)) return false;
        if (obj.HasMember(pkField.name.c_str()) ) {
            const jval& v = obj.FindMember(pkField.name.c_str())->value;
            // Minimal validity: non-null and non-zero/empty by type
            if (pkField.type == PropType::Integer || pkField.type == PropType::Number) {
                if (!v.IsNumber() && !v.IsInt() && !v.IsFloat()) return false;
                // treat 0 as invalid for generated ids
                return (v.IsFloat()) ? v.GetDouble() != 0.0 : v.GetInt()!= 0;
            } else { // strings (UUID, etc.)
                if (!v.IsString()) return false;
                return !std::string(v.GetString()).empty();
            }
        }
        return false;
    };

    //porcess foreach one object
    auto processOne = [&](const jval& obj) {
        // Decide per object
        const bool hasPk = has_id(obj);
        const bool validPk = pk_is_valid(obj);
        const bool isUpsert = hasPk && validPk;

        // Choose statement
        auto& stmt = isUpsert ? *stmtUpsert : *stmtInsert;

        // If INSERT path:
        //  - PK present but invalid: we may safely update the *existing* key -> preserves order
        //  - PK absent: generate PK but DO NOT add key to obj -> we will bind it *last*
        jdoc newid; // null by default
        newid.SetObject();
        if (!isUpsert) {
            create_id(const_cast<OrmProp&>(pkField), newid, "id");
        }

        // Bind in JSON key order for keys that exist in schema
        int paramIndex = 1;
        for (jit it = obj.MemberBegin(); it!=obj.MemberEnd(); it++) {
            auto fit = schema.fields.find(it->name.GetString());
            if (fit == schema.fields.end()) continue;
            const auto& fld = fit->second;
            if (fld.is_id) {
                if (isUpsert) { // tem PK e PK é Valida - usar PK do Objeto
                    stmt.bind(paramIndex++, it->value, fld.type);
                }  // !hasPK || (hasPK && PK_Invalida) // usar PK criada
            } else { //if isUpsert - hasID and PK_is_valid, use the obj ID
                stmt.bind(paramIndex++, it->value, fld.type);
            }
        }

        // if not upsert and not have pk - PK is the last
        if (!isUpsert) {
            stmt.bind(paramIndex++, newid, pkField.type);
        }

        // exec
        stmt.exec();
        rowsAffected++;

        // Tracking hook (no-op for now)
        if (!trackinfo.empty()) {
            // TODO: audit insert/upsert into Track table
        }
    };

    try {
        if (data.IsArray()) {
            for (jit it = data.MemberBegin(); it != data.MemberEnd(); it++) {
                if (it->value.IsObject()) processOne(it->value);
            }
        } else if (data.IsObject()) {
            processOne(data);
        }
        return rowsAffected;
    } catch (...) {
        throw; // caller controls transaction
    }
}

int Storage::update(const std::string& schemaName, jval& value, const std::string& trackinfo) {
    // 1 - find schema
    auto it = catalog_.find(schemaName);
    if (it == catalog_.end())
        throw std::runtime_error("Schema not found: " + schemaName);
    OrmSchema& schema = *(it->second);

    // call update withing a trx control - auto release connection
    auto rowsaff = with_conn(pool::DbIntent::Write,
        [&](SQLConnection& conn) {
            try {
                // 3 - begin transaction (if not already started)
                conn.begin();

                // 4 - delegate to overloaded insert
                int rows = update(conn, schema, value, trackinfo);

                // 5 - commit
                if (!conn.commit()) {
                    conn.rollback();
                    throw std::runtime_error("commit fail! transaction rolled back");
                };
                return rows;
            } catch (...) {
                conn.rollback();
                throw;
            }
        }
    );
    return rowsaff ? *rowsaff: 0;
}

int Storage::update(SQLConnection& conn, OrmSchema& schema, jval& value, const std::string& trackinfo) {

    // 1 - build SQL via DMLVisitor
    int rowsAffected = 0;
    const jval& obj = jhlp::first_obj(value);
    if (!has_id(obj)) throw std::runtime_error("object must have an ID");

    dml_pair sql = dmlVisitor_->update(schema, obj);

    // 2 - begin (nested allowed; conn tracks state)
    // conn.begin(); // do not begin TRX - controlled by the caller function
    try {
        // 3 - prepare - sql was build in the order of the json obj keys
        auto stmt = conn.prepare(sql.first);

        auto processOne = [&](const jval& val) {
            // 5.1 - check ID
            if (!has_id(val)) throw std::runtime_error("object must have an ID");

            // 5.2 - bind params (schema fields that exist in job)
            int paramIndex = 1; // one base index not zero based
            //foreach key in json object, find the OrmField and if exists - bind params
            OrmProp prop;
            OrmProp idprop;
            std::unordered_map<std::string, OrmProp>::iterator it;
            jval idvalue;

            for (jit it = val.MemberBegin(); it!= val.MemberEnd(); it++) {
                auto fld = schema.fields.find(it->name.GetString());
                if (fld != schema.fields.end()) {
                    prop = fld->second;
                    if (prop.is_id || prop.name == "id") { // the id is the last param => where id = 1234
                        idvalue = value;
                        idprop = prop;
                    } else {
                        stmt->bind(paramIndex++, value, prop.type);
                    }
                }
            }
            stmt->bind(paramIndex++, idvalue, idprop.type);

            // execute
            stmt->exec();
            rowsAffected++;

            // 5.3 - track
            if (!trackinfo.empty()) {
                // TODO: insert audit record into Track table
            }
        }; // process_one

        // 4/5 - dispatch on array vs object
        if (obj.IsArray()) {
            for (jit it = obj.MemberBegin(); it!=obj.MemberEnd(); it++) {
                if (it->value.IsObject())
                    processOne(it->value);
            }
        } else if (obj.IsObject()) {
            processOne(obj);
        }

        // 6 - do not commit or rollback - caller control the Transaction(trx)
        // if (!conn.commit()) {
        //     conn.rollback();
        //     throw std::runtime_error("commit fail - transaction rolled back");
        // }

        // 7 - notify subscribers
        // notify(schema.name, isUpsert ? "UPSERT" : "INSERT");

        return rowsAffected;
    } catch (...) {
        // conn.rollback();  // do not rollback - caller control the TRX
        throw;
    }
}



/**
 * @brief Generates a unique ID based on the specified kind. may need a SQLConnection to get ID from DB serial
 *
 * This function is a template that can return different ID types
 * (e.g., uint64_t for Snowflake, std::string for UUID) depending
 * on the template parameter.
 * @param kind The type of ID to generate.
 * @tparam T The desired return type (e.g., uint64_t, std::string).
 * @return The generated ID of type T.
 */

inline void Storage::create_id(OrmProp& prop, jdoc& doc, std::string key) {
    const char* ck = key.c_str();
    switch (prop.id_kind) {
        case IdKind::UUIDv7:{
            if (!doc.HasMember(ck)) { jhlp::set<std::string>(doc, key, ULID::get_id()); return ;}
            jval& val = doc.FindMember(ck)->value;
            if (!val.IsString())
                throw std::runtime_error("JSON Field must have a STRING datatype!");
            val.SetString(ULID::get_id().c_str(), doc.GetAllocator());
        }break;
        case IdKind::HighLow:
        case IdKind::Snowflake:{
            if (!doc.HasMember(ck)) { jhlp::set<int64_t>(doc, key, snowflake_.get_id()); return ;}
            jval& val = doc.FindMember(ck)->value;
            if (!val.IsInt64())
                throw std::runtime_error("JSON Field must have a NUMBER/INTEGER datatype!");
            val.SetInt64(snowflake_.get_id());
        }break;
        case IdKind::DBSerial:
        case IdKind::TBSerial:{
            if (!doc.HasMember(ck)) { jhlp::set<int64_t>(doc, key, snowflake_.get_id()); }
            jval& val = doc.FindMember(ck)->value;
            if (!val.IsInt64())
                throw std::runtime_error("JSON Field must have a NUMBER/INTEGER datatype!");

            //here we need the SQLconnection to get serial from DB
            auto rowsaff = with_conn(pool::DbIntent::Write,
                [&](SQLConnection& conn)->int {
                    int64_t id;
                    if(prop.id_kind == IdKind::TBSerial)
                        id = conn.nextValue(prop.schema_name);
                    else
                        id = conn.nextValue("db"); // or  conn.nextValue(prop.schema_name); for TBSerial
                    val.SetInt64(id);
                    return 1;
                }
            );
        }break;
        default:
            throw std::runtime_error("Unsupported ID kind.");
    }
}
// void Storage::del(const std::string &table, const json &pk_data,
//                   const std::string & /*user*/, const std::string & /*context*/)
// {
//     if (!pk_data.contains("id"))
//         throw std::runtime_error("Delete requires PK field 'id'");
//     std::string id = pk_data["id"];

//     std::string sql = "DELETE FROM " + table + " WHERE id = ?;";
//     // conn_->execDML(sql, {id});
// }
