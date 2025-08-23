
#include "sqlconnection.hpp"
#include <sqlite3.h>
#include <stdexcept>
#include <iostream>
#include <lib.hpp>

class SQLiteStatement final : public SQLStatement {
public:
    explicit SQLiteStatement(sqlite3_stmt* stmt)
        : stmt_(stmt) { }
    ~SQLiteStatement() override {
        if (stmt_) sqlite3_finalize(stmt_);
    }

    void set_text(int idx, str value) override {
        //handle unicode string UTF-8
        sqlite3_bind_text(stmt_, idx, value.c_str(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
        //the below function can handle UTF_16
        //sqlite3_bind_text64(stmt_, idx, s.c_str(), static_cast<int>(s.size()), SQLITE_TRANSIENT, SQLITE_UTF8);
    }

    void set_null(int idx) override {
        sqlite3_bind_null(stmt_, idx);
    }

    void bind(int idx, const jval& value, const PropType& type) override {

        // // Helper to bind BLOB
        // auto bind_blob = [&](const void* data, size_t len) {
        //     (SQLITE_VERSION_NUMBER >= 3007015) ?
        //         sqlite3_bind_blob64(stmt_, idx, data, static_cast<sqlite3_uint64>(len), SQLITE_TRANSIENT) :
        //         sqlite3_bind_blob(stmt_, idx, data, static_cast<int>(len), SQLITE_TRANSIENT);
        // };
        /************** HELPERS FUNCTIONS ***************************/


        // Null maps to NULL for every type
        if (value.IsNull()) { set_null(idx); return; }

        switch (type) {
            case PropType::String: { if(value.IsString()) {set_text(idx, value.GetString()); return;}
                THROW("bind: expected string"); return;
            }break;
            case PropType::Integer:
            case PropType::Number : {
                if (value.IsInt   ()) {sqlite3_bind_int   (stmt_, idx, value.GetInt   ()); return; }
                if (value.IsInt64 ()) {sqlite3_bind_int64 (stmt_, idx, value.GetInt64 ()); return; }
                if (value.IsUint  ()) {sqlite3_bind_int   (stmt_, idx, value.GetUint  ()); return; }
                if (value.IsUint64()) {sqlite3_bind_int64 (stmt_, idx, value.GetUint64()); return; }
                if (value.IsFloat ()) {sqlite3_bind_double(stmt_, idx, value.GetDouble()); return; }
                if (value.IsDouble()) {sqlite3_bind_double(stmt_, idx, value.GetDouble()); return; }
                THROW("bind: expected integer or number");
            }; break;
            case PropType::Bool: {
                if (value.IsBool()){set_bool(idx, value.GetBool() ); return;}
                if (value.IsInt ()){set_bool(idx, value.GetInt() != 0);return;}
                THROW("bind: expected boolean");
            }; break;
            case PropType::Date:
            case PropType::Time:
            case PropType::Dt_Time:
            case PropType::Tm_Stamp: {
                if (!value.IsString()) {str v =value.GetString(); set_datetime(idx, v); return;}
                THROW("bind: expected ISO-8601 string for date/time");
            };break;
            case PropType::Json: {
                if (value.IsObject()) {set_text(idx, jhlp::dump(value)); return;}
                if (value.IsArray() ) {set_text(idx, jhlp::dump(value)); return;}
                if (value.IsString()) {set_text(idx, value.GetString()); return;}
                THROW("bind: expected JSON object JSON array or string");
            }break;
            case PropType::Bin: {
                if (value.IsString()) {set_text(idx, value.GetString()); return;}
                THROW("bind: expected binary as yEnc string");
            }
        }
    }

    int exec() override {
        int rc = sqlite3_step(stmt_);
        if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
            THROW("SQLite exec failed");
        }
        return sqlite3_changes(sqlite3_db_handle(stmt_));
    }

    // execute with returning
    // int exec_ret() override{
    //     return 0;
    // }


private:
    sqlite3_stmt* stmt_;
};

class SQLiteConnection final : public SQLConnection {
public:
    ~SQLiteConnection() override { disconnect(); }

    void connect(const std::string& dsn) override {
        disconnect();
        if (sqlite3_open(dsn.c_str(), &db_) != SQLITE_OK) {
            THROW("Failed to open SQLite DB: " + dsn);
        }
    }

    void disconnect() override {
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
    }

    // transaction control
    bool begin() override {
        try {
            if (tr_started_) return true;
            tr_started_ = execSQL("BEGIN;");
            return tr_started_;
        } catch (...) {
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

    std::unique_ptr<SQLStatement> prepare(const std::string& sql, int numParams/*=-1*/) override {
        sqlite3_stmt* stmt = nullptr;
        // int nBytes = sql.size()+1; // (the number of chars where 1 char = 1 byte) + 1 null_terminator
        //param numParams ignored
        if (sqlite3_prepare_v2(db_, sql.c_str(), sql.size()+1, &stmt, nullptr) != SQLITE_OK) {
            THROW("SQLite prepare failed: " + sql);
        }
        return std::make_unique<SQLiteStatement>(stmt);
    }

    int64_t nextValue(std::string name) override {
        return 0 ;
    }


private:
    bool execSQL(const char* sql) {
        char* errmsg = nullptr;
        if (sqlite3_exec(db_, sql, nullptr, nullptr, &errmsg) != SQLITE_OK) {
            std::string err = errmsg ? errmsg : "unknown";
            sqlite3_free(errmsg);
            THROW("SQLite error: " + err);
        }
        return true;
    }

    sqlite3* db_ = nullptr;
};

PSQLConnection make_sqlite_connection() {
    return std::make_unique<SQLiteConnection>();
}