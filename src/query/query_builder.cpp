#include "query_builder.hpp"
#include <stdexcept>
#include <algorithm>

namespace ql {

QueryBuilder::QueryBuilder(const Doc& doc, GetSchemaFn get_schema, Dialect dialect)
    : doc_(doc), get_schema_(get_schema), dialect_(dialect) {}

std::string QueryBuilder::buildSelect() const {
    if (doc_.queries.empty()) {
        throw std::runtime_error("No queries in document");
    }

    std::ostringstream sql;
    sql << "SELECT ";

    std::string from, where, group_by;

    // Assume single query for simplicity; extend for multiple if needed
    const auto& query = doc_.queries.front();
    visitQuery(query, sql, from, where, group_by);

    sql << " FROM " << from;

    if (!where.empty()) {
        sql << " WHERE " << where;
    }

    if (!group_by.empty()) {
        sql << " GROUP BY " << group_by;
    }

    return sql.str();
}

void QueryBuilder::visitQuery(const Query& query, std::ostringstream& select, std::string& from, std::string& where, std::string& group_by) const {
    auto schema = getSchema(query.name);
    query.schema = schema;  // Set schema on query

    std::ostringstream fields_sql;
    for (const auto& field : query.fields) {
        visitField(field, fields_sql, *schema, from, where, group_by, schema->name);
    }

    std::string json_objects = fields_sql.str();
    if (!json_objects.empty() && json_objects.back() == ',') {
        json_objects.pop_back();  // Remove trailing comma
    }

    select << getJsonAggregate(json_objects);

    // Build FROM with parent chain
    from = buildParentJoins(*schema, from) + schema->name;

    // Build WHERE from query args
    where = buildWhereFromArgs(query.args, *schema);
}

void QueryBuilder::visitField(const Field& field, std::ostringstream& select, const OrmSchema& schema, std::string& from, std::string& where, std::string& group_by, const std::string& parent_alias) const {
    auto it = schema.fields.find(field.name);
    if (it == schema.fields.end()) {
        throw std::runtime_error("Field not found in schema: " + field.name);
    }
    const auto& prop = *it->second;

    std::string alias = field.alias.empty() ? field.name : field.alias;
    std::string field_expr = parent_alias + "." + field.name;

    // Apply functions
    for (const auto& func : field.funcs) {
        field_expr = translateFunc(func, field_expr);
        if (func.name == "sum" || func.name == "avg" || func.name == "count") {
            group_by += field_expr + ", ";
        }
    }

    // Add to SELECT as json_object part
    select << "'" << alias << "', " << field_expr << ", ";

    // Handle nested fields
    if (!field.fields.empty()) {
        auto ref_schema = prop.ref_Schema.lock();
        if (!ref_schema) {
            throw std::runtime_error("Referenced schema not found for field: " + field.name);
        }

        std::string nested_from, nested_where, nested_group_by;
        std::ostringstream nested_select;
        visitQuery({ref_schema->name, {}, field.fields, ref_schema}, nested_select, nested_from, nested_where, nested_group_by);

        // Build JOIN
        int level = 1;  // Default recursive or level 1
        for (const auto& func : field.funcs) {
            if (func.name == "level" && !func.args.empty()) {
                level = std::stoi(func.args.front().value);
            }
        }

        from += buildJoin(*ref_schema, parent_alias, prop, true, level);

        // Nested as subquery in SELECT
        select << "'" << alias << "', (" << nested_select.str() << " FROM " << nested_from;
        if (!nested_where.empty()) select << " WHERE " << nested_where;
        if (!nested_group_by.empty()) select << " GROUP BY " << nested_group_by;
        select << "), ";
    }

    // Add field args to where
    std::string field_where = buildWhereFromFieldArgs(field.args, field.name);
    if (!field_where.empty()) {
        where += (where.empty() ? "" : " AND ") + field_where;
    }
}

std::string QueryBuilder::translateFunc(const FieldFunc& func, const std::string& field_expr) const {
    std::ostringstream expr;
    std::string fname = func.name;
    std::string args;
    for (const auto& arg : func.args) {
        Value v; v.type = arg.type; v.name = arg.value;
        args += escapeValue(v) + ", ";
    }
    if (!args.empty()) args.pop_back();  // Remove trailing comma

    if (fname == "sum") {
        fname = "SUM";
    } else if (fname == "avg") {
        fname = "AVG";
    } else if (fname == "count") {
        fname = "COUNT";
    } else if (fname == "add") {
        return "(" + field_expr + " + " + args + ")";
    } else if (fname == "sub") {
        return "(" + field_expr + " - " + args + ")";
    } else if (fname == "mul") {
        return "(" + field_expr + " * " + args + ")";
    } else if (fname == "div") {
        return "(" + field_expr + " / " + args + ")";
    } else if (fname == "substr") {
        return "SUBSTR(" + field_expr + ", " + args + ")";
    } else if (fname == "concat") {
        return field_expr + " || " + args;
    } else if (fname == "groupby") {
        return field_expr;  // Handled in group_by
    } // Add more for date, etc.

    return fname + "(" + field_expr + (args.empty() ? "" : ", " + args) + ")";
}

std::string QueryBuilder::buildJoin(const OrmSchema& ref_schema, const std::string& parent_alias, const OrmProp& ref_field, bool is_nested, int level) const {
    std::string join_type = is_nested ? "LEFT JOIN" : "JOIN";
    std::string alias = getTableAlias(ref_schema, level);
    std::string on = parent_alias + "." + ref_schema.idprop()->name + " = " + alias + "." + ref_field.name;

    if (level >= 2) {
        // Multiple joins for self-joins
        std::ostringstream joins;
        for (int i = 1; i < level; ++i) {
            std::string prev_alias = getTableAlias(ref_schema, i - 1);
            std::string curr_alias = getTableAlias(ref_schema, i);
            joins << " " << join_type << " " << ref_schema.name << " " << curr_alias << " ON " << prev_alias + "." + ref_schema.idprop()->name + " = " << curr_alias + "." + ref_field.name;
        }
        return joins.str();
    } else if (level == 1 && ref_schema.name == parent_alias) {
        // Recursive CTE for self-join
        return "WITH RECURSIVE cte AS (SELECT * FROM " + ref_schema.name + " UNION SELECT r.* FROM " + ref_schema.name + " r JOIN cte ON cte." + ref_schema.idprop()->name + " = r." + ref_field.name + ") ";
    }

    return " " + join_type + " " + ref_schema.name + " " + alias + " ON " + on;
}

std::string QueryBuilder::getJsonAggregate(const std::string& json_objects) const {
    if (dialect_ == Dialect::SQLite) {
        return "json_group_array(json_object(" + json_objects + "))";
    } else if (dialect_ == Dialect::Postgres) {
        return "json_agg(row_to_json(t.*)) FROM (SELECT " + json_objects + ") t";
    }
    throw std::runtime_error("Unsupported dialect");
}

std::shared_ptr<OrmSchema> QueryBuilder::getSchema(const std::string& name) const {
    auto it = schemas_.find(name);
    if (it != schemas_.end()) return it->second;
    auto schema = get_schema_(name);
    if (!schema) throw std::runtime_error("Schema not found: " + name);
    schemas_[name] = schema;
    return schema;
}

std::string QueryBuilder::getTableAlias(const OrmSchema& schema, int level) const {
    return schema.name + (level > 0 ? "_" + std::to_string(level) : "");
}

std::string QueryBuilder::buildParentJoins(const OrmSchema& schema, std::string& from) const {
    std::ostringstream joins;
    auto current = schema.parent.lock();
    std::string parent_alias = schema.name;
    while (current) {
        std::string alias = getTableAlias(*current);
        joins << buildJoin(*current, parent_alias, *schema.idprop(), false);
        parent_alias = alias;
        current = current->parent.lock();
    }
    return joins.str();
}

std::string QueryBuilder::buildWhereFromArgs(const std::vector<QueryArg>& args, const OrmSchema& schema) const {
    std::ostringstream where;
    for (const auto& arg : args) {
        std::string lval = escapeValue(arg.lvalue);
        std::string comp = translateComparator(arg.comparator);
        std::string rval = escapeValue(arg.rvalue);
        where << lval << " " << comp << " " << rval << " AND ";
    }
    if (!where.str().empty()) where.str().erase(where.str().size() - 5);  // Remove last " AND "
    return where.str();
}

std::string QueryBuilder::buildWhereFromFieldArgs(const std::vector<FieldArg>& args, const std::string& field_name) const {
    std::ostringstream where;
    for (const auto& arg : args) {
        std::string comp = translateComparator(arg.comparator);
        std::string rval = escapeValue(arg.rvalue);
        where << field_name << " " << comp << " " << rval << " AND ";
    }
    if (!where.str().empty()) where.str().erase(where.str().size() - 5);
    return where.str();
}

std::string QueryBuilder::translateComparator(const Token& comp) const {
    switch (comp.type) {
        case ttEQUALS: return "=";
        case ttCOLON: return "=";
        case ttNOT_EQUALS: return "!=";
        case ttLESS: return "<";
        case ttGREATER: return ">";
        case ttLESS_EQUALS: return "<=";
        case ttGREATER_EQUALS: return ">=";
        default: throw std::runtime_error("Invalid comparator");
    }
}

std::string QueryBuilder::escapeValue(const Value& val) const {
    if (val.type == ttSTRING) return "'" + val.name + "'";
    return val.name;  // For numbers, fields, etc.
}

}  // namespace ql

