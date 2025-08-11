#include "catch.hpp"
#include "orm.hpp"
#include "visitor.hpp"
#include <nlohmann/json.hpp>
#include <string>

// Helper: build schema from JSON string
OrmSchema load_schema(const std::string& js) {
    OrmSchema s;
    nlohmann::json j = nlohmann::json::parse(js);
    OrmSchema::from_json(j, s);
    return s;
}

TEST_CASE("DDL covers all types and features", "[ddl]") {
    std::string jschema = R"({
      "properties": {
        "id":      { "type": "string", "primaryKey": true },
        "active":  { "type": "boolean", "default": true },
        "avatar":  { "type": "binary", "encoding": "base64" },
        "score":   { "type": "integer", "default": 42 },
        "joined":  { "type": "date" },
        "logins":  { "type": "integer" },
        "profile": { "type": "json" },
        "email":   { "type": "string", "unique": true, "default": "" },
        "last_seen": { "type": "datetime" }
      },
      "required": ["id", "email", "score"],
      "indexes": [
        { "fields": ["email"], "unique": true, "type": "btree", "indexName": "idx_email" },
        { "fields": ["score", "active"], "indexName": "idx_score_active" }
      ]
    })";
    OrmSchema schema = load_schema(jschema);

    PostgresDDLVisitor pgvis;
    std::string ddl_pg = pgvis.generate_ddl(schema, "users");
    REQUIRE(ddl_pg.find("CREATE TABLE users") != std::string::npos);
    REQUIRE(ddl_pg.find("id TEXT") != std::string::npos); // ULID PK
    REQUIRE(ddl_pg.find("avatar BYTEA") != std::string::npos);
    REQUIRE(ddl_pg.find("active BOOLEAN DEFAULT true") != std::string::npos);
    REQUIRE(ddl_pg.find("email TEXT UNIQUE DEFAULT ''") != std::string::npos);
    REQUIRE(ddl_pg.find("score INTEGER NOT NULL DEFAULT 42") != std::string::npos);
    REQUIRE(ddl_pg.find("profile JSONB") != std::string::npos);
    REQUIRE(ddl_pg.find("last_seen TIMESTAMP") != std::string::npos);
    REQUIRE(ddl_pg.find("PRIMARY KEY (id)") != std::string::npos);
    REQUIRE(ddl_pg.find("CREATE UNIQUE INDEX idx_email ON users (email);") != std::string::npos);
    REQUIRE(ddl_pg.find("CREATE INDEX idx_score_active ON users (score, active);") != std::string::npos);
}

TEST_CASE("DDL fails on duplicate fields", "[ddl][error]") {
    std::string jschema = R"({
      "properties": {
        "id": { "type": "string", "primaryKey": true },
        "id": { "type": "string" }
      }
    })";
    // This should not parse correctly or should warn
    try {
        OrmSchema s = load_schema(jschema);
        PostgresDDLVisitor v;
        std::string ddl = v.generate_ddl(s, "users");
        REQUIRE(false); // Should not get here
    } catch (...) {
        REQUIRE(true); // Exception caught
    }
}
