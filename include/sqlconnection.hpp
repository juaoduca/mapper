#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

class SQLStatement {
public:
    virtual ~SQLStatement() = default;
    virtual void bind(int idx, const nlohmann::json& value, const std::string& type) = 0;
    virtual int exec() = 0;  // return rows affected
};

class SQLConnection {
public:
    virtual ~SQLConnection() = default;

    // Connect using a DSN / path (SQLite: filename; Postgres: conninfo).
    virtual void connect(const std::string& dsn) = 0;

    // Safe to call multiple times.
    virtual void disconnect()  = 0;

    virtual std::unique_ptr<SQLStatement> prepare(const std::string& sql) = 0;

    virtual bool begin() = 0;
    virtual bool commit() = 0;
    virtual void rollback() = 0;

protected:
    bool tr_started_;
};

// Helpers for ownership
using PSQLConnection = std::unique_ptr<SQLConnection>;
