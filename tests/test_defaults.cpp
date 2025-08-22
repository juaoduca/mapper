#include "catch.hpp"
#include "ddl_visitor.hpp"
#include <string>

static OrmSchema load_schema_from_json(const std::string& js) {
    OrmSchema s;
    jdoc doc;
    jhlp::parse_str(js, doc);
    REQUIRE(OrmSchema::from_json(doc, s));
    return s;
}

TEST_CASE("sql_default emits correct SQL for all DefaultKind variants (Postgres & SQLite)", "[defaults]") {
    auto schema = load_schema_from_json(R"({
        "name": "users",
        "properties": {
            "id":    { "type": "string", "idprop": true, "idkind": "uuidv7" },
            "s":     { "type": "string",  "default": "abc" },
            "b":     { "type": "boolean", "default": true },
            "n":     { "type": "number",  "default": 42 },
            "t":     { "type": "string",  "default": "" },
            "rnull": { "type": "string",  "default": null }
        },
        "required": ["id"]
    })");

    PgDDLVisitor pg;
    auto ddl_pg = pg.visit(schema);
    REQUIRE(ddl_pg.find("CREATE TABLE IF NOT EXISTS users(") != std::string::npos);
    REQUIRE(ddl_pg.find("s TEXT") != std::string::npos);
    REQUIRE(ddl_pg.find("DEFAULT 'abc'") != std::string::npos);
    REQUIRE(ddl_pg.find("DEFAULT true") != std::string::npos);
    REQUIRE(ddl_pg.find("DEFAULT 42") != std::string::npos);
    REQUIRE(ddl_pg.find("DEFAULT NULL") != std::string::npos);
    REQUIRE(ddl_pg.find("DEFAULT ''") != std::string::npos);

    SqliteDDLVisitor sq;
    auto ddl_sq = sq.visit(schema);
    REQUIRE(ddl_sq.find("CREATE TABLE IF NOT EXISTS users(") != std::string::npos);
    REQUIRE(ddl_sq.find("DEFAULT 'abc'") != std::string::npos);
    // SQLite may accept TRUE/FALSE or 1/0; we normalized to literal true/false in OrmSchema::from_json
    REQUIRE(ddl_sq.find("DEFAULT true") != std::string::npos);
    REQUIRE(ddl_sq.find("DEFAULT 42") != std::string::npos);
    REQUIRE(ddl_sq.find("DEFAULT NULL") != std::string::npos);
    REQUIRE(ddl_pg.find("DEFAULT ''") != std::string::npos);
}
