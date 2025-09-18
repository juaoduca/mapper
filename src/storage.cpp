#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <memory>
#include "lib.hpp"
#include "ulid.hpp"
#include "dbpool.hpp"
#include "storage.hpp"
#include "bootstrap.hpp"
#include "ddl_visitor.hpp"
#include "dml_visitor.hpp"
#include "sqlconnection.hpp"
#include "schemacatalog.hpp"
#include "schemaupdate.hpp"

// Constructor
Storage::Storage(const std::string &db_path, Dialect dialect)
    : snowflake_(std::make_unique<SnowflakeIdGenerator>(25, 7)) {
    pool::AcquirePolicy pol = {
        .acquire_timeout = std::chrono::milliseconds(1500),
        .max_lease_time = std::chrono::milliseconds(0)
    };

    switch (dialect) {
        case Dialect::SQLite:
            ddlVisitor_ = std::make_shared<SqliteDDLVisitor>();
            dmlVisitor_ = std::make_shared<SqliteDMLVisitor>();
            // qryVisitor_ = std::make_unique<SqliteQRYVisitor>();
            // std::string db_path = "/path/to/sqlite.db";
            dbpool_ = std::make_unique<DbPool>(/*capacity*/ 1, db_path, make_sqlite_connection , pol);
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
                THROW("PostgreSQL support not built in");
            #endif
        break;
        default:
            THROW("Unsupported dialect");
    } // switch
    with_tr(pool::DbIntent::Write, [&](SQLConnection &conn) { conn.createDB(db_path); return 0; });
    catalog_ = std::make_shared<SchemaCatalog>();
    catalog_->insertCatalog = [this](const OrmSchema &schema) { return this->insertCatalog(schema); };
    catalog_->initSchema = [this](OrmSchema &schema) { return this->init_schema(schema); };
    catalog_->applyUpdates = [this](OrmSchema *old_, OrmSchema &new_) { return this->applyUpdates(old_, new_); };
}
//public
bool Storage::init_catalog() {
    GetRefSchemaFn fn = [&](const std::string &name) { return catalog_->get_schema(name); };
    //create the two DB tables if not exists
    OrmSchema schema_item;
    OrmSchema::from_json(SCHEMA_CATALOG_JSON, schema_item);
    schema_item.version = 1;
    catalog_->add(schema_item, false);// add to catalog NOT to table: so catalog_->get_schema() can work;

    OrmSchema schema_ver;
    OrmSchema::from_json(SCHEMA_VERSIONS_JSON, schema_ver, fn);
    schema_ver.version = 1; // the first version
    catalog_->add(schema_ver, false);

    auto resp = with_tr(pool::DbIntent::Write,
        [&](SQLConnection &conn) {
                createSchemaStorage(schema_item, conn);
                createSchemaStorage(schema_ver, conn);
                insertCatalog(schema_item, &conn);
                insertCatalog(schema_ver, &conn);
                return true;
            }
        );
    return resp ? true : false;
}
//private - create the schema table
bool Storage::createSchemaStorage(const OrmSchema &schema, SQLConnection &conn) {
    std::string ddl = ddlVisitor_->visit(schema);
    if (!conn.execDDL(ddl)) {THROW("[Storage::createSchemaStorage] - DDL Failed: %s", ddl.c_str());}
    schema.applied_at = lib::getDateTime();
    return true;
}
//private - create Schema_Item and Schema_versions record and insert into respective tables
bool Storage::insertCatalog(const OrmSchema &new_schema, SQLConnection *conn) {

    int applied_version = new_schema.applied ? new_schema.version : -1;
 // 1 - build the schema_catalog json data to insert
    jdoc jsch;
    jsch.SetObject();
    jdaloc as = jsch.GetAllocator();
    jsch.AddMember("id", (int64_t)0, as);
    jsch.AddMember("name", jval(new_schema.name.c_str(), as).Move(), as );
    jsch.AddMember("version", applied_version, as) ;
    jsch.AddMember("created_at", jval(lib::datetime().c_str(), as).Move(), as);// current time
    OrmSchema &cat_item = *catalog_->get_schema("schema_catalog");

// 1 - build the schema_catalog json data to insert
    jdoc jver;
    jver.SetObject();
    jdaloc av = jver.GetAllocator();
    jver.AddMember("id"        , (int64_t)0 , av              );
    jver.AddMember("schema"    , (int64_t)0 , av              );
    jver.AddMember("version"   , new_schema.version, av);
    jver.AddMember("applied"   , new_schema.applied, av);
    jver.AddMember("json"      , jval(new_schema.json.c_str(), av), av);
    jver.AddMember("applied_at", ""                     , av);
    OrmSchema &cat_ver = *catalog_->get_schema("schema_versions");

    auto insert_ = [&](SQLConnection &cn) {
            if (insert(cn, cat_item, jsch, "", "name") == 1) {
                cat_item.id = jsch["id"].GetInt64();
                jver["schema"].SetInt64(cat_item.id);
                if (insert(cn, cat_ver, jver, "", "schema, version") == 1) {
                    cat_ver.id = jver["id"].GetInt64();
                    return true;
                } else { THROW("Error on insert Catalog_Versions record for Schema: %s ", new_schema.name.c_str()); }
            } else { THROW("Error on insert Catalog_Item for Schema: %s ", new_schema.name.c_str()); }
            return false;
        };

    if (conn) { // if a conn was provided - call insert in the conn transaction
        return insert_(*conn) ;
    } else { // if not - start a new transaction
        auto resp = with_tr( pool::DbIntent::Write,
            [&](SQLConnection &conn) {
                return insert_(conn);
        });
        return resp ? *resp : false;
    }

    // auto resp = with_tr( pool::DbIntent::Write,
    //     [&](SQLConnection &conn) {
    //         if (insert(conn, cat_item, jsch, "") == 1) {
    //             cat_item.id = jsch["id"].GetInt64();
    //             jver["schema"].SetInt64(cat_item.id);
    //             if (insert(conn, cat_ver, jver, "") == 1) {
    //                 cat_ver.id = jver["id"].GetInt64();
    //                 return true;
    //             } else { THROW("Error on insert Catalog_Versions record for Schema: %s ", new_schema.name.c_str()); }
    //         } else { THROW("Error on insert Catalog_Item for Schema: %s ", new_schema.name.c_str()); }
    //         return false;
    //     });
    // return resp ? *resp : false;
}

