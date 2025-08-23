
#include "catch.hpp"
#include "schemaupdate.hpp"

OrmSchema load_schema_from_json(const std::string& js) {
    OrmSchema s;
    jdoc doc;
    jhlp::parse_str(js, doc);
    OrmSchema::from_json(doc, s);
    return s;
}

TEST_CASE("Add field") {
    auto old_schema = load_schema_from_json(R"({
      "properties": { "id": { "type": "integer", "primaryKey": true, "default": 0, "kind": "UUIDv7" } },
      "required": ["id"]
    })");
    auto new_schema = load_schema_from_json(R"({
      "properties": {
        "id": { "type": "integer", "primaryKey": true, "default": 0, "kind": "UUIDv7" },
        "active": { "type": "boolean", "default": true }
      },
      "required": ["id"]
    })");

    SchemaUpdate mgr(old_schema, new_schema);
    auto ddls = mgr.plan_migration("postgres");
    REQUIRE(ddls.size() == 1);
    REQUIRE(ddls[0].find("ADD COLUMN active") != std::string::npos);
}

TEST_CASE("Remove field") {
    auto old_schema = load_schema_from_json(R"({
      "properties": {
        "id": { "type": "integer", "primaryKey": true, "default": 0, "kind": "UUIDv7" },
        "active": { "type": "boolean", "default": true }
      },
      "required": ["id"]
    })");
    auto new_schema = load_schema_from_json(R"({
      "properties": { "id": { "type": "integer", "primaryKey": true, "default": 0, "kind": "UUIDv7" } },
      "required": ["id"]
    })");

    SchemaUpdate mgr(old_schema, new_schema);
    auto ddls = mgr.plan_migration("postgres");
    REQUIRE(ddls.size() == 1);
    REQUIRE(ddls[0].find("DROP COLUMN active") != std::string::npos);
}

TEST_CASE("Type change") {
    auto old_schema = load_schema_from_json(R"({
      "properties": { "id": { "type": "integer", "primaryKey": true, "default": 0, "kind": "UUIDv7" } },
      "required": ["id"]
    })");
    auto new_schema = load_schema_from_json(R"({
      "properties": { "id": { "type": "string", "primaryKey": true, "default": "0", "kind": "UUIDv7" } },
      "required": ["id"]
    })");

    SchemaUpdate mgr(old_schema, new_schema);
    auto ddls = mgr.plan_migration("postgres");
    REQUIRE(ddls[0].find("ALTER COLUMN id TYPE string") != std::string::npos);
}

TEST_CASE("Default value change") {
    auto old_schema = load_schema_from_json(R"({
      "properties": { "id": { "type": "integer", "default": 0, "kind": "UUIDv7" } }
    })");
    auto new_schema = load_schema_from_json(R"({
      "properties": { "id": { "type": "integer", "default": 42, "kind": "UUIDv7" } }
    })");

    SchemaUpdate mgr(old_schema, new_schema);
    auto ddls = mgr.plan_migration("postgres");
    REQUIRE(!ddls.empty());
    bool found = false;
    for (const auto& sql : ddls)
        if (sql.find("ALTER COLUMN id SET DEFAULT 42") != std::string::npos) found = true;
    REQUIRE(found);
}

// TEST_CASE("No changes = no DDL") {
//     auto old_schema = load_schema_from_json(R"({
//       "properties": { "id": { "type": "integer", "primaryKey": true, "kind": "UUIDv7" } }
//     })");
//     auto new_schema = old_schema;
//     SchemaUpdate mgr(old_schema, new_schema);
//     auto ddls = mgr.plan_migration("postgres");
//     REQUIRE(ddls.empty());
// }
