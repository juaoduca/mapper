// connection_postgres.cpp
#include "sqlconnection.hpp"
#include "orm.hpp"                 // DT_STR, DT_INT, DT_NUM, DT_BOOL, DT_DATE, DT_TIME, DT_DTIME, DT_TIMEST, DT_BIN, DT_JSON
#include <libpq-fe.h>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstdlib>

using nlohmann::json;

/*=============================  PgStatement  =============================*/
class PgStatement final : public SQLStatement {
public:
    PgStatement(PGconn* conn, std::string sql)
        : conn_(conn), sql_(std::move(sql)) {}

    ~PgStatement() override = default;

    // Bind positional parameter (1-based) with declared schema type
    void bind(int idx, const json& value, const std::string& type) override {
        ensure_slot_(idx);

        auto set_null = [&](){
            params_[idx-1]  = nullptr;  // SQL NULL
            lengths_[idx-1] = 0;
            formats_[idx-1] = 0;        // text format
        };

        auto set_text = [&](const std::string& s){
            if (static_cast<size_t>(idx) > values_.size()) values_.resize(idx);
            values_[idx-1]  = s;                   // own storage
            params_[idx-1]  = values_[idx-1].c_str();
            lengths_[idx-1] = static_cast<int>(values_[idx-1].size());
            formats_[idx-1] = 0;                   // text format
        };

        auto bool_to_text = [&](bool b){ return b ? std::string("true") : std::string("false"); };

        auto hex_encode = [&](const std::string& bytes){
            static const char* hexd = "0123456789abcdef";
            std::string out;
            out.reserve(2 + bytes.size()*2);
            out += "\\x";
            for (unsigned char c : bytes) {
                out.push_back(hexd[(c >> 4) & 0xF]);
                out.push_back(hexd[(c     ) & 0xF]);
            }
            return out;
        };

        // 1) NULL
        if (value.is_null()) { set_null(); return; }

        // 2) Match declared schema type (strict)
        if (type == DT_STR) {
            if (!value.is_string()) throw std::runtime_error("bind: expected string");
            set_text(value.get<std::string>());
            return;
        }

        if (type == DT_INT) {
            if (!value.is_number_integer()) throw std::runtime_error("bind: expected integer");
            set_text(std::to_string(value.get<long long>()));
            return;
        }

        if (type == DT_NUM) {
            if (value.is_number_float()) {
                set_text(std::to_string(value.get<double>()));
                return;
            }
            if (value.is_number_integer()) {
                set_text(std::to_string(static_cast<long long>(value.get<long long>())));
                return;
            }
            throw std::runtime_error("bind: expected number");
        }

        if (type == DT_BOOL) {
            if (value.is_boolean()) {
                set_text(bool_to_text(value.get<bool>()));
                return;
            }
            if (value.is_number_integer()) {
                set_text(bool_to_text(value.get<long long>() != 0));
                return;
            }
            throw std::runtime_error("bind: expected boolean");
        }

        // Dates/times as ISO-8601 text; server casts to DATE/TIME/TIMESTAMP/TIMESTAMPTZ
        if (type == DT_DATE || type == DT_TIME || type == DT_DTIME || type == DT_TIMEST) {
            if (!value.is_string()) throw std::runtime_error("bind: expected ISO-8601 string for date/time");
            set_text(value.get<std::string>());
            return;
        }

        // JSON as text (server parses into json)
        if (type == DT_JSON) {
            if (value.is_string()) set_text(value.get<std::string>());
            else set_text(value.dump());
            return;
        }

        // BYTEA as hex text (“\x...”) so we can keep text format
        if (type == DT_BIN) {
            if (!value.is_string()) throw std::runtime_error("bind: expected binary as string of bytes");
            const std::string& raw = value.get_ref<const std::string&>();
            set_text(hex_encode(raw));
            return;
        }

        throw std::runtime_error("bind: unsupported declared type for Postgres");
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
        if (!res) throw std::runtime_error("Postgres exec failed: no result");

        auto st = PQresultStatus(res);
        if (st != PGRES_COMMAND_OK && st != PGRES_TUPLES_OK) {
            std::string err = PQerrorMessage(conn_);
            PQclear(res);
            throw std::runtime_error("Postgres exec failed: " + err);
        }

        int rows = 0;
        if (st == PGRES_COMMAND_OK) {
            const char* t = PQcmdTuples(res);
            rows = (t && *t) ? std::atoi(t) : 0;
        } else { // PGRES_TUPLES_OK
            rows = PQntuples(res);
        }
        PQclear(res);
        return rows;
    }

private:
    void ensure_slot_(int idx) {
        if (idx <= 0) throw std::runtime_error("bind: index must be >= 1");
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
public:
    ~PgConnection() override { disconnect(); }

    void connect(const std::string& dsn) override {
        disconnect();
        conn_ = PQconnectdb(dsn.c_str());
        if (!conn_ || PQstatus(conn_) != CONNECTION_OK) {
            std::string err = conn_ ? PQerrorMessage(conn_) : "no connection";
            disconnect();
            throw std::runtime_error("Postgres connect failed: " + err);
        }
    }

    void disconnect() override {
        if (conn_) {
            PQfinish(conn_);
            conn_ = nullptr;
        }
    }

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
        if (!conn_) throw std::runtime_error("prepare: not connected");
        return std::make_unique<PgStatement>(conn_, sql);
    }

private:
    bool execSQL(const char* sql) {
        if (!conn_) throw std::runtime_error("exec_simple_: not connected");
        PGresult* res = PQexec(conn_, sql);
        if (!res) throw std::runtime_error(std::string("Postgres error executing: ") + sql);
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            std::string err = PQerrorMessage(conn_);
            PQclear(res);
            throw std::runtime_error("Postgres error: " + err);
        }
        PQclear(res);
        return true;
    }

    PGconn* conn_ = nullptr;
};

#if HAVE_POSTGRESQL
PSQLConnection make_postgres_connection() {
    return std::make_unique<PgConnection>();
}
#endif