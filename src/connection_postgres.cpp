// connection_postgres.cpp
#include "sqlconnection.hpp"
#include <format>
#include <libpq-fe.h>
#include <string>
#include <vector>
#include <cstdlib>

/*=============================  PgStatement  =============================*/
class PgStatement final : public SQLStatement {
public:
    PgStatement(PGconn* conn, std::string sql, std::string name)
        : conn_(conn), sql_(std::move(sql)) { name_ = name; }

    ~PgStatement() override = default;

    // Bind positional parameter (1-based) with declared schema type
    void bind(int idx, const jval& value, const PropType& type) override {
        ensure_slot_(idx);

        if (value.IsNull()) { bind_null(idx); return; }

        switch (type) {
            case PropType::String: {
                if(value.IsString()) {std::string v= value.GetString(); bind_text(idx, "'"+v+"'"); return;}
                THROW("bind: expected string"); return;
            }break;
            case PropType::Integer:
            case PropType::Number : {
                if (value.IsInt    ()) {int      v = value.GetInt    (); bind_text(idx, std::to_string(v)); return; }
                if (value.IsInt64  ()) {int64_t  v = value.GetInt64  (); bind_text(idx, std::to_string(v)); return; }
                if (value.IsUint   ()) {uint     v = value.GetUint   (); bind_text(idx, std::to_string(v)); return; }
                if (value.IsUint64 ()) {uint64_t v = value.GetUint64 (); bind_text(idx, std::to_string(v)); return; }
                if (value.IsFloat  ()) {float    v = value.GetDouble (); bind_text(idx, std::to_string(v)); return; }
                if (value.IsDouble ()) {double   v = value.GetDouble (); bind_text(idx, std::to_string(v)); return; }
                THROW("bind: expected integer or number");
            }; break;
            case PropType::Bool: {
                if(value.IsBool()){bind_bool(idx, value.GetBool() ); return;}
                if (value.IsInt()){bind_bool(idx, value.GetInt() == 1 ? true : false);return;} // 0=false 1 = true
                THROW("bind: expected boolean");
            }; break;
            case PropType::Date:
            case PropType::Time:
            case PropType::Dt_Time:
            case PropType::Tm_Stamp: {
                if (!value.IsString()) {bind_datetime(idx, value.GetString()); return;}
                THROW("bind: expected ISO-8601 string for date/time");
            };break;
            case PropType::Json: {
                if (value.IsObject()) {bind_text(idx, jhlp::asString(value)); return;}
                if (value.IsArray() ) {bind_text(idx, jhlp::asString(value)); return;}
                if (value.IsString()) {bind_text(idx, value.GetString()); return;}
                THROW("bind: expected JSON object JSON array or string");
            }break;
            case PropType::Bin: {
                if (value.IsString()) {bind_encoded(idx, value.GetString()); return;}
                THROW("bind: expected binary as yEnc string");
            }
        }
        THROW("bind: unsupported declared type for Postgres");

        // // 2) Match declared schema type (strict)
        // if (type == PropType::String) {
        //     if (!value.IsString()) THROW("bind: expected string");
        //     bind_text(value.GetString());
        //     return;
        // }

        // if (type == PropType::Integer || type == PropType::Number) {
        //     if (value.IsInt    ()) {int      v = value.GetInt    (); bind_text(std::to_string(v)); return; }
        //     if (value.IsInt64  ()) {int64_t  v = value.GetInt64  (); bind_text(std::to_string(v)); return; }
        //     if (value.IsUint   ()) {uint     v = value.GetUint   (); bind_text(std::to_string(v)); return; }
        //     if (value.IsUint64 ()) {uint64_t v = value.GetUint64 (); bind_text(std::to_string(v)); return; }
        //     if (value.IsFloat  ()) {float    v = value.GetDouble (); bind_text(std::to_string(v)); return; }
        //     if (value.IsDouble ()) {double   v = value.GetDouble (); bind_text(std::to_string(v)); return; }
        //     THROW("bind: expected integer or number");
        // }

        // if (type == PropType::Bool) {
        //     if (value.IsBool()) {
        //         bind_text(bool_to_text( value.GetBool() ));
        //         return;
        //     }
        //     if (value.IsInt()) {
        //         bind_text(bool_to_text(value.GetInt() != 0));
        //         return;
        //     }
        //     THROW("bind: expected boolean");
        // }

        // Dates/times as ISO-8601 text; server casts to DATE/TIME/TIMESTAMP/TIMESTAMPTZ
        // if (type == PropType::Date || type == PropType::Time || type == PropType::Dt_Time || type == PropType::Tm_Stamp) {
        //     if (!value.IsString()) THROW("bind: expected ISO-8601 string for date/time");
        //     bind_text(value.GetString());
        //     return;
        // }

        // JSON as text (server parses into json)
        // if (type == PropType::Json) {
        //     if (value.IsString()) bind_text(value.GetString());
        //     else bind_text(value.GetString());
        //     return;
        // }

        // BYTEA as hex text (“\x...”) so we can keep text format
        // if (type == PropType::Bin) {
        //     if (!value.IsString()) THROW("bind: expected binary as yEnc string");
        //     // const std::string& raw = value.get_ref<const std::string&>();
        //     bind_text(idx, value.GetString());
        //     return;
        // }

    }