bool Storage::applyUpdates(const OrmSchema *old_schema, const OrmSchema &new_schema) const {

    SchemaUpdate su = SchemaUpdate(old_schema, new_schema, ddlVisitor_ );

    std::vector<std::string> plan = su.plan_migration();

    with_tr(pool::DbIntent::Write,
        [&](SQLConnection &conn) {
            for (auto script : plan) {
                if (!conn.execDDL(script)) {
                    THROW("Migration plan for table: %s, fail on step: %s ", old_schema->name.c_str(), script.c_str());
                }
            }
            return true;
        });
    return false;
}

bool Storage::addSchema(std::string &JSONSchema) {
    OrmSchema sch;
    GetRefSchemaFn fn = [&](const std::string &name) { return catalog_->get_schema(name); };
    OrmSchema::from_json(JSONSchema, sch, fn);
    return addSchema(sch);
}

bool Storage::addSchema(OrmSchema &schema){
  return catalog_->add(schema);
}

bool Storage::getSchema(std::string &name, OrmSchema &schema) {
    schema = *catalog_->get_schema(name);
    return true;
}

bool Storage::init_schemas(const std::vector<std::string> &schema_names) {
    bool resp = false;
    for (const std::string &name: schema_names) {
        OrmSchema &sch = *catalog_->get_schema(name);
        resp = init_schema(sch);
    };
    return resp;
}

bool Storage::init_schema(const OrmSchema &schema) {
    const OrmSchema *parent_ptr = nullptr;
    // build the ladder of parents
    if (schema.ladder.empty()) {
        // starts with this
        parent_ptr = &schema;
        while (parent_ptr != nullptr) {
            schema.ladder.emplace(schema.ladder.begin(), *parent_ptr);
            parent_ptr = parent_ptr->parent.lock().get();
        }
        // reach the root
    }
    //
    with_conn(pool::DbIntent::Write,
        [&](SQLConnection &conn) {
            for (auto sch : schema.ladder) {
                createSchemaStorage(sch, conn);
            }
            return true;
        }
    );
    // search for referenced fields;

    for (auto fld_it : schema.fields) {
        if (auto ref = fld_it.second->ref_Schema.lock() ) {
            init_schema(*ref);
        }
    }
    return true;
}

bool Storage::select(std::string &sql, std::string &json_string) {
    auto r = with_conn(pool::DbIntent::Read,
        [&](SQLConnection &conn){
            char *res = nullptr;
            if ( conn.execGET(sql, &res) ) {
                if (res != nullptr) {
                    json_string = std::string(reinterpret_cast<const char *>(res));
                    return true;
                } else {
                    return false;
                }
            } else {
                return false;
            }
        }
    );
    return r ? *r : false;
}

