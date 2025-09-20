#include <catch.hpp>
#include "query/query_compiler.hpp"
#include "query/select_builder.hpp"
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

TEST_CASE("SelectBuilder Aggregations", "PG_SQL_AGG") {
    // Animal -> Canine -> Dog
    auto Animal = makeSchema("Animal", nullptr, {"ID","Age"});
    auto Canine = makeSchema("Canine", Animal, {});
    auto Dog    = makeSchema("Dog", Canine, {"Breed"});

    SelectBuilder::GetSchemaFn get = [&](const std::string& n)->std::shared_ptr<OrmSchema>{
        if (n=="Animal") return Animal;
        if (n=="Canine") return Canine;
        if (n=="Dog")    return Dog;
        return {};
    };

    // Query with aggregation & group by
    const char* doc = "{ Dog { breed.groupby , dog_per_breed:id.count , average_age:age.avg } }";
    ql::QueryCompiler qc;
    auto ast = qc.compile(doc);

    SelectBuilder sb(get, Dialect::Postgres);
    auto sql = sb.build_sql(ast);

    // FROM/JOIN chain
    REQUIRE(sql.find("FROM \"Animal\" t0\nJOIN \"Canine\" t1 ON (t0.\"ID\" = t1.\"ID\")") != std::string::npos);
    REQUIRE(sql.find("JOIN \"Dog\" t2 ON (t1.\"ID\" = t2.\"ID\")") != std::string::npos);

    // JSON object with aggregates (case-preserved keys)
    REQUIRE(sql.find("jsonb_build_object('breed', t2.\"Breed\", 'dog_per_breed', count(*), 'average_age', avg(t0.\"Age\"))") != std::string::npos);

    // Group by column expression
    REQUIRE(sql.find("GROUP BY t2.\"Breed\"") != std::string::npos);

    // Outer JSON array aggregation
    REQUIRE(sql.find("SELECT jsonb_agg(obj) AS data") != std::string::npos);
}

TEST_CASE("QueryCompiler", "Minimal") {
    const char* doc = "{ Golden_Retriever { Name, Age, Breed } }";
    ql::QueryCompiler qc;
    auto ast = qc.compile(doc);
    REQUIRE(ast.rootTypeName == "Golden_Retriever");
    REQUIRE(ast.selectionSet.size()  == 3u);
    REQUIRE(ast.selectionSet[0].name == "Name");
    REQUIRE(ast.selectionSet[1].name == "Age");
    REQUIRE(ast.selectionSet[2].name == "Breed");
}

TEST_CASE("SelectBuilder", "PG_SQL") {
    // build chain: Animal -> Canine -> Dog -> Golden_Retriever
    auto Animal = makeSchema("Animal", nullptr, {"ID","Name","Age"});
    auto Canine = makeSchema("Canine", Animal, {});
    auto Dog    = makeSchema("Dog", Canine, {"Breed"});
    auto Gold   = makeSchema("Golden_Retriever", Dog, {});

    SelectBuilder::GetSchemaFn get = [&](const std::string& n)->std::shared_ptr<OrmSchema>{
        if (n=="Animal") return Animal;
        if (n=="Canine") return Canine;
        if (n=="Dog") return Dog;
        if (n=="Golden_Retriever") return Gold;
        return {};
    };

    const char* doc = "{ Golden_Retriever { Name, Age, Breed } }";
    ql::QueryCompiler qc;
    auto ast = qc.compile(doc);

    SelectBuilder sb(get, Dialect::Postgres);
    auto sql = sb.build_sql(ast);

    // sanity checks
    REQUIRE(sql.find("FROM \"Animal\" t0\nJOIN \"Canine\" t1 ON (t0.\"ID\" = t1.\"ID\")") != std::string::npos);
    REQUIRE(sql.find("JOIN \"Dog\" t2 ON (t1.\"ID\" = t2.\"ID\")") !=  std::string::npos);
    REQUIRE(sql.find("JOIN \"Golden_Retriever\" t3 ON (t2.\"ID\" = t3.\"ID\")") !=  std::string::npos);
    REQUIRE(sql.find("jsonb_build_object('Name', t0.\"Name\", 'Age', t0.\"Age\", 'Breed', t2.\"Breed\")") !=  std::string::npos);
    REQUIRE(sql.find("SELECT jsonb_agg(obj) AS data") !=  std::string::npos);
}

TEST_CASE("SelectBuilder2", "SQLITE_SQL") {
    // same chain
    auto Animal = makeSchema("Animal", nullptr, {"ID","Name"});
    auto Dog = makeSchema("Dog", Animal, {"Tag"});
    SelectBuilder::GetSchemaFn get = [&](const std::string& n)->std::shared_ptr<OrmSchema>{
        if (n=="Animal") return Animal;
        if (n=="Dog") return Dog;
        return {};
    };

    const char* doc = "{ Dog { Name, Tag } }";
    ql::QueryCompiler qc;
    auto ast = qc.compile(doc);

    SelectBuilder sb(get, Dialect::SQLite);
    auto sql = sb.build_sql(ast);

    REQUIRE(sql.find("FROM \"Animal\" t0\nJOIN \"Dog\" t1 ON (t0.\"ID\" = t1.\"ID\")") != std::string::npos);
    REQUIRE(sql.find("json_object('Name', t0.\"Name\", 'Tag', t1.\"Tag\")") !=  std::string::npos);
    REQUIRE(sql.find("SELECT json_group_array(obj) AS data") !=  std::string::npos);
}


