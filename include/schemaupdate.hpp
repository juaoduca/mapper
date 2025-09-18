#pragma once
#include "orm.hpp"
#include <vector>
#include <string>

class SchemaUpdate {
public:
    SchemaUpdate(const OrmSchema* old_schema, const OrmSchema& new_schema, std::shared_ptr<DDLVisitor> visitor);

    // Generates DDL migration scripts, returns as a list of SQL statements (strings)
    std::vector<std::string> plan_migration();

private:
    const OrmSchema* old_schema_;
    const OrmSchema& new_schema_;
    std::shared_ptr<DDLVisitor> visitor_;   // dialect-specific
};
