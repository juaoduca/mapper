#include "visitor.hpp"

std::string DDLVisitor::visit(const OrmSchema& schema) {
    print_fields(schema);
    return "";
}

void DDLVisitor::print_fields(const OrmSchema& schema) const {
    std::cout << "Fields in schema:" << std::endl;
    for (const auto& f : schema.fields) {
        std::cout << "  - " << f.name << " (" << f.type << ")";
        if (!f.encoding.empty())
            std::cout << " [encoding: " << f.encoding << "]";
        if (f.required)
            std::cout << " [required]";
        if (f.is_id)
            std::cout << " [PRIMARY KEY]";
        if (f.is_indexed)
            std::cout << " [index:" << (f.index_type.empty() ? "btree" : f.index_type) << "]";
        if (f.is_unique)
            std::cout << " [UNIQUE]";
        if (!f.index_name.empty())
            std::cout << " [indexName: " << f.index_name << "]";
        if (!f.default_value.empty())
            std::cout << " [default: " << f.default_value << "]";
        std::cout << std::endl;
    }
    if (!schema.indexes.empty()) {
        std::cout << "Indexes in schema:" << std::endl;
        for (const auto& idx : schema.indexes) {
            std::cout << "  - (";
            for (size_t i = 0; i < idx.fields.size(); ++i) {
                std::cout << idx.fields[i];
                if (i < idx.fields.size() - 1) std::cout << ", ";
            }
            std::cout << ") [type: " << (idx.type.empty() ? "btree" : idx.type) << "]";
            if (idx.unique) std::cout << " [UNIQUE]";
            if (!idx.index_name.empty())
                std::cout << " [indexName: " << idx.index_name << "]";
            std::cout << std::endl;
        }
    }
}

std::string DDLVisitor::sql_type(const OrmField& f) const {

    if (f.type == "string") return "TEXT";
    if (f.type == "integer") return "INTEGER";
    // if (f.type == "number") return (pgeng ? "NUMERIC" : "REAL");
    if (f.type == "boolean") return "BOOLEAN";
    // if (f.type == "json") return (pgeng ? "JSON" : "BLOB");
    if (f.type == "date") return "DATE";
    if (f.type == "time") return "TIME";
    if (f.type == "datetime") return "TIMESTAMP";
    // if (f.type == "timestamp") return (pgeng ? "TIMESTAMP WITH TIME ZONE" : "TEXT");
    if (f.type == "binary") return "BYTEA";
    return "TEXT";
}

static std::string sql_escape_single_quotes(const std::string& s) {
     std::string out; out.reserve(s.size() + 4);
     for (char c : s) out += (c == '\'') ? "''" : std::string(1, c);
     return out;
 }

 std::string DDLVisitor::sql_default(const OrmField& f) const {
    using DK = DefaultKind;
    switch (f.default_kind) {
        case DK::None:
            return "";
        case DK::String: {
            // Always single-quote; empty string becomes DEFAULT ''
            return " DEFAULT '" + sql_escape_single_quotes(f.default_value) + "'";
        }
        case DK::Boolean:
        case DK::Number:
            // Already normalized to true/false or numeric literal
            return " DEFAULT " + f.default_value;
        case DK::Raw:
            // Verbatim (e.g., NULL, JSON text if you allow it)
            return " DEFAULT " + f.default_value;
    }
    return "";
}

std::string PgDDLVisitor::sql_type(const OrmField& f) const {
    if (f.type == "string") return "TEXT";
    if (f.type == "integer") return "INTEGER";
    if (f.type == "number") return "NUMERIC";
    if (f.type == "boolean") return "BOOLEAN";
    if (f.type == "json") return "JSON";
    if (f.type == "date") return "DATE";
    if (f.type == "time") return "TIME";
    if (f.type == "datetime") return "TIMESTAMP";
    if (f.type == "timestamp") return "TIMESTAMP WITH TIME ZONE";
    if (f.type == "binary") return "BYTEA";
    return "TEXT";
}

