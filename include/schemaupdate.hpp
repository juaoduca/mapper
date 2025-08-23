#pragma once
#include "orm.hpp"
#include <vector>
#include <string>

class SchemaUpdate {
public:
    SchemaUpdate(const OrmSchema& old_schema, const OrmSchema& new_schema);

    // Generates DDL migration scripts, returns as a list of SQL statements (strings)
    std::vector<std::string> plan_migration(const std::string& db_engine);

private:
    const OrmSchema& old_schema_;
    const OrmSchema& new_schema_;
};
