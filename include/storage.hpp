#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "orm.hpp"
#include "visitor.hpp"
#include "sqlconnection.hpp"

// Storage: simplified for SQLite; adapt for Postgres if needed.
class Storage {
public:
    Storage(const std::string& db_path, Dialect dialect); // constructor
    ~Storage(); // destructor

    bool exec(std::string_view sql); // exec SQL direct to DB
    bool init_catalog(); // initializecatalog table to hold JSONSchemas for all tables in ORM

    // Insert: generates ULID if not in data; returns PK used.
    std::string insert(const std::string& table, nlohmann::json& data, const std::string& user = "", const std::string& context = "");

    // Update: requires PK in data; updates only provided fields.
    void update(const std::string& table, const nlohmann::json& data, const std::string& user = "", const std::string& context = "");

    // Delete: requires PK in pk_data (JSON: { "id": ... }).
    void delete_row(const std::string& table, const nlohmann::json& pk_data, const std::string& user = "", const std::string& context = "");

    // (Schema and cache: can be added here)
private:
    std::unique_ptr<OrmSchemaVisitor> visitor_; // holds the correct SQL generator
    std::unique_ptr<SQLConnection> conn_;
};
