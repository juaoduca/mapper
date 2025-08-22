#include "catch.hpp"
// #include "orm.hpp"
#include "ddl_visitor.hpp"
#include <string>

static OrmSchema load_schema_from_json(const std::string& js) {
    OrmSchema s{};
    jdoc doc;
    jhlp::parse_str(js, doc);
    REQUIRE(OrmSchema::from_json(doc, s));
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
    REQUIRE(ddl_pg.find("CREATE TABLE IF NOT EXISTS orders(") != std::string::npos);

    SqliteDDLVisitor sq;
    auto ddl_sq = sq.visit(schema);
    REQUIRE(ddl_sq.find("CREATE TABLE IF NOT EXISTS orders(") != std::string::npos);
}
