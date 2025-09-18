#include "ddl_visitor.hpp"

static std::string sql_escape_single_quotes(const std::string& s) {
    std::string out; out.reserve(s.size() + 4);
    for (char c : s) out += (c == '\'') ? "''" : std::string(1, c);
    return out;
}

std::string DDLVisitor::sql_default(const OrmProp& f) {
    using DK = DefaultKind;
    switch (f.default_kind) {
        case DK::None:
            return "";
        case DK::String:
            return " DEFAULT '" + sql_escape_single_quotes(f.default_value) + "'";
        case DK::Boolean:
        case DK::Number:
            return " DEFAULT " + f.default_value;
        case DK::Raw:
            return " DEFAULT " + f.default_value;
        case DK::Null:
            return " DEFAULT NULL";
    }
    return "";
}

/* ---------- PostgreSQL ---------- */

std::string PgDDLVisitor::sql_type(const OrmProp& f) {
    if (f.type == PropType::String   ) return "TEXT"                    ;
    if (f.type == PropType::Integer  ) return "INTEGER"                 ;
    if (f.type == PropType::Number   ) return "NUMERIC"                 ;
    if (f.type == PropType::Bool     ) return "BOOLEAN"                 ;
    if (f.type == PropType::Json     ) return "JSON"                    ;
    if (f.type == PropType::Date     ) return "DATE"                    ;
    if (f.type == PropType::Time     ) return "TIME"                    ;
    if (f.type == PropType::Dt_Time  ) return "TIMESTAMP"               ;
    if (f.type == PropType::Tm_Stamp ) return "TIMESTAMP WITH TIME ZONE";
    if (f.type == PropType::Bin      ) return "BYTEA"                   ;
    if (f.type == PropType::Object   ) {
        OrmProp *rf = f.ref_Field.lock().get();
        if (rf != nullptr) {
            if (rf->id_kind == IdKind::UUIDv7) {
                return "TEXT";
            } else {
                return "BIGINT";
            }
        } else {
            return "BIGINT";
        }
    }
    return "TEXT";
}

std::string PgDDLVisitor::visit(const OrmSchema& schema) {
    auto Q = [&](const std::string& id) -> std::string { return quote(id); };
    std::ostringstream ddl;
    ddl << "CREATE TABLE IF NOT EXISTS " << Q(schema.name) << "(\n";

    std::vector<std::string> pk_fields;
    size_t i = 0, n = schema.fields.size();

    for (const auto& kv : schema.fields) {
        const OrmProp& f = *kv.second;
        ddl << " " << Q(f.name) << " " << sql_type(f);
        if (f.required) ddl << " NOT NULL";
        if (f.is_unique) ddl << " UNIQUE";
        ddl << sql_default(f);
        if (f.is_id) pk_fields.push_back(Q(f.name));
        if (++i < n) ddl << ",\n";
        else ddl << "\n";
    }

    if (!pk_fields.empty()) {
        ddl << ",  PRIMARY KEY (";
        for (size_t j = 0; j < pk_fields.size(); ++j) {
            ddl << pk_fields[j];
            if (j + 1 < pk_fields.size()) ddl << ", ";
        }
        ddl << ")";
    }

    ddl << "\n);";

    // Per-field indexes
    for (const auto& kv : schema.fields) {
        const OrmProp& f = *kv.second;
        if (f.is_indexed && !f.is_id) {
            ddl << "\nCREATE ";
            if (f.is_unique) ddl << "UNIQUE ";
            ddl << "INDEX ";
            if (!f.index_name.empty()) ddl << Q(f.index_name) << " ";
            ddl << "ON " << Q(schema.name) << " (" << Q(f.name) << ");";
        }
    }

    // Schema-level composite indexes
    for (const auto& idx : schema.indexes) {
        ddl << "\nCREATE ";
        if (idx.unique) ddl << "UNIQUE ";
        ddl << "INDEX ";
        if (!idx.index_name.empty()) ddl << Q(idx.index_name) << " ";
        ddl << "ON " << Q(schema.name) << " (";
        for (size_t j = 0; j < idx.fields.size(); ++j) {
            ddl << Q(idx.fields[j]);
            if (j + 1 < idx.fields.size()) ddl << ", ";
        }
        ddl << ");";
    }

    ddl << std::endl;
    std::cout << "PgDDLVisitor::visit(): " << ddl.str();
    return ddl.str();
}

