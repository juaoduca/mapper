#include "catch.hpp"
#include "orm.hpp"
#include "visitor.hpp"
#include <nlohmann/json.hpp>
#include <string>

static OrmSchema load_schema_from_json(const std::string& js) {
    OrmSchema s{};
    auto j = nlohmann::json::parse(js);
    REQUIRE(OrmSchema::from_json(j, s));
    return s;
}

TEST_CASE("sql_default emits correct SQL for all DefaultKind variants (Postgres & SQLite)", "[defaults]") {
    auto schema = load_schema_from_json(R"({
        "name": "users",
        "properties": {
            "id":    { "type": "integer", "primaryKey": true },
            "s":     { "type": "string",  "default": "abc" },
            "b":     { "type": "boolean", "default": true },
            "n":     { "type": "number",  "default": 42 },
            "t":     { "type": "string",  "default": "" },
            "rnull": { "type": "string",  "default": null }
        },
        "required": ["id"]
    })");

    PgDDLVisitor pg;
    auto ddl_pg = pg.visit(static_cast<const void*>(&schema));
    REQUIRE(ddl_pg.find("CREATE TABLE users(") != std::string::npos);
    REQUIRE(ddl_pg.find("s text") != std::string::npos || ddl_pg.find("s varchar") != std::string::npos || ddl_pg.find("s ") != std::string::npos);
    REQUIRE(ddl_pg.find("DEFAULT 'abc'") != std::string::npos);
    REQUIRE(ddl_pg.find("DEFAULT true") != std::string::npos);
    REQUIRE(ddl_pg.find("DEFAULT 42") != std::string::npos);
    REQUIRE(ddl_pg.find("DEFAULT NULL") != std::string::npos);
    REQUIRE(ddl_pg.find("DEFAULT ''") != std::string::npos);

    SqliteDDLVisitor sq;
    auto ddl_sq = sq.visit(static_cast<const void*>(&schema));
    REQUIRE(ddl_sq.find("CREATE TABLE users(") != std::string::npos);
    REQUIRE(ddl_sq.find("DEFAULT 'abc'") != std::string::npos);
    // SQLite may accept TRUE/FALSE or 1/0; we normalized to literal true/false in OrmSchema::from_json
    REQUIRE(ddl_sq.find("DEFAULT true") != std::string::npos);
    REQUIRE(ddl_sq.find("DEFAULT 42") != std::string::npos);
    REQUIRE(ddl_sq.find("DEFAULT NULL") != std::string::npos);
    REQUIRE(ddl_pg.find("DEFAULT ''") != std::string::npos);
}