std::string PgDDLVisitor::visit(const OrmSchema& schema) {
    std::ostringstream ddl;
    ddl << "CREATE TABLE "<< schema.name << "(\n";
    std::vector<std::string> pk_fields;
    for (size_t i = 0; i < schema.fields.size(); ++i) {
        const auto& f = schema.fields[i];
        ddl << "  " << f.name << " " << sql_type(f);
        if (f.required) ddl << " NOT NULL";
        if (f.is_unique) ddl << " UNIQUE";
        ddl << sql_default(f);
        if (f.is_id) pk_fields.push_back(f.name);
        if (i < schema.fields.size() - 1) ddl << ",\n";
        else ddl << "\n";
    }
    if (!pk_fields.empty()) {
        ddl << ",  PRIMARY KEY (";
        for (size_t i = 0; i < pk_fields.size(); ++i) {
            ddl << pk_fields[i];
            if (i < pk_fields.size() - 1) ddl << ", ";
        }
        ddl << ")";
    }
    ddl << "\n);";
    // Per-field indexes
    for (const auto& f : schema.fields) {
        if (f.is_indexed && !f.is_id) {
            ddl << "\nCREATE ";
            if (f.is_unique) ddl << "UNIQUE ";
            ddl << "INDEX ";
            if (!f.index_name.empty())
                ddl << f.index_name << " ";
            ddl << "ON users (" << f.name << ");";
        }
    }
    // Schema-level indexes
    for (const auto& idx : schema.indexes) {
        ddl << "\nCREATE ";
        if (idx.unique) ddl << "UNIQUE ";
        ddl << "INDEX ";
        if (!idx.index_name.empty())
            ddl << idx.index_name << " ";
        ddl << "ON users (";
        for (size_t i = 0; i < idx.fields.size(); ++i) {
            ddl << idx.fields[i];
            if (i < idx.fields.size() - 1) ddl << ", ";
        }
        ddl << ");";
    }

    ddl << std::endl;
    std::cout << "PgDDLVisitor::visit(): " << ddl.str();
    return ddl.str();
}

std::string SqliteDDLVisitor::sql_type(const OrmField& f) const {
    if (f.type == "string") return "TEXT";
    if (f.type == "integer") return "INTEGER";
    if (f.type == "number") return "REAL";
    if (f.type == "boolean") return "BOOLEAN";
    if (f.type == "json") return "BLOB";
    if (f.type == "date") return "DATE";
    if (f.type == "time") return "TIME";
    if (f.type == "datetime") return "TIMESTAMP";
    if (f.type == "timestamp") return "TEXT"; // can handle the time zone
    if (f.type == "binary") return "BYTEA";
    return "TEXT";
}

std::string SqliteDDLVisitor::visit(const OrmSchema& schema) {
    std::ostringstream ddl;
    ddl << "CREATE TABLE "<< schema.name << "(\n";
    std::vector<std::string> pk_fields;
    for (size_t i = 0; i < schema.fields.size(); ++i) {
        const auto& f = schema.fields[i];
        ddl << "  " << f.name << " " << sql_type(f);
        if (f.required) ddl << " NOT NULL";
        if (f.is_unique) ddl << " UNIQUE";
        ddl << sql_default(f);
        if (f.is_id) pk_fields.push_back(f.name);
        if (i < schema.fields.size() - 1) ddl << ",\n";
        else ddl << "\n";
    }
    if (!pk_fields.empty()) {
        ddl << ",  PRIMARY KEY (";
        for (size_t i = 0; i < pk_fields.size(); ++i) {
            ddl << pk_fields[i];
            if (i < pk_fields.size() - 1) ddl << ", ";
        }
        ddl << ")";
    }
    ddl << "\n);";
    // Per-field indexes
    for (const auto& f : schema.fields) {
        if (f.is_indexed && !f.is_id) {
            ddl << "\nCREATE ";
            if (f.is_unique) ddl << "UNIQUE ";
            ddl << "INDEX ";
            if (!f.index_name.empty())
                ddl << f.index_name << " ";
            ddl << "ON users (" << f.name << ");";
        }
    }
    // Schema-level indexes
    for (const auto& idx : schema.indexes) {
        ddl << "\nCREATE ";
        if (idx.unique) ddl << "UNIQUE ";
        ddl << "INDEX ";
        if (!idx.index_name.empty())
            ddl << idx.index_name << " ";
        ddl << "ON users (";
        for (size_t i = 0; i < idx.fields.size(); ++i) {
            ddl << idx.fields[i];
            if (i < idx.fields.size() - 1) ddl << ", ";
        }
        ddl << ");";
    }
    ddl << std::endl;
    std::cout << "SqliteDDLVisitor::visit(): " << ddl.str();
    return ddl.str();
}


// Generate DDL for tests or output
std::string PgDDLVisitor::generate_ddl(const OrmSchema& schema) {
    return visit(schema);
}