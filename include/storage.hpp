#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "orm.hpp"

// Storage: simplified for SQLite; adapt for Postgres if needed.
class Storage {
public:
    Storage(const std::string& db_path);
    ~Storage();

    // Insert: generates ULID if not in data; returns PK used.
    std::string insert(const std::string& table, nlohmann::json& data, const std::string& user = "", const std::string& context = "");

    // Update: requires PK in data; updates only provided fields.
    void update(const std::string& table, const nlohmann::json& data, const std::string& user = "", const std::string& context = "");

    // Delete: requires PK in pk_data (JSON: { "id": ... }).
    void delete_row(const std::string& table, const nlohmann::json& pk_data, const std::string& user = "", const std::string& context = "");

    // (Schema and cache: can be added here)
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
