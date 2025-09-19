#include "select_builder.hpp"
#include "ddl_visitor.hpp" // for quote behavior reference
#include <sstream>
#include <unordered_map>
#include <stdexcept>
#include <strings.h>

namespace {

static std::string qident(const std::string& s, Dialect d) {
    if (d == Dialect::Postgres) return "\"" + s + "\"";
    return "\"" + s + "\""; // SQLite too (keeps single source)
}

struct Table {
    std::shared_ptr<const OrmSchema> schema;
    std::string alias;
};

static std::vector<Table> parent_chain(std::shared_ptr<const OrmSchema> leaf) {
    std::vector<Table> chain;
    std::shared_ptr<const OrmSchema> cur = leaf;
    while (cur) {
        chain.emplace(chain.begin(), (Table){cur, {}}); // root..leaf
        cur = cur->parent.lock();
    }
    for (size_t i=0;i<chain.size();++i) chain[i].alias = "t"+std::to_string(i);
    return chain;
}

static const OrmProp* find_owner_prop(const std::vector<Table>& chain, const std::string& name) {
    // search leaf->root, allow overrides
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        const auto& s = *it->schema;
        for (const auto& p : s.fields) {
            if ( lib::istrcmp(p.second->name.c_str(), name.c_str()) ) {
                return p.second.get();
            }
        }
    }
    return nullptr;
}

static std::string json_object_sql(Dialect d,
    const std::vector<std::pair<std::string,std::string>>& key_exprs)
{
    std::ostringstream oss;
    if (d == Dialect::Postgres) {
        oss << "jsonb_build_object(";
    } else {
        oss << "json_object(";
    }
    for (size_t i=0;i<key_exprs.size();++i) {
        if (i) oss << ", ";
        oss << "'" << key_exprs[i].first << "', " << key_exprs[i].second;
    }
    oss << ")";
    return oss.str();
}

static std::string json_array_agg_sql(Dialect d, const std::string& expr) {
    return (d == Dialect::Postgres) ? ("jsonb_agg(" + expr + ")") : ("json_group_array(" + expr + ")");
}

static std::string make_agg_expr(Dialect d, ql::AggKind k, const std::string& colExpr) {
    switch (k) {
        case ql::AggKind::Count:
            // semantic: COUNT(*) regardless of field (matches example)
            return "count(*)";
        case ql::AggKind::Avg:
            return "avg(" + colExpr + ")";
        case ql::AggKind::Sum:
            return "sum(" + colExpr + ")";
        case ql::AggKind::None:
        default:
            return colExpr;
    }
}

} // anon ..

std::string SelectBuilder::build_sql(const ql::QueryDoc& q) const {
    auto leaf = get_schema_(q.rootTypeName);
    if (!leaf) throw std::runtime_error("Unknown root type: " + q.rootTypeName);

    // chain & FROM/JOIN
    auto chain = parent_chain(std::shared_ptr<const OrmSchema>(leaf));
    if (chain.empty()) throw std::runtime_error("Empty parent chain for: " + q.rootTypeName);

    std::ostringstream from;
    from << "FROM " << qident(chain.front().schema->name, dialect_) << " " << chain.front().alias << "\n";
    for (size_t i=1;i<chain.size();++i) {
        const auto& prev = chain[i-1];
        const auto& cur  = chain[i];
        from << "JOIN " << qident(cur.schema->name, dialect_) << " " << cur.alias
             << " ON (" << prev.alias << "." << qident("ID", dialect_)
             << " = "    << cur.alias  << "." << qident("ID", dialect_) << ")\n";
    }

    struct OutCol {
        std::string key;     // JSON key (alias or name), case-preserved
        std::string expr;    // SQL expression for that key (may include aggregate)
        bool isGroup = false;
        bool isAgg   = false;
    };

    std::vector<OutCol> out;
    std::vector<std::string> groupBy; // fully-qualified column exprs
    bool hasAgg = false;

    // Build output columns
    for (const auto& f : q.selectionSet) {
        const std::string key = f.alias.value_or(f.name);

        // find owner + table alias (case-insensitive match)
        const OrmProp* owner = find_owner_prop(chain, f.name);
        if (!owner) throw std::runtime_error("Field not found in hierarchy: " + f.name);

        std::string tableAlias;
        for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
            for (const auto& p : it->schema->fields) {
                if ( lib::istrcmp(p.second->name.c_str(), f.name.c_str()) ) {
                    tableAlias = it->alias;
                    break;
                }
            }
            if (!tableAlias.empty()) break;
        }
        if (tableAlias.empty())
            throw std::runtime_error("Owner table alias resolution failed for: " + f.name);

        const std::string baseExpr = tableAlias + "." + qident(owner->name, dialect_);

        OutCol oc;
        oc.key = key;
        oc.isGroup = f.groupBy;
        oc.isAgg   = (f.agg != ql::AggKind::None);
        hasAgg = hasAgg || oc.isAgg;

        if (oc.isAgg) {
            oc.expr = make_agg_expr(dialect_, f.agg, baseExpr);
        } else {
            oc.expr = baseExpr;
        }

        if (oc.isGroup) groupBy.push_back(baseExpr);
        out.push_back(std::move(oc));
    }

    // Validate: if there is any aggregate, every non-agg must be grouped
    if (hasAgg) {
        for (const auto& oc : out) {
            if (!oc.isAgg && !oc.isGroup) {
                throw std::runtime_error(
                    "Non-aggregated projection '" + oc.key + "' must be marked with .groupby when aggregates are used");
            }
        }
    }

    // Build JSON object from out[]
    std::vector<std::pair<std::string,std::string>> key_exprs;
    key_exprs.reserve(out.size());
    for (const auto& oc : out) key_exprs.emplace_back(oc.key, oc.expr);

    const auto json_obj = json_object_sql(dialect_, key_exprs);
    const auto json_agg = json_array_agg_sql(dialect_, "obj");

    std::ostringstream sql;
    sql << "SELECT " << json_agg << " AS data\nFROM (\n"
        << "  SELECT " << json_obj << " AS obj\n"
        << from.str();

    if (!groupBy.empty()) {
        // GROUP BY clause over raw column expressions (not aliases)
        sql << "GROUP BY ";
        for (size_t i=0;i<groupBy.size();++i) {
            if (i) sql << ", ";
            sql << groupBy[i];
        }
        sql << "\n";
    }

    sql << ") s;";

    std::cout << sql.str() << std::endl;
    return sql.str();
}
