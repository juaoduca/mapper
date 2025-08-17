#include <iostream>
#include "orm.hpp"
#include "ddl_visitor.hpp"
#include "schemamanager.hpp"
#include <fstream>
#include <nlohmann/json.hpp>

bool load_schema_from_file(const std::string &path, OrmSchema &schema)
{
    std::ifstream f(path);
    if (!f)
        return false;
    nlohmann::json j;
    f >> j;
    return OrmSchema::from_json(j, schema);
}

int main()
{
    OrmSchema old_schema, new_schema;
    if (!load_schema_from_file("../data/example-schema-old.json", old_schema))
    {
        std::cerr << "Failed to load old schema." << std::endl;
        return 1;
    }
    if (!load_schema_from_file("../data/example-schema-new.json", new_schema))
    {
        std::cerr << "Failed to load new schema." << std::endl;
        return 1;
    }

    std::cout << "[*] Old Schema:" << std::endl;
    DDLVisitor dumper_old;
    old_schema.accept(dumper_old);

    std::cout << "\n[*] New Schema:" << std::endl;
    DDLVisitor dumper_new;
    new_schema.accept(dumper_new);

    std::cout << "\n[*] Migration Plan (DDL diff):" << std::endl;
    SchemaManager mgr(old_schema, new_schema);
    auto ddls = mgr.plan_migration("postgres");
    for (const auto &sql : ddls)
    {
        std::cout << sql << std::endl;
    }

    std::cout << "\n[*] PostgreSQL DDL (for new schema):" << std::endl;
    PgDDLVisitor pg;
    new_schema.accept(pg);

    return 0;
}