int Storage::insert(const std::string &schemaName, jval &data, const std::string &trackinfo, const std::string &key) {
    // 1 - find schema
    auto schema = catalog_->get_schema(schemaName);
    // call insert withing a transaction control - auto release connection
    auto rowsaff = with_tr(pool::DbIntent::Write,
        [&](SQLConnection &conn) {
                int rows = insert(conn, *schema, data, trackinfo, key);
                return rows;
        });
    return rowsaff ? *rowsaff : 0;
}

int Storage::insert(SQLConnection &conn, const OrmSchema &schema, jval &data, const std::string &trackinfo, const std::string &key) {
    #define REF_PROP_ERROR "OrmProp::ref_Field is null: need a valid reference Schema: %s Prop: %s"
    #define REF_PROP_VALUE "JSON data object with invalid prop value for prop: %s schema: %s"
    jdoc doc; // empty doc wiil be freed when out of scope
    jdaloc &a = doc.GetAllocator();
    const jval &first_obj = jhlp::first_obj(data); // object or array; first_object semantics for DML
    const std::shared_ptr<OrmProp> idprop = schema.idprop();

    const DML_Result dmlInsert = dmlVisitor_->insert(schema, first_obj, key);

    auto stmtInsert = conn.prepare(dmlInsert.sql);

    int rowsAffected = 0;

    // process foreach one object
    auto processOne = [&](jval &job) {

        // Bind in JSON key order, for keys that exist in schema - leaving
        int paramIndex = 1;
        bool hasid = false;
        for (auto it = job.MemberBegin(); it != job.MemberEnd(); it++) {
            auto fit = schema.fields.find(it->name.GetString());
            if (fit == schema.fields.end()) continue;
            const auto &fld = fit->second;
            if (fld->type == PropType::Object) {
                std::shared_ptr<OrmProp> ref_prop = fld->ref_Field.lock();
                if (ref_prop == nullptr) {THROW(REF_PROP_ERROR, schema.name.c_str(), fld->name.c_str()); }
                if(it->value.IsObject()) {
                    jval &job = it->value.GetObject();
                    auto jit = job.FindMember(ref_prop->name.c_str());
                    if (jit == job.MemberEnd()) {THROW(REF_PROP_VALUE, fld->name.c_str(), schema.name.c_str()); }
                    if (jit->value.IsString()) {
                        stmtInsert->bind(paramIndex++, jit->value, PropType::String); //bind in order
                    } else {
                        stmtInsert->bind(paramIndex++, jit->value, PropType::Number); //bind in order
                    }
                } else { // it->value is a value
                    if(it->value.IsString()){
                        stmtInsert->bind(paramIndex++, it->value, PropType::String); //bind with the item value
                    } else {
                        stmtInsert->bind(paramIndex++, it->value, PropType::Number); //bind with the item value
                    }
                }
            } else if (fld->is_id) { // if ID is present valid or invalid
                hasid = true;
                if (!dmlInsert.id_valid) {
                    create_id(fld, it->value, a, &conn);
                }
                stmtInsert->bind(dmlInsert.id_index, it->value, idprop->type); //bind in order
                paramIndex++; //increment to jump the id position
            } else { // field is not PK nor object
                stmtInsert->bind(paramIndex++, it->value, fld->type);
            }
        }
        if (!hasid) {
            jval newid;
            create_id(idprop, newid, a, &conn);
            stmtInsert->bind(dmlInsert.id_index, newid, idprop->type);
        }
        // exec
        stmtInsert->exec();
        rowsAffected++;

        // Tracking hook (no-op for now)
        if (!trackinfo.empty()) {
            // TODO: audit insert/upsert into Track table
        }
    };

    try {
        if (data.IsArray()) {
            for (jval& obj : data.GetArray()) {
                if (obj.IsObject()) {
                    processOne(obj);
                }
            }
        } else if (data.IsObject()) {
            processOne(data);
        }
        return rowsAffected;
    } catch (...) {
        throw; // caller controls transaction
    }
}

int Storage::update(const std::string &schemaName, jval &value, const std::string &trackinfo) {
    // 1 - find schema
    auto schema = catalog_->get_schema(schemaName);

    // call update withing a trx control - auto release connection
    auto rowsaff = with_tr(pool::DbIntent::Write,
        [&](SQLConnection &conn) {
            int rows = update(conn, *schema, value, trackinfo);
            return rows;
        });
    return rowsaff ? *rowsaff : 0;
}

