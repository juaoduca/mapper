#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <random>
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
    virtual void set_null(int idx) = 0;
    virtual void set_text(int idx, std::string value) = 0;

    void set_datetime(int idx, std::string value) {
        set_text(idx, value);
    }

    void set_encoded(int idx, std::string data) {
        set_text(idx, data);
    };

    void set_bool(int idx, bool value) {
        set_text(idx, value ? "true" : "false");
    }

};

class SQLConnection {
public:
    virtual ~SQLConnection() = default;

    // Connect using a DSN / path (SQLite: filename; Postgres: conninfo).
    virtual void connect(const std::string& dsn) = 0;

    // Safe to call multiple times.
    virtual void disconnect()  = 0;

    virtual std::unique_ptr<SQLStatement> prepare(const std::string& sql, int numParams=-1) = 0;

    virtual bool begin() = 0;
    virtual bool commit() = 0;
    virtual void rollback() = 0;

    virtual int64_t nextValue(std::string name) = 0;

    std::string stmtName(){
        low_++;
        int high = random_.get(1234, 9876);
        return   "stmt-"+(std::to_string(high)+"."+std::to_string(low_));
    }

protected:
    bool tr_started_;
    Random random_;
    int low_ = 5678;
};

// Helpers for ownership
using PSQLConnection = std::unique_ptr<SQLConnection>;
