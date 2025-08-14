#include "storage.hpp"
#include "ulid.hpp"
#include "connection.hpp"
#include <stdexcept>
#include <sstream>
#include <nlohmann/json.hpp>
#include <visitor.hpp>
#include <orm.hpp>

// Forward declarations for connection factories
ConnectionPtr make_sqlite_connection();
#if HAVE_POSTGRESQL
ConnectionPtr make_postgres_connection();
#endif

using nlohmann::json;

Storage::Storage(const std::string& db_path, Dialect dialect) {
    // Choose visitor + connection by dialect
    switch (dialect) {
        case Dialect::SQLite:
            visitor_ = std::make_unique<SqliteDDLVisitor>();
            conn_ = make_sqlite_connection();
            break;
        case Dialect::Postgres:
#if HAVE_POSTGRESQL
            visitor_ = std::make_unique<PostgresDDLVisitor>();
            conn_ = make_postgres_connection();
#else
            throw std::runtime_error("PostgreSQL support not built in");
#endif
            break;
        default:
            throw std::runtime_error("Unsupported dialect");
    }

    // Connect to database
    conn_->connect(db_path);
}

Storage::~Storage() = default;

bool Storage::exec(std::string_view sql) {
    return conn_->execDDL(sql);
}

bool Storage::init_catalog() {
    static constexpr const char* kCatalogSchemaJson = R"JSON(
    {
      "name": "schema",
      "version": 1,
      "type": "object",
      "properties": {
        "name":        { "type": "string",  "nullable": false },
        "version":     { "type": "integer", "nullable": false, "default": 1 },
        "json_schema": { "type": "string",  "nullable": false }
      },
      "primaryKey": ["name"]
    }
    )JSON";

    OrmSchema schema;
    OrmSchema::from_json(json::parse(kCatalogSchemaJson), schema);
    std::string ddl = visitor_->visit(schema);

    // Ensure idempotency
    if (ddl.find("IF NOT EXISTS") == std::string::npos) {
        const char* needle = "CREATE TABLE";
        if (auto pos = ddl.find(needle); pos != std::string::npos) {
            ddl.insert(pos + std::strlen(needle), " IF NOT EXISTS");
        }
    }
    return conn_->execDDL(ddl);
}

std::string Storage::insert(const std::string& table, json& data,
                            const std::string& /*user*/, const std::string& /*context*/) {
    // Ensure PK 'id'
    if (!data.contains("id") || data["id"].is_null() ||
        (data["id"].is_string() && data["id"].get<std::string>().empty())) {
        data["id"] = ULID::generate();
    }
    std::string id = data["id"];

    // Build SQL and params
    std::ostringstream cols, vals;
    std::vector<std::string> params;
    bool first = true;
    for (auto it = data.begin(); it != data.end(); ++it) {
        if (!first) { cols << ", "; vals << ", "; }
        first = false;
        cols << it.key();
        vals << "?";
        params.push_back(it.value().is_string() ? it.value().get<std::string>() : it.value().dump());
    }

    std::string sql = "INSERT INTO " + table + " (" + cols.str() + ") VALUES (" + vals.str() + ");";
    conn_->execDML(sql, params);
    return id;
}

void Storage::update(const std::string& table, const json& data,
                     const std::string& /*user*/, const std::string& /*context*/) {
    if (!data.contains("id")) throw std::runtime_error("Update requires PK field 'id'");
    std::string id = data["id"];

    std::ostringstream set;
    std::vector<std::string> params;
    bool first = true;
    for (auto it = data.begin(); it != data.end(); ++it) {
        if (it.key() == "id") continue;
        if (!first) set << ", ";
        first = false;
        set << it.key() << " = ?";
        params.push_back(it.value().is_string() ? it.value().get<std::string>() : it.value().dump());
    }
    params.push_back(id); // WHERE id = ?

    std::string sql = "UPDATE " + table + " SET " + set.str() + " WHERE id = ?;";
    conn_->execDML(sql, params);
}

void Storage::delete_row(const std::string& table, const json& pk_data,
                         const std::string& /*user*/, const std::string& /*context*/) {
    if (!pk_data.contains("id")) throw std::runtime_error("Delete requires PK field 'id'");
    std::string id = pk_data["id"];

    std::string sql = "DELETE FROM " + table + " WHERE id = ?;";
    conn_->execDML(sql, { id });
}