    // Execute and return rows affected (INSERT/UPDATE/DELETE) or row count for SELECT
    int exec() override {
        const int nParams = static_cast<int>(params_.size());
        PGresult* res = PQexecParams(
            conn_,
            sql_.c_str(),
            nParams,
            nullptr,                                   // let server infer types
            (nParams ? params_.data()  : nullptr),
            (nParams ? lengths_.data() : nullptr),
            (nParams ? formats_.data() : nullptr),     // all text format
            0                                          // text results
        );
        if (!res) THROW("Postgres exec failed: no result");

        auto st = PQresultStatus(res);
        if (st != PGRES_COMMAND_OK && st != PGRES_TUPLES_OK) {
            std::string err = PQerrorMessage(conn_);
            PQclear(res);
            THROW("Postgres exec failed: " + err);
        }

        int rows = 0;
        if (st == PGRES_COMMAND_OK) {
            const char* t = PQcmdTuples(res);
            rows = (t && *t) ? std::atoi(t) : 0;
        }
        // else { // PGRES_TUPLES_OK
        //     rows = PQntuples(res);
        //     int cols = PQnfields(res);
        //     if (!PQgetisnull(res, 0, 0)) {

        //     }
        // }
        PQclear(res);
        return rows;
    }
protected:
    void bind_null(int idx) override {
        params_[idx-1]  = nullptr;  // SQL NULL
        lengths_[idx-1] = 0;
        formats_[idx-1] = 0;        // text format
    }

    void bind_text(int idx, std::string value) override {
        if (static_cast<size_t>(idx) > values_.size()) {
            values_.resize(idx);
        }
        values_[idx-1]  = value;                   // own storage
        params_[idx-1]  = value.c_str();
        lengths_[idx-1] = static_cast<int>(value.size());
        formats_[idx-1] = 0;
    }
private:
    void ensure_slot_(int idx) {
        if (idx < 1) THROW("bind: index must be >= 1");
        if (static_cast<size_t>(idx) > params_.size()) {
            params_.resize(idx, nullptr);
            lengths_.resize(idx, 0);
            formats_.resize(idx, 0);
        }
        if (static_cast<size_t>(idx) > values_.size())
            values_.resize(idx);
    }

    PGconn* conn_;
    std::string sql_;
    std::vector<const char*> params_;
    std::vector<std::string> values_; // backing for params_
    std::vector<int> lengths_;
    std::vector<int> formats_;
};

/*=============================  PgConnection  =============================*/
class PgConnection final : public SQLConnection {
private:
    PGconn* conn_ = nullptr;
public:
    ~PgConnection() override { disconnect(); }

