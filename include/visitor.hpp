#pragma once
#include "orm.hpp"
#include <iostream>
#include <sstream>

class OrmSchemaVisitor {
public:
    virtual ~OrmSchemaVisitor() = default;
    virtual void visit(const OrmSchema& schema);

public:
    void print_fields(const OrmSchema& schema) const;

    std::string sql_type(const OrmField& f, const std::string& db_engine) const;

    std::string sql_default(const OrmField& f) const;
};

class BaseDDLVisitor : public OrmSchemaVisitor {
public:
    std::string generate_ddl(const OrmSchema& schema, const std::string& table_name);
    void visit(const OrmSchema& schema) override;
};

class PostgresDDLVisitor : public OrmSchemaVisitor {
public:
    std::string generate_ddl(const OrmSchema& schema, const std::string& table_name);

    // Implement visit pattern:
    void visit(const OrmSchema& schema) override;

    // Use buffer_ for output, and table_name_ for the current table
private:
    std::ostringstream buffer_;
    std::string table_name_;
};

class SqliteDDLVisitor : public OrmSchemaVisitor {
public:
    std::string generate_ddl(const OrmSchema& schema, const std::string& table_name);
    void visit(const OrmSchema& schema) override;
};
