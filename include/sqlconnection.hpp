#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <random>
#include "jsonhlp.hpp"
#include "orm.hpp"


class Random {
private:
    std::random_device rd;
    std::mt19937 gen; // Declare the engine
public:
    // Initialize 'gen' in the constructor's initializer list
    Random() : gen(rd()) {}
    ~Random() = default;

    int get(int min, int max) {
        std::uniform_int_distribution<> distrib(min, max);
        return distrib(gen);
    }
};


class SQLStatement {
public:
    virtual ~SQLStatement() = default;
    virtual void bind(int idx, const jval& value, const PropType& type) = 0;
    virtual int exec() = 0;  // return rows affected
    // virtual int exec_ret() = 0; // with data rosAffcted + row_field[0,0] returning ID
protected:
    std::string name_;
    virtual void bind_null(int idx) = 0;
    virtual void bind_text(int idx, std::string value) = 0;

    void bind_datetime(int idx, std::string value) {
        bind_text(idx, value);
    }

    void bind_encoded(int idx, std::string data) {
        bind_text(idx, data);
    };

    void bind_bool(int idx, bool value) {
        bind_text(idx, value ? "true" : "false");
    }

};

class SQLConnection {
public:
    virtual ~SQLConnection() = default;

    virtual void createDB(const std::string &dsn) = 0;

    // Connect using a DSN / path (SQLite: filename; Postgres: conninfo).
    virtual void connect(const std::string &dsn) = 0;

    // Safe to call multiple times.
    virtual void disconnect()  = 0;

    virtual std::unique_ptr<SQLStatement> prepare(const std::string& sql, int numParams=-1) const = 0;

    virtual bool begin() = 0;
    virtual bool commit() = 0;
    virtual void rollback() = 0;

    virtual uint64_t nextValue(const std::string &name) = 0;

    virtual bool execDDL(const std::string &ddl) = 0; // true | false
    virtual int  execDML(const std::string &dml, void* result) = 0; // return rows_affected, result => RETURNING clause
    virtual bool execGET(const std::string &sql, char **result) = 0; // result JSON string

    std::string stmtName() const {
        low_++;
        int high = random_.get(1234, 9876);
        return   "stmt-"+(std::to_string(high)+"."+std::to_string(low_));
    }

protected:
    bool tr_started_;
    mutable Random random_;
    mutable int low_ = 5678;
};

//factory function declaration - implemented in connection_sqlite.cpp
std::unique_ptr<SQLConnection> make_sqlite_connection();
//factory function declaration - implemented in connection_postgres.cpp
std::unique_ptr<SQLConnection> make_postgres_connection();
