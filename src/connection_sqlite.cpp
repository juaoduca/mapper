#include "sqlconnection.hpp"

#include <sqlite3.h>
#include <stdexcept>
#include <iostream>
#include <lib.hpp>


class SQLiteConnection; // fwd

class SQLiteStatement final : public SQLStatement {
private:
    sqlite3_stmt* stmt_;
    sqlite3 *stmt_db_;

    friend SQLiteConnection;
protected:
    void bind_text(int idx, std::string value) override {
        //handle unicode string UTF-8
        sqlite3_bind_text(stmt_, idx, value.c_str(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
        //the below function can handle UTF_16
        //sqlite3_bind_text64(stmt_, idx, s.c_str(), static_cast<int>(s.size()), SQLITE_TRANSIENT, SQLITE_UTF8);
    }

    void bind_null(int idx) override {
        sqlite3_bind_null(stmt_, idx);
    }

    // void bind_blob(int idx, const void* data, size_t len) override {
    //     (SQLITE_VERSION_NUMBER >= 3007015) ?
    //         sqlite3_bind_blob64(stmt_, idx, data, static_cast<sqlite3_uint64>(len), SQLITE_TRANSIENT) :
    //         sqlite3_bind_blob(stmt_, idx, data, static_cast<int>(len), SQLITE_TRANSIENT);
    // }

public:
    explicit SQLiteStatement(sqlite3_stmt* stmt): stmt_(stmt) { }
    ~SQLiteStatement() override { if (stmt_) sqlite3_finalize(stmt_); }

    void bind(int idx, const jval& value, const PropType& type) override {
        // Null maps to NULL for every type
        if (value.IsNull()) { bind_null(idx); return; }

        switch (type) {
            case PropType::String: { if(value.IsString()) {bind_text(idx, value.GetString()); return;}
                THROW("bind: expected string"); return;
            }; break;
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
                if (value.IsBool()){bind_bool(idx, value.GetBool() ); return;}
                if (value.IsInt ()){bind_bool(idx, value.GetInt() != 0);return;}
                THROW("bind: expected boolean");
            }; break;
            case PropType::Date:
            case PropType::Time:
            case PropType::Dt_Time:
            case PropType::Tm_Stamp: {
                if (value.IsString()) {
                    std::string v =value.GetString();
                    bind_datetime(idx, v);
                    return;
                }
                THROW("bind: expected ISO-8601 string for date/time");
            }; break;
            case PropType::Json: {
                if (value.IsObject()) {bind_text(idx, jhlp::asString(value)); return;}
                if (value.IsArray() ) {bind_text(idx, jhlp::asString(value)); return;}
                if (value.IsString()) {bind_text(idx, value.GetString()); return;}
                THROW("bind: expected JSON object JSON array or string");
            }; break;
            case PropType::Bin: {
                if (value.IsString()) {bind_text(idx, value.GetString()); return;}
                THROW("bind: expected binary as yEnc string");
            }
        }
    }

    int exec() override {
        int rc = sqlite3_step(stmt_);
        if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
            const char *msg = sqlite3_errmsg(stmt_db_);
            THROW("SQLite exec failed with msg: %s ", msg);
        }
        return sqlite3_changes(sqlite3_db_handle(stmt_));
    }
};

class SQLiteConnection final : public SQLConnection {
public:
    ~SQLiteConnection() override { disconnect(); }

