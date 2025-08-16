#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

class SQLConnection {
public:
    virtual ~SQLConnection() = default;

    // Connect using a DSN / path (SQLite: filename; Postgres: conninfo).
    virtual void connect(const std::string& dsn) = 0;

    // Safe to call multiple times.
    virtual void disconnect() noexcept = 0;

    // DDL: returns true on success, false on failure.
    virtual bool execDDL(std::string sql) = 0;

    // DML (INSERT/UPDATE/DELETE/UPSERT):
    // returns affected rows; parameters are positional.
    virtual int execDML(std::string sql,
                        const std::vector<std::string>& params = {}) = 0;

    // SELECT: returns rows as JSON objects (colName -> value as string/JSON).
    virtual std::vector<nlohmann::json>
    get(std::string sql,
        const std::vector<std::string>& params = {}) = 0;
};

// Helpers for ownership
using PSQLConnection = std::unique_ptr<SQLConnection>;
