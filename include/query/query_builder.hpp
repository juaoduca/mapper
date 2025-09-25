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
    std::string buildSelect() const;  // Returns the SQL SELECT statement

private:
    const Doc& doc_;
    GetSchemaFn get_schema_;
    Dialect dialect_;
    mutable std::unordered_map<std::string, std::shared_ptr<OrmSchema>> schemas_;  // Cache schemas by name

    // Visitor methods
    void visitQuery(const Query& query, std::ostringstream& sql, std::string& from, std::string& where, std::string& group_by) const;
    void visitField(const Field& field, std::ostringstream& select, const OrmSchema& schema, std::string& from, std::string& where, std::string& group_by, const std::string& parent_alias) const;
    std::string translateFunc(const FieldFunc& func, const std::string& field_expr) const;
    std::string buildJoin(const OrmSchema& ref_schema, const std::string& parent_alias, const OrmProp& ref_field, bool is_nested, int level = 0) const;
    std::string getJsonAggregate(const std::string& json_objects) const;

    // Helpers
    std::shared_ptr<OrmSchema> getSchema(const std::string& name) const;
    std::string getTableAlias(const OrmSchema& schema, int level = 0) const;
    std::string buildParentJoins(const OrmSchema& schema, std::string& from) const;
    std::string buildWhereFromArgs(const std::vector<QueryArg>& args, const OrmSchema& schema) const;
    std::string buildWhereFromFieldArgs(const std::vector<FieldArg>& args, const std::string& field_name) const;
    std::string translateComparator(const Token& comp) const;
    std::string escapeValue(const Value& val) const;
};

}  // namespace ql

// #endif  // QUERY_BUILDER_HPP