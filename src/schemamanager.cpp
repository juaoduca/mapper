#include "schemamanager.hpp"
#include "ddl_visitor.hpp"
#include <unordered_map>
#include <set>

SchemaManager::SchemaManager(const OrmSchema& old_schema, const OrmSchema& new_schema)
    : old_schema_(old_schema), new_schema_(new_schema) {}

std::vector<std::string> SchemaManager::plan_migration(const std::string& db_engine) {
    std::vector<std::string> ddl_statements;

    // Build maps for quick lookup
    std::unordered_map<std::string, OrmField> old_fields, new_fields;
    for (const auto& f : old_schema_.fields) old_fields[f.name] = f;
    for (const auto& f : new_schema_.fields) new_fields[f.name] = f;

    // ADD & ALTER columns
    for (const auto& [name, nf] : new_fields) {
        auto it = old_fields.find(name);
        if (it == old_fields.end()) {
            // ADD COLUMN
            DDLVisitor* v = nullptr;
            if (db_engine == "postgres") v = new PgDDLVisitor();
            else v = new SqliteDDLVisitor();
            std::string col_sql = nf.name + " " + v->sql_type(nf);
            if (nf.required) col_sql += " NOT NULL";
            if (nf.is_unique) col_sql += " UNIQUE";
            col_sql += v->sql_default(nf);
            delete v;
            ddl_statements.push_back("ALTER TABLE users ADD COLUMN " + col_sql + ";");
        } else {
            const auto& of = it->second;
            // ALTER COLUMN TYPE
            if (nf.type != of.type) {
                ddl_statements.push_back("ALTER TABLE users ALTER COLUMN " + nf.name +
                    " TYPE " + nf.type + ";");
            }
            // ALTER COLUMN DEFAULT
            if (nf.default_value != of.default_value) {
                ddl_statements.push_back("ALTER TABLE users ALTER COLUMN " + nf.name +
                    " SET DEFAULT " + (nf.default_value.empty() ? "NULL" : nf.default_value) + ";");
            }
            // ALTER COLUMN NULLABILITY
            if (nf.required != of.required) {
                if (nf.required)
                    ddl_statements.push_back("ALTER TABLE users ALTER COLUMN " + nf.name + " SET NOT NULL;");
                else
                    ddl_statements.push_back("ALTER TABLE users ALTER COLUMN " + nf.name + " DROP NOT NULL;");
            }
            // ALTER COLUMN UNIQUENESS (this is really an index operation, handle below)
        }
    }
    // DROP columns
    for (const auto& [name, of] : old_fields) {
        if (new_fields.find(name) == new_fields.end()) {
            ddl_statements.push_back("ALTER TABLE users DROP COLUMN " + of.name + ";");
        }
    }

    // Indexes: by name for easier diff
    auto to_index_key = [](const OrmIndex& idx) {
        std::string k = idx.index_name + ":";
        for (auto& f : idx.fields) k += f + ",";
        k += idx.type + (idx.unique ? ":U" : "");
        return k;
    };
    std::unordered_map<std::string, OrmIndex> old_idx_map, new_idx_map;
    for (const auto& idx : old_schema_.indexes) old_idx_map[to_index_key(idx)] = idx;
    for (const auto& idx : new_schema_.indexes) new_idx_map[to_index_key(idx)] = idx;

    // Add new indexes
    for (const auto& [k, idx] : new_idx_map) {
        if (old_idx_map.find(k) == old_idx_map.end()) {
            std::string sql = "CREATE ";
            if (idx.unique) sql += "UNIQUE ";
            sql += "INDEX ";
            if (!idx.index_name.empty()) sql += idx.index_name + " ";
            sql += "ON users (";
            for (size_t i = 0; i < idx.fields.size(); ++i) {
                sql += idx.fields[i];
                if (i < idx.fields.size() - 1) sql += ", ";
            }
            sql += ");";
            ddl_statements.push_back(sql);
        }
    }
    // Drop removed indexes
    for (const auto& [k, idx] : old_idx_map) {
        if (new_idx_map.find(k) == new_idx_map.end()) {
            // By convention, Postgres: DROP INDEX index_name;
            // In real world, you may need to store old index names
            if (!idx.index_name.empty())
                ddl_statements.push_back("DROP INDEX " + idx.index_name + ";");
        }
    }

    // Per-field indexes (for 'index' and 'unique' in fields)
    // (Optional: logic can be extended here, similar to above)

    return ddl_statements;
}
