#pragma once
#include "orm.hpp"
#include <iostream>
#include <sstream>

class SQLVisitor {
public:
    virtual ~SQLVisitor() = default;
    virtual std::string visit(const void* aSchema) = 0;
};

class DDLVisitor: public SQLVisitor {
public:
    std::string visit(const void* aSchema) override;

    void print_fields(const OrmSchema& schema) const;
    std::string sql_type(const OrmField& f) const;
    std::string sql_default(const OrmField& f) const;

};


class PgDDLVisitor : public DDLVisitor {
public:
    std::string sql_type(const OrmField& f) const;
    std::string generate_ddl(const OrmSchema& schema);
    std::string visit(const void* aSchema) override;
private:
    std::ostringstream buffer_;
    std::string table_name_;
};

class SqliteDDLVisitor : public DDLVisitor {
public:
    std::string sql_type(const OrmField& f) const;
    std::string generate_ddl(const OrmSchema& schema);
    std::string  visit(const void* aSchema) override;
};

class DMLVisitor: public SQLVisitor {
public:
    std::string  visit(const void* aSchema) override;
};

class QRYVisitor: public SQLVisitor {
public:
    std::string  visit(const void* aSchema) override;
};