int Storage::update(SQLConnection &conn, OrmSchema &schema, jval &value, const std::string &trackinfo) {

    // 1 - build SQL via DMLVisitor
    int rowsAffected = 0;
    const jval &obj = jhlp::first_obj(value);

    if (!obj.HasMember(schema.idprop()->name.c_str())) {
        THROW("[Storage::update()] - object must have an ID");
    }

    dml_pair sql = dmlVisitor_->update(schema, obj);

    // 2 - begin (nested allowed; conn tracks state)
    // conn.begin(); // do not begin TRX - controlled by the caller function
    try {
        // 3 - prepare - sql was build in the order of the json obj keys
        auto stmt = conn.prepare(sql.first);

        auto processOne = [&](const jval &val_obj) {
            // 5.1 - check ID
        if (!val_obj.HasMember(schema.idprop()->name.c_str())) {
            THROW("[Storage::update()] - object must have an ID");
        }


            // 5.2 - bind params (schema fields that exist in json_object)
            int paramIndex = 1; // one base index not zero based
            // foreach key in json object, find the OrmField and if exists - bind params
            OrmProp prop;
            OrmProp idprop;
            std::unordered_map<std::string, OrmProp>::iterator it;
            const rapidjson::Value *idvalue = nullptr;

            for (auto it = val_obj.MemberBegin(); it != val_obj.MemberEnd(); it++) {
                auto fld = schema.fields.find(it->name.GetString());
                if (fld != schema.fields.end()) {
                    prop = *fld->second;
                    if (prop.is_id) { // the id is the last param => where id = 1234
                        idvalue = &it->value; // hold the value
                        idprop = prop;        // hold the id prop
                    } else {
                        stmt->bind(paramIndex++, it->value, prop.type);
                    }
                }
            }
            // bind last param = where id = ?$1
            stmt->bind(paramIndex++, *idvalue, idprop.type);

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
            for (auto it = obj.MemberBegin(); it != obj.MemberEnd(); it++) {
                if (it->value.IsObject())
                    processOne(it->value);
            }
        } else if (obj.IsObject()) {
            processOne(obj);
        }

        // 6 - do not commit or rollback - caller control the Transaction(trx)
        // if (!conn.commit()) {
        //     conn.rollback();
        //     THROW("commit fail - transaction rolled back");
        // }

        // 7 - notify subscribers
        // notify(schema.name, isUpsert ? "UPSERT" : "INSERT");

        return rowsAffected;
    } catch (...) {
        // conn.rollback();  // do not rollback - caller control the TRX
        throw;
    }
}

void Storage::create_id(const std::shared_ptr<OrmProp> idprop, jval &val, jdaloc &aloc, SQLConnection *conn) {
    std::string prtname = idprop->parent.expired() ? "" : idprop->parent.lock()->name;
    bool nullval = val.IsNull();
    switch (idprop->id_kind) {
        case IdKind::UUIDv7: {
            if (nullval) {
                val.SetString("");
            } else if (!val.IsString()) {
                THROW("JSON Field must have a NUMBER/INTEGER datatype!");
            }
            std::string v = val.GetString();
            if (v.empty()) { val.SetString(ULID::get_id().c_str(), aloc); }
        } break;
        case IdKind::HighLow: // need a config function to get the High value and a table to keep the low value
        case IdKind::Snowflake: {
            if (nullval) {
                val.SetInt64(0);
            } else if (!val.IsInt64()){
                THROW("JSON Field must have a NUMBER/INTEGER datatype!");
            }
            if (val.GetInt64() == 0) {
                val.SetInt64(snowflake_->get_id());
            }
        } break;
        case IdKind::DBSerial:
        case IdKind::TBSerial: {
            if (nullval) {
                val.SetInt64(0);
            } else if (!val.IsInt64()){
                THROW("JSON Field must have a NUMBER/INTEGER datatype!");
            }
            if (val.GetInt64() == 0) {
                if (conn) {
                    int64_t id;
                    if (idprop->id_kind == IdKind::TBSerial)
                        id = (*conn).nextValue(idprop->parent.lock()->name);
                    else
                        id = (*conn).nextValue("db"); // or  conn.nextValue(idprop->schema_name); for TBSerial
                    val.SetInt64(id);
                } else {
                    with_conn(pool::DbIntent::Write,
                        [&](SQLConnection &cn){
                            int64_t id;
                            if (idprop->id_kind == IdKind::TBSerial)
                                id = cn.nextValue(idprop->parent.lock()->name);
                            else
                                id = cn.nextValue("db"); // or  conn.nextValue(idprop->schema_name); for TBSerial
                            val.SetInt64(id);
                            return id;
                        }
                    );
                }
            }
        } break;
        default:
            THROW("Unsupported ID kind.");
    }// switch
} // function
