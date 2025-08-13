#include "visitor.hpp"

std::string OrmSchemaVisitor::visit(const OrmSchema& schema) {
    // print_fields(schema);
    return "";
}

void OrmSchemaVisitor::print_fields(const OrmSchema& schema) const {
    std::cout << "Fields in schema:" << std::endl;
    for (const auto& f : schema.fields) {
        std::cout << "  - " << f.name << " (" << f.type << ")";
        if (!f.encoding.empty())
            std::cout << " [encoding: " << f.encoding << "]";
        if (f.required)
            std::cout << " [required]";
        if (f.is_primary_key)
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

std::string OrmSchemaVisitor::sql_type(const OrmField& f, const std::string& db_engine) const {
    if (f.type == "string") return "TEXT";
    if (f.type == "integer") return "INTEGER";
    if (f.type == "number") return "DOUBLE PRECISION";
    if (f.type == "boolean") return "BOOLEAN";
    if (f.type == "json") return (db_engine == "postgres" ? "JSONB" : "JSON");
    if (f.type == "date") return "DATE";
    if (f.type == "time") return "TIME";
    if (f.type == "datetime") return "TIMESTAMP";
    if (f.type == "timestamp") return (db_engine == "postgres" ? "TIMESTAMP WITH TIME ZONE" : "TEXT");
    if (f.type == "binary") return "BYTEA";
    return "TEXT";
}

std::string OrmSchemaVisitor::sql_default(const OrmField& f) const {
    if (f.default_value.empty()) return "";
    if (f.type == "string" && f.default_value.length() > 2) {
        // Remove leading/trailing double quotes (from JSON dump), use single quotes for SQL
        return " DEFAULT '" + f.default_value.substr(1, f.default_value.length() - 2) + "'";
    } else {
        return " DEFAULT " + f.default_value;
    }
}

std::string BaseDDLVisitor::visit(const OrmSchema& schema) {
    // print_fields(schema);
    return "";
}

std::string PostgresDDLVisitor::visit(const OrmSchema& schema) {
    std::ostringstream ddl;
    const std::string& table = schema.name;   // <-- use the new property
    ddl << "CREATE TABLE "<< table << "(\n";
    std::vector<std::string> pk_fields;
    for (size_t i = 0; i < schema.fields.size(); ++i) {
        const auto& f = schema.fields[i];
        ddl << "  " << f.name << " " << sql_type(f, "postgres");
        if (f.required) ddl << " NOT NULL";
        if (f.is_unique) ddl << " UNIQUE";
        ddl << sql_default(f);
        if (f.is_primary_key) pk_fields.push_back(f.name);
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
        if (f.is_indexed && !f.is_primary_key) {
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
    std::cout << "PostgresDDLVisitor::visit(): " << ddl.str();
    return ddl.str();
}

std::string SqliteDDLVisitor::visit(const OrmSchema& schema) {
    std::ostringstream ddl;
    const std::string& table = schema.name;   // <-- use the new property
    ddl << "CREATE TABLE "<< table << "(\n";
    std::vector<std::string> pk_fields;
    for (size_t i = 0; i < schema.fields.size(); ++i) {
        const auto& f = schema.fields[i];
        ddl << "  " << f.name << " " << sql_type(f, "sqlite3");
        if (f.required) ddl << " NOT NULL";
        if (f.is_unique) ddl << " UNIQUE";
        ddl << sql_default(f);
        if (f.is_primary_key) pk_fields.push_back(f.name);
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
        if (f.is_indexed && !f.is_primary_key) {
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
std::string PostgresDDLVisitor::generate_ddl(const OrmSchema& schema) {
    return visit(schema);     // Call visit, which fills buffer_;
}