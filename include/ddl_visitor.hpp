#pragma once
#include "orm.hpp"
#include <iostream>
#include <sstream>

class DDLVisitor {
public:
    virtual ~DDLVisitor() = default;
    virtual std::string visit(const OrmSchema& schema) = 0;
    virtual std::string sql_type(const OrmProp& f) = 0;
    virtual std::string sql_default(const OrmProp& f);
    virtual std::string quote(const std::string value) {return value;}; // return a identifier quoted or not
};

class PgDDLVisitor : public DDLVisitor {
public:
    std::string sql_type(const OrmProp& f) override;
    std::string generate_ddl(const OrmSchema& schema);
    std::string visit(const OrmSchema& schema) override;
    std::string quote(const std::string value) override { return "\""+value+"\"" ; }
private:
    std::ostringstream buffer_;
    std::string table_name_;
};

class SqliteDDLVisitor : public DDLVisitor {
public:
    std::string sql_type(const OrmProp& f) override;
    std::string generate_ddl(const OrmSchema& schema);
    std::string visit(const OrmSchema& schema) override;
};