TEST_CASE("SelectBuilder Aggregations last", "PG_SQL_AGG") {
    // Animal -> Canine -> Dog
    auto Animal = makeSchema("Animal", nullptr, {"ID","Age"});
    auto Canine = makeSchema("Canine", Animal, {});
    auto Dog    = makeSchema("Dog", Canine, {"Breed"});

    SelectBuilder::GetSchemaFn get = [&](const std::string& n)->std::shared_ptr<OrmSchema>{
        if (n=="Animal") return Animal;
        if (n=="Canine") return Canine;
        if (n=="Dog")    return Dog;
        return {};
    };

    // Query with aggregation & group by
    const char* doc = "{ Dog { breed.groupby , dog_per_breed:id.count , average_age:age.avg } }";
    ql::QueryCompiler qc;
    auto ast = qc.compile(doc);

    SelectBuilder sb(get, Dialect::Postgres);
    auto sql = sb.build_sql(ast);

    // FROM/JOIN chain
    REQUIRE(sql.find("FROM \"Animal\" t0\nJOIN \"Canine\" t1 ON (t0.\"ID\" = t1.\"ID\")") != std::string::npos);
    REQUIRE(sql.find("JOIN \"Dog\" t2 ON (t1.\"ID\" = t2.\"ID\")") != std::string::npos);

    // JSON object with aggregates (case-preserved keys)
    REQUIRE(sql.find("jsonb_build_object('breed', t2.\"Breed\", 'dog_per_breed', count(*), 'average_age', avg(t0.\"Age\"))") != std::string::npos);

    // Group by column expression
    REQUIRE(sql.find("GROUP BY t2.\"Breed\"") != std::string::npos);

    // Outer JSON array aggregation
    REQUIRE(sql.find("SELECT jsonb_agg(obj) AS data") != std::string::npos);
}

// Helpers to build schemas with explicit types
static std::shared_ptr<OrmSchema> makeSchemaT(const std::string& name, std::shared_ptr<OrmSchema> parent,
                                              std::initializer_list<std::pair<std::string,PropType>> props) {
    auto s = std::make_shared<OrmSchema>();
    s->name = name;
    if (parent) s->parent = parent;
    for (auto& kv : props) {
        OrmProp p; p.name = kv.first; p.is_id = (kv.first == "ID");
        p.type = kv.second;
        p.parent = s;
        s->fields.emplace(kv.first, std::make_shared<OrmProp>(std::move(p)));
    }
    return s;
}

TEST_CASE("DateTime funcs - Postgres", "[dtfuncs][pg]") {
    auto Animal = makeSchemaT("Animal", nullptr, {{"ID",PropType::Integer}, {"BornAt",PropType::Dt_Time}, {"Name",PropType::String}});
    auto Dog    = makeSchemaT("Dog", Animal, {{"Breed",PropType::String}});
    SelectBuilder::GetSchemaFn get = [&](const std::string& n)->std::shared_ptr<OrmSchema>{
        if (n=="Animal") return Animal;
        if (n=="Dog") return Dog;
        return {};
    };
    const char* doc = "{ Dog { Only_Date:BornAt.date, Ym:BornAt.ym, Ns:BornAt.ns, Breed } }";
    std::cout << doc << std::endl;
    ql::QueryCompiler qc;
    auto ast = qc.compile(doc);
    SelectBuilder sb(get, Dialect::Postgres);
    auto sql = sb.build_sql(ast);
    REQUIRE(sql.find("to_char(t0.\"BornAt\", 'YYYY-MM-DD')") != std::string::npos);
    REQUIRE(sql.find("to_char(t0.\"BornAt\", 'YYYY-MM')") != std::string::npos);
    REQUIRE(sql.find("to_char(t0.\"BornAt\", 'MI:SS')") != std::string::npos);
    REQUIRE(sql.find("Only_Date") != std::string::npos);
    REQUIRE(sql.find("Ym") != std::string::npos);
    REQUIRE(sql.find("Ns") != std::string::npos);
}

TEST_CASE("DateTime funcs - SQLite", "[dtfuncs][sqlite]") {
    auto Animal = makeSchemaT("Animal", nullptr, {{"ID",PropType::Integer}, {"When",PropType::Dt_Time}});
    auto Dog    = makeSchemaT("Dog", Animal, {{"Breed",PropType::String}});
    SelectBuilder::GetSchemaFn get = [&](const std::string& n)->std::shared_ptr<OrmSchema>{
        if (n=="Animal") return Animal;
        if (n=="Dog") return Dog;
        return {};
    };
    const char* doc = "{ Dog { Time_Ms:When.timems, NdH:When.mdh } }";
    std::cout << doc << std::endl;
    ql::QueryCompiler qc;
    auto ast = qc.compile(doc);
    SelectBuilder sb(get, Dialect::SQLite);
    auto sql = sb.build_sql(ast);
    REQUIRE(sql.find("strftime('%H:%M:%f', t0.\"When\")") != std::string::npos);
    REQUIRE(sql.find("strftime('%m-%dT%H', t0.\"When\")") != std::string::npos);
    REQUIRE(sql.find("Time_Ms") != std::string::npos);
    REQUIRE(sql.find("NdH") != std::string::npos);
}