#include "sqlconnection.hpp"
#include <sqlite3.h>
#include <stdexcept>

class SQLiteConnection final : public SQLConnection {
public:
    ~SQLiteConnection() override { disconnect(); }

    void connect(const std::string& dsn) override {
        disconnect();
        if (sqlite3_open(dsn.c_str(), &db_) != SQLITE_OK) {
            throw std::runtime_error("SQLite open failed: " + std::string(sqlite3_errmsg(db_)));
        }
        // sane defaults
        execDDL("PRAGMA foreign_keys = ON;");
        execDDL("PRAGMA journal_mode = WAL;");
        execDDL("PRAGMA synchronous = NORMAL;");
    }

    void disconnect() noexcept override {
        if (db_) { sqlite3_close(db_); db_ = nullptr; }
    }

    bool execDDL(std::string sql) override {
        char* err = nullptr;
        int rc = sqlite3_exec(db_, std::string(sql).c_str(), nullptr, nullptr, &err);
        if (rc != SQLITE_OK) {
            if (err) sqlite3_free(err);
            return false;
        }
        return true;
    }

    int execDML(std::string sql,
                const std::vector<std::string>& params) override {
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, std::string(sql).c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("SQLite prepare failed: " + std::string(sqlite3_errmsg(db_)));
        }
        bind(stmt, params);
        int rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            std::string msg = "SQLite step failed: " + std::string(sqlite3_errmsg(db_));
            sqlite3_finalize(stmt);
            throw std::runtime_error(msg);
        }
        int changes = sqlite3_changes(db_);
        sqlite3_finalize(stmt);
        return changes;
    }

    std::vector<nlohmann::json>
    get(std::string sql, const std::vector<std::string>& params) override {
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, std::string(sql).c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("SQLite prepare failed: " + std::string(sqlite3_errmsg(db_)));
        }
        bind(stmt, params);

        std::vector<nlohmann::json> rows;
        int rc;
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            nlohmann::json row = nlohmann::json::object();
            int n = sqlite3_column_count(stmt);
            for (int i = 0; i < n; ++i) {
                const char* name = sqlite3_column_name(stmt, i);
                const unsigned char* txt = sqlite3_column_text(stmt, i);
                if (!name) continue;
                if (txt) row[name] = std::string(reinterpret_cast<const char*>(txt));
                else     row[name] = nullptr;
            }
            rows.push_back(std::move(row));
        }
        if (rc != SQLITE_DONE) {
            std::string msg = "SQLite step failed: " + std::string(sqlite3_errmsg(db_));
            sqlite3_finalize(stmt);
            throw std::runtime_error(msg);
        }
        sqlite3_finalize(stmt);
        return rows;
    }

private:
    static void bind(sqlite3_stmt* stmt, const std::vector<std::string>& params) {
        for (size_t i = 0; i < params.size(); ++i) {
            sqlite3_bind_text(stmt, static_cast<int>(i + 1), params[i].c_str(), -1, SQLITE_TRANSIENT);
        }
    }

    sqlite3* db_ = nullptr;
};

// Factory
PSQLConnection make_sqlite_connection() {
    return std::make_unique<SQLiteConnection>();
}