/* ---------- SQLite ---------- */

std::string SqliteDDLVisitor::sql_type(const OrmProp& f) {
    if (f.type == PropType::String   ) return "TEXT"     ;
    if (f.type == PropType::Integer  ) return "INTEGER"  ;
    if (f.type == PropType::Number   ) return "REAL"     ;
    if (f.type == PropType::Bool     ) return "BOOLEAN"  ;
    if (f.type == PropType::Json     ) return "TEXT"     ;
    if (f.type == PropType::Date     ) return "DATE"     ;
    if (f.type == PropType::Time     ) return "TIME"     ;
    if (f.type == PropType::Dt_Time  ) return "TIMESTAMP";
    if (f.type == PropType::Tm_Stamp ) return "TEXT"     ;
    if (f.type == PropType::Bin      ) return "BLOB"     ;
    if (f.type == PropType::Object   ) {
        OrmProp *rf = f.ref_Field.lock().get();
        if (rf != nullptr) {
            if (rf->id_kind == IdKind::UUIDv7) {
                return "TEXT";
            } else {
                return "INTEGER";
            }
        } else {
            return "INTEGER";
        }
    }
    return "TEXT";
}

std::string SqliteDDLVisitor::visit(const OrmSchema& schema) {
    auto Q = [&](const std::string& id) -> std::string { return quote(id); };
    std::ostringstream ddl;
    ddl << "CREATE TABLE IF NOT EXISTS " << Q(schema.name) << "(\n";

    std::vector<std::string> pk_fields;
    size_t i = 0, n = schema.fields.size();

    for (const auto& kv : schema.fields) {
        const OrmProp& f = *kv.second;
        ddl << " " << Q(f.name) << " " << sql_type(f);
        if (f.required) ddl << " NOT NULL";
        if (f.is_unique) ddl << " UNIQUE";
        ddl << sql_default(f);
        if (f.is_id) pk_fields.push_back(Q(f.name));
        if (++i < n) ddl << ",\n";
        else ddl << "\n";
    }

    if (!pk_fields.empty()) {
        ddl << ",  PRIMARY KEY (";
        for (size_t j = 0; j < pk_fields.size(); ++j) {
            ddl << pk_fields[j];
            if (j + 1 < pk_fields.size()) ddl << ", ";
        }
        ddl << ")";
    }

    ddl << "\n);";

    // Per-field indexes
    for (const auto& kv : schema.fields) {
        const OrmProp& f = *kv.second;
        if (f.is_indexed && !f.is_id) {
            ddl << "\nCREATE ";
            if (f.is_unique) ddl << "UNIQUE ";
            ddl << "INDEX IF NOT EXISTS ";
            if (!f.index_name.empty()) {
                ddl << f.index_name << " " ;
            } else {
                ddl << "idx_" << f.name << " " ;
            }
            ddl << "ON " << Q(schema.name) << " (" << Q(f.name) << ");";
        }
    }

    // Schema-level composite indexes
    for (const auto& idx : schema.indexes) {
        ddl << "\nCREATE ";
        if (idx.unique) ddl << "UNIQUE ";
        ddl << "INDEX IF NOT EXISTS ";
        if (!idx.index_name.empty()) ddl << idx.index_name << " ";
        ddl << "ON " << Q(schema.name) << " (";
        for (size_t j = 0; j < idx.fields.size(); ++j) {
            ddl << idx.fields[j];
            if (j + 1 < idx.fields.size()) ddl << ", ";
        }
        ddl << ");";
    }

    ddl << std::endl;
    std::cout << "SqliteDDLVisitor::visit(): \n" << ddl.str();
    return ddl.str();
}

/* Utility for tests/output */
std::string PgDDLVisitor::generate_ddl(const OrmSchema& schema) {
    return visit(schema);
}
