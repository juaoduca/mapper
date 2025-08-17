#include "sqlconnection.hpp"
#include "orm.hpp"
#include <sqlite3.h>
#include <stdexcept>

class SQLiteStatement final : public SQLStatement {
public:
    explicit SQLiteStatement(sqlite3_stmt* stmt) : stmt_(stmt) {}
    ~SQLiteStatement() override {
        if (stmt_) sqlite3_finalize(stmt_);
    }

    void bind(int idx, const nlohmann::json& value, const std::string& type) override {
    using nlohmann::json;

    // Null maps to NULL for every type
    if (value.is_null()) {
        sqlite3_bind_null(stmt_, idx);
        return;
    }

    // Helper to bind TEXT
    auto bind_text = [&](const std::string& s) {
        sqlite3_bind_text(stmt_, idx, s.c_str(), static_cast<int>(s.size()), SQLITE_TRANSIENT);
    };
    // Helper to bind BLOB
    auto bind_blob = [&](const void* data, size_t len) {
#if SQLITE_VERSION_NUMBER >= 3007015
        sqlite3_bind_blob64(stmt_, idx, data, static_cast<sqlite3_uint64>(len), SQLITE_TRANSIENT);
#else
        sqlite3_bind_blob(stmt_, idx, data, static_cast<int>(len), SQLITE_TRANSIENT);
#endif
    };

    // STRING
    if (type == DT_STR) {
        if (!value.is_string()) throw std::runtime_error("Expected string");
        bind_text(value.get<std::string>());
        return;
    }

    // INTEGER
    if (type == DT_INT) {
        if (value.is_number_integer()) {
            sqlite3_bind_int64(stmt_, idx, value.get<long long>());
            return;
        }
        throw std::runtime_error("Expected integer");
    }

    // NUMBER (REAL)
    if (type == DT_NUM) {
        if (value.is_number_float()) {
            sqlite3_bind_double(stmt_, idx, value.get<double>());
            return;
        }
        if (value.is_number_integer()) {
            // Upcast integer to REAL if schema says number
            sqlite3_bind_double(stmt_, idx, static_cast<double>(value.get<long long>()));
            return;
        }
        throw std::runtime_error("Expected number");
    }

    // BOOLEAN (store as INTEGER 0/1; SQLite BOOLEAN affinity is numeric)
    if (type == DT_BOOL) {
        if (value.is_boolean()) {
            sqlite3_bind_int(stmt_, idx, value.get<bool>() ? 1 : 0);
            return;
        }
        // Optional: allow 0/1 numbers for bool
        if (value.is_number_integer()) {
            sqlite3_bind_int(stmt_, idx, value.get<long long>() != 0 ? 1 : 0);
            return;
        }
        throw std::runtime_error("Expected boolean");
    }

    // DATE/TIME/DATETIME/TIMESTAMP — store as TEXT (ISO-8601)
    if (type == DT_DATE || type == DT_TIME || type == DT_DTIME || type == DT_TIMEST) {
        if (!value.is_string()) throw std::runtime_error("Expected ISO-8601 string for date/time");
        bind_text(value.get<std::string>());
        return;
    }

    // JSON — DDL maps to BLOB; store canonical JSON UTF-8 bytes
    if (type == DT_JSON) {
        std::string payload;
        if (value.is_string()) {
            // Accept pre-serialized JSON string
            payload = value.get<std::string>();
        } else {
            // Serialize object/array/etc. to JSON text
            payload = value.dump();
        }
        bind_blob(payload.data(), payload.size());
        return;
    }

    // BINARY — expect a string of raw bytes (or base64 if that’s your convention)
    if (type == DT_BIN) {
        if (!value.is_string()) throw std::runtime_error("Expected binary as string of bytes");
        const std::string& s = value.get_ref<const std::string&>();
        bind_blob(s.data(), s.size());
        return;
    }

    // Fallback
    throw std::runtime_error("Unsupported declared type for bind: " + type);
}

    int exec() override {
        int rc = sqlite3_step(stmt_);
        if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
            throw std::runtime_error("SQLite exec failed");
        }
        return sqlite3_changes(sqlite3_db_handle(stmt_));
    }

private:
    sqlite3_stmt* stmt_;
};

class SQLiteConnection final : public SQLConnection {
public:
    ~SQLiteConnection() override { disconnect(); }

    void connect(const std::string& dsn) override {
        disconnect();
        if (sqlite3_open(dsn.c_str(), &db_) != SQLITE_OK) {
            throw std::runtime_error("Failed to open SQLite DB: " + dsn);
        }
    }

    void disconnect() override {
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
    }

//transaction control
    bool begin() override {
        try {
            if (tr_started_) return true;
            tr_started_ = execSQL("BEGIN;");
            return tr_started_;
        } catch(...) {
            return false;
        }
    }

    bool commit() {
        if (!tr_started_) return false;
        if (execSQL("COMMIT;")) {
            tr_started_ = false;
            return true;
        }
        return false;
    }

    void rollback() {
        if (!tr_started_) return;
        if (execSQL("ROLLBACK;")) {
            tr_started_ = false;
        }
    }

    std::unique_ptr<SQLStatement> prepare(const std::string& sql) override {
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("SQLite prepare failed: " + sql);
        }
        return std::make_unique<SQLiteStatement>(stmt);
    }

private:
    bool execSQL(const char* sql) {
        char* errmsg = nullptr;
        if (sqlite3_exec(db_, sql, nullptr, nullptr, &errmsg) != SQLITE_OK) {
            std::string err = errmsg ? errmsg : "unknown";
            sqlite3_free(errmsg);
            throw std::runtime_error("SQLite error: " + err);
        }
        return true;
    }

    sqlite3* db_ = nullptr;
};



PSQLConnection make_sqlite_connection() {
    return std::make_unique<SQLiteConnection>();
}