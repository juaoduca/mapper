#pragma once
#include "query_ast.hpp"
#include "orm.hpp"          // OrmSchema, OrmProp, Dialect enum
#include <string>
#include <vector>
#include <memory>

class SelectBuilder {
public:
    // catalog lookup is provided by caller
    using GetSchemaFn = std::function<std::shared_ptr<OrmSchema>(const std::string&)>;

    explicit SelectBuilder(GetSchemaFn get_schema, Dialect dialect)
      : get_schema_(std::move(get_schema)), dialect_(dialect) {}

    // Throws on rootType unknown or field not found in the chain.
    std::string build_sql(const ql::QueryDoc& q) const;

private:
    GetSchemaFn get_schema_;
    Dialect dialect_;

};
