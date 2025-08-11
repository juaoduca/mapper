#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include "orm.hpp"
#include "schemamanager.hpp"
#include <nlohmann/json.hpp>

OrmSchema load_schema_from_json(const std::string& js) {
    OrmSchema s;
    nlohmann::json j = nlohmann::json::parse(js);
    OrmSchema::from_json(j, s);
    return s;
}

TEST_CASE("Add field") {
    auto old_schema = load_schema_from_json(R"({
      "properties": { "id": { "type": "integer", "primaryKey": true, "default": 0 } },
      "required": ["id"]
    })");
    auto new_schema = load_schema_from_json(R"({
      "properties": {
        "id": { "type": "integer", "primaryKey": true, "default": 0 },
        "active": { "type": "boolean", "default": true }
      },
      "required": ["id"]
    })");

    SchemaManager mgr(old_schema, new_schema);
    auto ddls = mgr.plan_migration("postgres");
    REQUIRE(ddls.size() == 1);
    REQUIRE(ddls[0].find("ADD COLUMN active") != std::string::npos);
}

TEST_CASE("Remove field") {
    auto old_schema = load_schema_from_json(R"({
      "properties": {
        "id": { "type": "integer", "primaryKey": true, "default": 0 },
        "active": { "type": "boolean", "default": true }
      },
      "required": ["id"]
    })");
    auto new_schema = load_schema_from_json(R"({
      "properties": { "id": { "type": "integer", "primaryKey": true, "default": 0 } },
      "required": ["id"]
    })");

    SchemaManager mgr(old_schema, new_schema);
    auto ddls = mgr.plan_migration("postgres");
    REQUIRE(ddls.size() == 1);
    REQUIRE(ddls[0].find("DROP COLUMN active") != std::string::npos);
}

TEST_CASE("Type change") {
    auto old_schema = load_schema_from_json(R"({
      "properties": { "id": { "type": "integer", "primaryKey": true, "default": 0 } },
      "required": ["id"]
    })");
    auto new_schema = load_schema_from_json(R"({
      "properties": { "id": { "type": "string", "primaryKey": true, "default": "0" } },
      "required": ["id"]
    })");

    SchemaManager mgr(old_schema, new_schema);
    auto ddls = mgr.plan_migration("postgres");
    REQUIRE(ddls[0].find("ALTER COLUMN id TYPE string") != std::string::npos);
}

TEST_CASE("Default value change") {
    auto old_schema = load_schema_from_json(R"({
      "properties": { "id": { "type": "integer", "default": 0 } }
    })");
    auto new_schema = load_schema_from_json(R"({
      "properties": { "id": { "type": "integer", "default": 42 } }
    })");

    SchemaManager mgr(old_schema, new_schema);
    auto ddls = mgr.plan_migration("postgres");
    REQUIRE(!ddls.empty());
    bool found = false;
    for (const auto& sql : ddls)
        if (sql.find("ALTER COLUMN id SET DEFAULT 42") != std::string::npos) found = true;
    REQUIRE(found);
}

TEST_CASE("No changes = no DDL") {
    auto old_schema = load_schema_from_json(R"({
      "properties": { "id": { "type": "integer", "primaryKey": true } }
    })");
    auto new_schema = old_schema;
    SchemaManager mgr(old_schema, new_schema);
    auto ddls = mgr.plan_migration("postgres");
    REQUIRE(ddls.empty());
}
