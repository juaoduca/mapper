#include <catch.hpp>
#include "query/query_builder.hpp"
#include "orm.hpp"

// A tiny fake catalog for tests
static std::shared_ptr<OrmSchema> makeSchema(const std::string& name, std::shared_ptr<OrmSchema> parent,
                                             std::initializer_list<std::string> props) {
    auto s = std::make_shared<OrmSchema>();
    s->name = name;
    if (parent) s->parent = parent;
    for (auto& pn : props) {
        OrmProp p; p.name = pn; p.is_id = (pn == "ID");
        p.parent = s;
        s->fields.emplace(pn, std::make_shared<OrmProp>(std::move(p)));
    }
    return s;
}

// Helper function to create a Token from a TokenType and a string literal
static Token newtk(TokenType type, const char* value) {
    Token t;
    t.type = type;
    strncpy(t.value, value, TOKEN_SIZE - 1);
    t.value[TOKEN_SIZE - 1] = '\0'; // Ensure null-termination
    return t;
}

TEST_CASE("Query Compiler constructor","without getSchema() Function") {
    // Animal -> Canine -> Dog
    auto Animal = makeSchema("Animal", nullptr, {"ID","Age"});
    auto Canine = makeSchema("Canine", Animal, {});
    auto Dog    = makeSchema("Dog", Canine, {"Breed"});

    auto get = [&](const std::string& n)->std::shared_ptr<OrmSchema>{
     if (n=="Animal") return Animal;
     if (n=="Canine") return Canine;
     if (n=="Dog")    return Dog;
     return {};
    };

    ql::QueryParser qp("{ query{ field1 field2 field3}}");
    const ql::Doc doc = qp.parseDoc();
    ql::QueryBuilder qb(doc, get, Dialect::SQLite);
    std::string sql = qb.buildSelect();

    REQUIRE(!sql.empty());

}