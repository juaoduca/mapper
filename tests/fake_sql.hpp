#pragma once
#include <memory>
#include <string>
#include <vector>
#include <sqlconnection.hpp>

// ---- Test fakes ----
struct BoundParam {
    int idx{};
    json::Type valuetype;
    std::string valuename;
    std::string valuecast;
    PropType type;
};

class FakeSQLConnection;

class FakeStatement final : public SQLStatement {
public:
    std::string sql;
    std::vector<BoundParam> binds;
    int exec_calls{0};
    int bind_calls{0};
    FakeSQLConnection* owner{nullptr}; // set by connection::prepare

    void bind(int idx, const jval& value, const PropType& type) override {
        json::Type tp ;
        std::string nm = "null";
        std::string val;
        if (type == PropType::Json) { // value arrives as an object or as a string
            if (value.IsObject()) {
                val = jhlp::dump(value);
                tp = value.GetType();
            } else if (value.IsString()) {
                val = value.GetString();
                tp = value.GetType();
            }
        } else if (value.IsObject() && value.MemberCount() > 0) {
            tp = value.MemberBegin()->value.GetType();
            nm = value.MemberBegin()->name.GetString();
            val = jhlp::dump(value.MemberBegin()->value);
        } else if (value.IsString()) {
            val = value.GetString();
            tp = value.GetType();
        } else {
            tp = value.GetType();
            val = jhlp::dump(value);
        }
        binds.push_back({idx, tp, nm, val , type});
        bind_calls++;
    }
    int exec() override; // defined after FakeSQLConnection
    // int exec_ret() override {return 1; }; // with data rosAffcted + row_field[0,0] returning ID

    void set_null(int idx) override {

    }

    void set_text(int idx, std::string value) override {

    }


};

class FakeSQLConnection final : public SQLConnection {
public:
    struct CapturedStatement {
        std::string sql;
        std::vector<BoundParam> binds;
        int exec_calls{0};
        int bind_calls{0};
    };

    CapturedStatement last; // inspect in tests: conn.last.sql, conn.last.binds, ...

    std::unique_ptr<SQLStatement> prepare(const std::string& sql, int numParams) override ;

    void connect(const std::string&) override {}
    void disconnect() override {}
    bool begin() override { return true; }
    bool commit() override { return true; }
    void rollback() override {}

    int64_t nextValue(std::string name) override {return 0; }

};

// ---- Inline impls that need full types ----
inline int FakeStatement::exec() {
    ++exec_calls;
    // snapshot into owning connection for test inspection
    if (owner) {
        owner->last.sql        = sql;
        owner->last.binds      = binds;
        owner->last.exec_calls = exec_calls;
        owner->last.bind_calls = bind_calls;
    }
    return 1; // rows affected
}

std::unique_ptr<SQLStatement> FakeSQLConnection::prepare(const std::string& sql, int numParams) {
    auto stmt = std::make_unique<FakeStatement>();
    stmt->owner = this;
    stmt->sql   = sql; // keep as std::string; avoid dangling c_str()
    return stmt;     // implicit move to unique_ptr<SQLStatement>
}