    void createDB(const std::string &dsn) override {
        //create the DB
        //create CItext
        //create the sequence table to hold nextValue
        //
    }

    void connect(const std::string& dsn) override {
        disconnect();
        // if (dsn.find("connect=false") != std::string::npos) return;
        conn_ = PQconnectdb(dsn.c_str());
        if (!conn_ || PQstatus(conn_) != CONNECTION_OK) {
            std::string err = conn_ ? PQerrorMessage(conn_) : "no connection";
            disconnect();
            THROW("Postgres connect failed: " + err);
        }
    }

    void disconnect() override {
        if (conn_) {
            PQfinish(conn_);
            conn_ = nullptr;
        }
    }

    bool begin() override {
        if (tr_started_) return true;
        std::string s = "BEGIN;";
        tr_started_ = execDDL(s);
        return tr_started_;
    }

    bool commit() {
        if (!tr_started_) return false;
        std::string s = "COMMIT;";
        if (execDDL(s)) {
            tr_started_ = false;
            return true;
        }
        return false;
    }

    void rollback() {
        if (!tr_started_) return;
        std::string s = "ROLLBACK;";
        if (execDDL(s)) {
            tr_started_ = false;
        }
    }

    std::unique_ptr<SQLStatement> prepare(const std::string& sql, int numParams=-1) const override {
        if (!conn_) THROW("prepare: not connected");
        return std::make_unique<PgStatement>(conn_, sql, stmtName());
    }

    uint64_t nextValue(const std::string &name) override{
        std::string sql = "select nextval('"+name+"');";
        char *newid;
        if (execGET(sql, &newid) ) {
            const std::string s(reinterpret_cast<const char*>(newid));
            return std::stoull(s);
        };
        return 0 ;
    }

    bool execDDL(const std::string &ddl) override { // no rows affected, neither returning values
        if (!conn_) THROW("[execDDL()]: not connected");
        PGresult* res = PQexec(conn_, ddl.c_str());
        if (!res) THROW("[execDDL()]: Postgres error executing: %s", ddl.c_str());
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            std::string err = PQerrorMessage(conn_);
            PQclear(res);
            THROW("[execDDL()]:Postgres error: " + err);
        }
        PQclear(res);
        return true;
    };

    int execDML(const std::string &dml, void *result) override {// rows-affected and returning value
        if (!conn_) THROW("exec_simple_: not connected");
        PGresult* res = PQexec(conn_, dml.c_str());
        if (!res) THROW("Postgres error executing: %s", dml.c_str());
        result = nullptr;
        int rc = PQresultStatus(res);
        if (rc == PGRES_TUPLES_OK ) {
            if ( !PQgetisnull(res, 0,0) ) {
                result = PQgetvalue(res, 0, 0);
            }
            char *rows = PQcmdTuples(res);
            PQclear(res);
            return atoi(rows);
        }
        if (rc != PGRES_COMMAND_OK) {
            std::string err = PQerrorMessage(conn_);
            PQclear(res);
            THROW("Postgres error: " + err);
        }
        result = 0;
        return 0;

    };

    bool execGET(const std::string &sql, char **result) override {// no row count, just a returning value
        if (!conn_) THROW("exec_simple_: not connected");
        PGresult* res = PQexec(conn_, sql.c_str());
        if (!res) THROW("Postgres error executing: %s", sql.c_str());
        *result = (char*)"0";
        int rc = PQresultStatus(res);
        if (rc == PGRES_TUPLES_OK ) {
            if ( !PQgetisnull(res, 0,0) ) {
                *result = PQgetvalue(res, 0, 0);
            }
            return true;
        }
        if (rc != PGRES_COMMAND_OK) {
            std::string err = PQerrorMessage(conn_);
            PQclear(res);
            THROW("Postgres error: " + err);
        }
        return false;
    };
};

#if HAVE_POSTGRESQL
std::unique_ptr<SQLConnection> make_postgres_connection() {
    return std::make_unique<PgConnection>();
}
#endif