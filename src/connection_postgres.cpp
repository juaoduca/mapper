#include "connection.hpp"
#include <libpq-fe.h>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>


class PgConnection final : public Connection {
public:
    ~PgConnection() override { disconnect(); }

    void connect(const std::string& dsn) override {
        disconnect();
        conn_ = PQconnectdb(dsn.c_str());
        if (PQstatus(conn_) != CONNECTION_OK) {
            std::string msg = PQerrorMessage(conn_);
            disconnect();
            throw std::runtime_error("Postgres connect failed: " + msg);
        }
        // Ensure standard settings if needed; explicit transactions left to caller.
    }

    void disconnect() noexcept override {
        if (conn_) { PQfinish(conn_); conn_ = nullptr; }
    }

    bool execDDL(std::string_view sql) override {
        PGresult* r = PQexec(conn_, std::string(sql).c_str());
        if (!r) return false;
        auto status = PQresultStatus(r);
        bool ok = (status == PGRES_COMMAND_OK);
        PQclear(r);
        return ok;
    }

    int execDML(std::string_view sql,
                const std::vector<std::string>& params) override {
        std::vector<const char*> vals; vals.reserve(params.size());
        for (auto& p : params) vals.push_back(p.c_str());

        PGresult* r = params.empty()
            ? PQexec(conn_, std::string(sql).c_str())
            : PQexecParams(conn_, std::string(sql).c_str(),
                           static_cast<int>(params.size()),
                           nullptr, vals.data(), nullptr, nullptr, 0);

        if (!r) throw std::runtime_error("Postgres exec failed (null result)");
        auto status = PQresultStatus(r);
        if (!(status == PGRES_COMMAND_OK || status == PGRES_TUPLES_OK)) {
            std::string msg = PQerrorMessage(conn_);
            PQclear(r);
            throw std::runtime_error("Postgres DML failed: " + msg);
        }
        int affected = 0;
        if (const char* s = PQcmdTuples(r); s && *s) affected = std::atoi(s);
        PQclear(r);
        return affected;
    }

    std::vector<nlohmann::json>
    get(std::string_view sql, const std::vector<std::string>& params) override {
        std::vector<const char*> vals; vals.reserve(params.size());
        for (auto& p : params) vals.push_back(p.c_str());

        PGresult* r = params.empty()
            ? PQexec(conn_, std::string(sql).c_str())
            : PQexecParams(conn_, std::string(sql).c_str(),
                           static_cast<int>(params.size()),
                           nullptr, vals.data(), nullptr, nullptr, 0);

        if (!r) throw std::runtime_error("Postgres exec failed (null result)");
        auto status = PQresultStatus(r);
        if (status != PGRES_TUPLES_OK) {
            std::string msg = PQerrorMessage(conn_);
            PQclear(r);
            throw std::runtime_error("Postgres SELECT failed: " + msg);
        }

        int rows = PQntuples(r);
        int cols = PQnfields(r);
        std::vector<nlohmann::json> out; out.reserve(rows);
        for (int i = 0; i < rows; ++i) {
            nlohmann::json row = nlohmann::json::object();
            for (int j = 0; j < cols; ++j) {
                const char* name = PQfname(r, j);
                if (PQgetisnull(r, i, j)) row[name] = nullptr;
                else                       row[name] = std::string(PQgetvalue(r, i, j));
            }
            out.push_back(std::move(row));
        }
        PQclear(r);
        return out;
    }

private:
    PGconn* conn_ = nullptr;
};

// Factory
ConnectionPtr make_postgres_connection() {
    return std::make_unique<PgConnection>();
}
