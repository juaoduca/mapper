#pragma once
#include "orm.hpp"
#include "query_compiler.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <memory>
#include <functional>

namespace ql {

class QueryBuilder {
public:
    QueryBuilder(const Doc& doc, GetSchemaFn get_schema, Dialect dialect);
    std::string buildSelect();  // Returns the SQL SELECT statement

private:
    const Doc& doc_;
    GetSchemaFn get_schema_;
    Dialect dialect_;

};

}  // namespace ql

// #endif  // QUERY_BUILDER_HPP