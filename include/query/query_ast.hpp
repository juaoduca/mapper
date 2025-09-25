#pragma once
#include <string>
#include <vector>
#include <optional>
#include "lexer.h"
#include "orm.hpp"

namespace ql {

    class Value {
    public:
        std::string name;  // preserve case
        TokenType type;
    };

    class Field;

    struct FieldFunc {
        std::string name;
        std::vector<Token> args;
    };

    struct FieldArg { // "(" comparator, { RValue } ")";
        std::shared_ptr<Field> field;
        Token comparator;
        Value rvalue;
    };

    class FuncCall: public Value {
    public:
        std::vector<Token> args;
    };

    class Field: public Value {
    public:
        std::string alias; // preserve case
        std::vector<FieldArg> args;
        std::vector<FieldFunc> funcs; // function chain func.func().func().func
        std::vector<Field> fields;
    };

    struct QueryArg {
        Value lvalue;
        Token comparator;
        Value rvalue;
    };

    struct Query {
        std::string name;
        std::vector<QueryArg> args;
        std::vector<Field> fields;
        mutable std::shared_ptr<OrmSchema> schema;
    };

    struct Doc {
        std::string id; // for cache purposes
        std::vector<Query> queries;
    };

};