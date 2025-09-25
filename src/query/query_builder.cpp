#include "query_builder.hpp"
#include <stdexcept>
#include <algorithm>

namespace ql {

    QueryBuilder::QueryBuilder(const Doc& doc, GetSchemaFn get_schema, Dialect dialect)
        : doc_(doc), get_schema_(get_schema), dialect_(dialect) {}

    std::string QueryBuilder::buildSelect() {
        return "";
    }

}  // namespace ql