    void createDB(const std::string &dsn) override {
        // connect(dsn);
        execDDL("CREATE TABLE serial (ID INTEGER PRIMARY KEY AUTOINCREMENT, val integer)");
        execDDL("insert into serial (val) values (null) ");
        execDDL("DROP TABLE serial");
        execDDL("INSERT INTO sqlite_sequence (name, seq) SELECT 'db', 0 WHERE NOT EXISTS ( SELECT 1 FROM sqlite_sequence WHERE name = 'db');");
        execDDL("INSERT INTO sqlite_sequence (name, seq) SELECT 'db_track', 0 WHERE NOT EXISTS ( SELECT 1 FROM sqlite_sequence WHERE name = 'db_track');");
        //create a temp table with SERIAL ID
        //insert a "db" row in the sqlite_sequence table to hold DBSerial ID
        //remove the temp table
    }

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
        if (tr_started_) return true;
        tr_started_ = sqlite3_exec(db_, "BEGIN;", nullptr, nullptr, nullptr) == SQLITE_OK;
        return tr_started_;
    }

    bool commit() {
        if (!tr_started_) return false;
        if (sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr) == SQLITE_OK) {
            tr_started_ = false;
            return true;
        }
        return false;
    }

    void rollback() {
        if (!tr_started_) return;
        if (sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr) == SQLITE_OK) {
            tr_started_ = false;
        }
    }

    std::unique_ptr<SQLStatement> prepare(const std::string& sql, int numParams/*=-1*/) const override {
        sqlite3_stmt* stmt = nullptr;
        // int nBytes = sql.size()+1; // (the number of chars where 1 char = 1 byte) + 1 null_terminator
        //param numParams ignored
        if (sqlite3_prepare_v2(db_, sql.c_str(), sql.size()+1, &stmt, nullptr) != SQLITE_OK) {
            THROW("SQLite prepare failed: " + sql);
        }
        std::unique_ptr<SQLiteStatement> stm = std::make_unique<SQLiteStatement>(stmt);
        stm->stmt_db_ = db_;
        return stm;
    }

    uint64_t nextValue(const std::string &name) override {
        int64_t value = 0;
        std::string sql = "UPDATE sqlite_sequence set seq = seq+1 where name = '"+name+"' RETURNING seq";
        if (execDML(sql, &value) > 0) {
            return value;
        }
        return 0;
    }

    bool execDDL(const std::string &ddl) override {
        char* errmsg = nullptr;
        int rc = sqlite3_exec(db_, ddl.c_str(), nullptr, nullptr, &errmsg);
        if (rc != SQLITE_OK) {
            std::string err = errmsg ? errmsg : "unknown";
            sqlite3_free(errmsg);
            THROW("SQLite error: " + err);
        }
        return true;
    };

    int execDML(const std::string &dml, void *result) override {
        sqlite3_stmt* stmt = nullptr;
        // int nBytes = sql.size()+1; // (the number of chars where 1 char = 1 byte) + 1 null_terminator
        int rc = sqlite3_prepare_v2(db_, dml.c_str(), dml.size()+1, &stmt, nullptr);
        if ( rc != SQLITE_OK) {
            THROW("SQLite prepare failed: " + dml);
        }
        int rows = 0; //sqlite3_changes(db_);
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            rows++;
            int ct = sqlite3_column_type(stmt, 0);
            if (ct == SQLITE_INTEGER) {
                int id = sqlite3_column_int(stmt, 0);
                *((int*)result) =  id;
            } else if (ct == SQLITE_TEXT) {
                result = (void*)sqlite3_column_text(stmt, 0);
            } else if (ct == SQLITE_FLOAT) {
                double val = sqlite3_column_double(stmt, 0);
                *((double*)result) = val;
            } else if (ct == SQLITE_NULL) {
                result = nullptr;
            }

        } else if (rc == SQLITE_DONE) {
            result = nullptr;
        } else {
            THROW("Failed to execute step");
        }
        sqlite3_finalize(stmt);
        return rows;
    };

    bool execGET(const std::string &sql, char **result) override {
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql.c_str(), sql.size()+1, &stmt, nullptr);
        if ( rc != SQLITE_OK) {
            THROW("SQLite prepare failed: " + sql);
        }

        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            // A row of data is available. Get the column value.
            // The first column is at index 0.
            char *r = (char *)sqlite3_column_text(stmt, 0);
            int len = strlen(r);
            *result = (char*)malloc(len+1);
            strcpy(*result, r);
            // result = sqlite3_column_text(stmt, 0);
        } else if (rc == SQLITE_DONE) {
            // *result = (char*)"0";
        } else {
            THROW("Failed to execute step");
        }
        int rows = sqlite3_changes(db_);
        sqlite3_finalize(stmt);
        return rows;
    };


private:
    sqlite3* db_ = nullptr;
};

std::unique_ptr<SQLConnection> make_sqlite_connection() {
    return std::make_unique<SQLiteConnection>();
}