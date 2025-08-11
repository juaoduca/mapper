#include "storage.hpp"
#include "ulid.hpp"
#include <sqlite3.h>
#include <stdexcept>
#include <sstream>
#include <iostream>

// --- Pimpl idiom for minimal headers ---
struct Storage::Impl {
    sqlite3* db = nullptr;

    Impl(const std::string& path) {
        if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
            throw std::runtime_error("Failed to open SQLite DB: " + path);
        }
    }
    ~Impl() {
        if (db) sqlite3_close(db);
    }
};

Storage::Storage(const std::string& db_path)
    : impl_(std::make_unique<Impl>(db_path)) {}

Storage::~Storage() = default;

std::string Storage::insert(const std::string& table, nlohmann::json& data, const std::string& user, const std::string& context) {
    // PK field is 'id'
    if (!data.contains("id") || data["id"].get<std::string>().empty()) {
        data["id"] = ULID::generate();
    }
    std::string id = data["id"];

    // Build SQL
    std::ostringstream cols, vals;
    std::vector<std::string> fields;
    std::vector<std::string> values;

    for (auto it = data.begin(); it != data.end(); ++it) {
        cols << it.key();
        vals << "?";
        if (std::next(it) != data.end()) {
            cols << ", ";
            vals << ", ";
        }
        fields.push_back(it.key());
        values.push_back(it.value().is_string() ? it.value().get<std::string>() : it.value().dump());
    }

    std::string sql = "INSERT INTO " + table + " (" + cols.str() + ") VALUES (" + vals.str() + ");";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(impl_->db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("SQLite prepare failed: " + std::string(sqlite3_errmsg(impl_->db)));
    }

    // Bind params
    for (size_t i = 0; i < values.size(); ++i) {
        sqlite3_bind_text(stmt, static_cast<int>(i + 1), values[i].c_str(), -1, SQLITE_TRANSIENT);
    }

    // Exec
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::string msg = "SQLite insert failed: " + std::string(sqlite3_errmsg(impl_->db));
        sqlite3_finalize(stmt);
        throw std::runtime_error(msg);
    }

    sqlite3_finalize(stmt);

    // TODO: Write change_log record here
    return id;
}

void Storage::update(const std::string& table, const nlohmann::json& data, const std::string& user, const std::string& context) {
    if (!data.contains("id")) throw std::runtime_error("Update requires PK field 'id'");
    std::string id = data["id"];

    std::ostringstream sql;
    sql << "UPDATE " << table << " SET ";
    std::vector<std::string> fields;
    std::vector<std::string> values;
    for (auto it = data.begin(); it != data.end(); ++it) {
        if (it.key() == "id") continue;
        sql << it.key() << " = ?";
        if (std::next(it) != data.end()) sql << ", ";
        fields.push_back(it.key());
        values.push_back(it.value().is_string() ? it.value().get<std::string>() : it.value().dump());
    }
    sql << " WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(impl_->db, sql.str().c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("SQLite prepare failed: " + std::string(sqlite3_errmsg(impl_->db)));
    }
    int idx = 1;
    for (const auto& val : values) {
        sqlite3_bind_text(stmt, idx++, val.c_str(), -1, SQLITE_TRANSIENT);
    }
    sqlite3_bind_text(stmt, idx, id.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::string msg = "SQLite update failed: " + std::string(sqlite3_errmsg(impl_->db));
        sqlite3_finalize(stmt);
        throw std::runtime_error(msg);
    }

    sqlite3_finalize(stmt);
    // TODO: Write change_log record here
}

void Storage::delete_row(const std::string& table, const nlohmann::json& pk_data, const std::string& user, const std::string& context) {
    if (!pk_data.contains("id")) throw std::runtime_error("Delete requires PK field 'id'");
    std::string id = pk_data["id"];

    std::string sql = "DELETE FROM " + table + " WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(impl_->db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("SQLite prepare failed: " + std::string(sqlite3_errmsg(impl_->db)));
    }
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::string msg = "SQLite delete failed: " + std::string(sqlite3_errmsg(impl_->db));
        sqlite3_finalize(stmt);
        throw std::runtime_error(msg);
    }
    sqlite3_finalize(stmt);
    // TODO: Write change_log record here
}
