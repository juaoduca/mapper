#include "storage.hpp"
#include "ulid.hpp"
#include "sqlconnection.hpp"
#include <stdexcept>
#include <sstream>
#include <nlohmann/json.hpp>
#include <visitor.hpp>
#include <dbpool.hpp>
#include <orm.hpp>
#include <bootstrap.hpp>

// Forward declarations for connection factories
PSQLConnection make_sqlite_connection();
#if HAVE_POSTGRESQL
PSQLConnection make_postgres_connection();
#endif

using nlohmann::json;

Storage::Storage(const std::string &db_path, Dialect dialect): catalog(OrmSchemaMap())
{
    // Choose visitor + connection by dialect
    switch (dialect)
    {
    case Dialect::SQLite:
        visitor_ = std::make_unique<SqliteDDLVisitor>();
        //dbpool_ = create a DBPool implementation for SQlite
        break;
    case Dialect::Postgres:
#if HAVE_POSTGRESQL
        visitor_ = std::make_unique<PgDDLVisitor>();
        //dbpool_ = create a DBPool implementation for Postgres with N connections
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
    pool::DbPool::AcquireResult ac = dbpool_->acquire(pool::DbIntent::Read, std::chrono::milliseconds(1000));
    if (!ac.ok) return false;
    pool::Lease& lease = ac.lease; // when lease get out of scope - release connection
    SQLConnection& conn = lease.conn();
    return conn.execDDL(sql);
}


int Storage::execDML(std::string sql, const std::vector<std::string>& params)
{
    pool::DbPool::AcquireResult ac = dbpool_->acquire(pool::DbIntent::Read, std::chrono::milliseconds(1000));
    if (!ac.ok) return 0;
    pool::Lease& lease = ac.lease; // when lease get out of scope - release connection
    SQLConnection& conn = lease.conn();
    return conn.execDML(sql, params);
}

int Storage::addSchema(OrmSchema& schema, bool apply) {
    if (schema.name.empty()) {
        return -1; // invalid
    }
    // later: insert into schema_catalog + schema_versions
    return 0;
}

bool Storage::init_catalog()
{
    std::vector<std::string> schemas = {SCHEMA_CATALOG_JSON, SCHEMA_VERSIONS_JSON};
    OrmSchema schema = OrmSchema();
    //retrieve a SQLConnection from the pool
    //start a transaction
    for (int i=0; i<2; i++) {
        if (OrmSchema::from_json(json::parse(schemas[i]), schema) ) {
            std::string ddl = visitor_->visit(static_cast<const void*>(&schema));
            if (ddl.find("IF NOT EXISTS") == std::string::npos)
            {
                const char *needle = "CREATE TABLE";
                if (auto pos = ddl.find(needle); pos != std::string::npos)
                {
                    ddl.insert(pos + std::strlen(needle), " IF NOT EXISTS");
                }
            }
            if ( /* execute DDL success */ true) {
                addSchema(schema, false);
            }
        }
    }
    //commit transaction or rollback if error - return false
    //release SQLConnection
    return true;
}

std::string Storage::insert(const std::string &schema, nlohmann::json &data, const std::string &user, const std::string &context)
{
    // Ensure PK 'id'
    if (!data.contains("id") || data["id"].is_null() ||
        (data["id"].is_string() && data["id"].get<std::string>().empty()))
    {
        data["id"] = ULID::generate();
    }
    std::string id = data["id"];

    // Build SQL and params
    std::ostringstream cols, vals;
    std::vector<std::string> params;
    bool first = true;
    for (auto it = data.begin(); it != data.end(); ++it)
    {
        if (!first)
        {
            cols << ", ";
            vals << ", ";
        }
        first = false;
        cols << it.key();
        vals << "?";
        params.push_back(it.value().is_string() ? it.value().get<std::string>() : it.value().dump());
    }

    std::string sql = "INSERT INTO " + schema + " (" + cols.str() + ") VALUES (" + vals.str() + ");";
    execDML(sql, params);
    return id;
}

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


