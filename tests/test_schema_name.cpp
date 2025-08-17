#include "catch.hpp"
#include "orm.hpp"
#include "ddl_visitor.hpp"
#include <nlohmann/json.hpp>
#include <string>

static OrmSchema load_schema_from_json(const std::string& js) {
    OrmSchema s{};
    auto j = nlohmann::json::parse(js);
    REQUIRE(OrmSchema::from_json(j, s));
    return s;
}

TEST_CASE("Schema name is picked from 'name' and used in CREATE TABLE", "[name]") {
    auto schema = load_schema_from_json(R"({
        "name": "orders",
        "properties": {
            "id": { "type": "integer", "primaryKey": true }
        },
        "required": ["id"]
    })");

    PgDDLVisitor pg;
    auto ddl_pg = pg.generate_ddl(schema);
    REQUIRE(ddl_pg.find("CREATE TABLE orders(") != std::string::npos);

    SqliteDDLVisitor sq;
    auto ddl_sq = sq.visit(schema);
    REQUIRE(ddl_sq.find("CREATE TABLE orders(") != std::string::npos);
}
