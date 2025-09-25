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

// Simple query: { User { id } }
TEST_CASE("Simple Single Field Query", "[query_builder]") {
    auto User = makeSchema("User", nullptr, {"id", "name"});

    auto getSchema = [&](const std::string& n) -> std::shared_ptr<OrmSchema> {
        if (n == "User") return User;
        return nullptr;
    };

    // Mock Doc with simple query
    ql::Doc doc;
    doc.id = "test_simple";
    ql::Query q;
    q.name = "User";
    ql::Field f;
    f.name = "id";
    q.fields = {f};
    doc.queries = {q};

    ql::QueryBuilder qb(doc, getSchema, Dialect::SQLite);
    std::string sql = qb.buildSelect();
    std::string expected_sqlite = "SELECT json_group_array(json_object('id', User.id)) FROM User";
    REQUIRE(sql == expected_sqlite);

    ql::QueryBuilder qp(doc, getSchema, Dialect::Postgres);
    sql = qp.buildSelect();
    std::string expected_pg = "SELECT json_agg(row_to_json(t.*)) FROM (SELECT 'id', User.id) t";  // Simplified; adjust per exact output
    REQUIRE(sql.find("json_agg(row_to_json") != std::string::npos);  // Partial match for PG
}

// Query with args: { User(name: \"Alice\") { id } } → WHERE name = 'Alice'
TEST_CASE("Query with Arguments", "[query_builder]") {
    auto User = makeSchema("User", nullptr, {"id", "name"});

    auto getSchema = [&](const std::string& n) -> std::shared_ptr<OrmSchema> {
        if (n == "User") return User;
        return nullptr;
    };

    ql::Doc doc;
    doc.id = "test_args";
    ql::Query q;
    q.name = "User";
    ql::QueryArg arg;
    arg.lvalue.name = "name"; arg.comparator = newtk(ttEQUALS, "=");  // Mock Token
    arg.rvalue.name = "Alice"; arg.rvalue.type = ttSTRING;
    q.args = {arg};
    ql::Field f; f.name = "id";
    q.fields = {f};
    doc.queries = {q};

    ql::QueryBuilder qb(doc, getSchema, Dialect::SQLite);
    std::string sql = qb.buildSelect();
    std::string expected = "SELECT json_group_array(json_object('id', User.id)) FROM User WHERE name = 'Alice'";
    REQUIRE(sql == expected);

    // Repeat for Postgres (similar WHERE)
    ql::QueryBuilder qp(doc, getSchema, Dialect::Postgres);
    sql = qp.buildSelect();
    REQUIRE(sql.find("WHERE name = 'Alice'") != std::string::npos);
}

// Nested query: { User { id, posts { title } } } → LEFT JOIN
TEST_CASE("Nested Query with Join", "[query_builder]") {
    auto User = makeSchema("User", nullptr, {"id"});
    auto Post = makeSchema("Post", nullptr, {"id", "title", "user_id"});  // FK to User.id
    // Mock FK: Post.fields["user_id"]->ref_Schema = User; ref_Field = User->fields["id"]

    auto getSchema = [&](const std::string& n) -> std::shared_ptr<OrmSchema> {
        if (n == "User") return User;
        if (n == "Post") return Post;
        return nullptr;
    };

    ql::Doc doc;
    doc.id = "test_nested";
    ql::Query q; q.name = "User";
    ql::Field userField; userField.name = "id";
    ql::Field postField; postField.name = "posts";
    ql::Field titleField; titleField.name = "title";
    postField.fields = {titleField};
    q.fields = {userField, postField};
    doc.queries = {q};

    ql::QueryBuilder qb(doc, getSchema, Dialect::SQLite);
    std::string sql = qb.buildSelect();
    std::string expected = "SELECT json_group_array(json_object('id', User.id, 'posts', (SELECT ... FROM Post LEFT JOIN ...))) FROM User LEFT JOIN Post ON User.id = Post.user_id";
    REQUIRE(sql.find("LEFT JOIN Post") != std::string::npos);

    // Postgres variant
    ql::QueryBuilder qp(doc, getSchema, Dialect::Postgres);
    sql = qp.buildSelect();
    REQUIRE(sql.find("LEFT JOIN Post") != std::string::npos);
}

// Function call: { User { count: id.count } } → COUNT(id)
TEST_CASE("Field Function (Aggregate)", "[query_builder]") {
    auto User = makeSchema("User", nullptr, {"id"});

    auto getSchema = [&](const std::string& n) -> std::shared_ptr<OrmSchema> {
        if (n == "User") return User;
        return nullptr;
    };

    ql::Doc doc;
    doc.id = "test_func";
    ql::Query q; q.name = "User";
    ql::Field f; f.name = "id"; f.alias = "count";
    ql::FieldFunc fc; fc.name = "count";
    f.funcs = {fc};
    q.fields = {f};
    doc.queries = {q};

    ql::QueryBuilder qb(doc, getSchema, Dialect::SQLite);
    std::string sql = qb.buildSelect();
    REQUIRE(sql.find("COUNT(User.id)") != std::string::npos);
    REQUIRE(sql.find("GROUP BY") != std::string::npos);

    // Postgres
    ql::QueryBuilder qp(doc, getSchema, Dialect::Postgres);
    sql = qp.buildSelect();
    REQUIRE(sql.find("COUNT(User.id)") != std::string::npos);
}

// Scale: Self-join with level(2): { self { id, self.level(2) { id } } }
TEST_CASE("Self-Join with Level", "[query_builder]") {
    auto Self = makeSchema("Self", nullptr, {"id", "parent_id"});  // Self-ref FK

    auto getSchema = [&](const std::string& n) -> std::shared_ptr<OrmSchema> {
        if (n == "Self") return Self;
        return nullptr;
    };

    ql::Doc doc;
    doc.id = "test_self";
    ql::Query q; q.name = "Self";
    ql::Field idField; idField.name = "id";
    ql::Field selfField; selfField.name = "self";
    ql::FieldFunc levelFunc; levelFunc.name = "level"; levelFunc.args.push_back(newtk(ttINT, "2"));
    selfField.funcs = {levelFunc};
    ql::Field nestedId; nestedId.name = "id";
    selfField.fields = {nestedId};
    q.fields = {idField, selfField};
    doc.queries = {q};

    ql::QueryBuilder qb(doc, getSchema, Dialect::SQLite);
    std::string sql = qb.buildSelect();
    // Expect multiple JOINs for level 2
    REQUIRE(sql.find("JOIN Self Self_1 ON") != std::string::npos);
    REQUIRE(sql.find("JOIN Self Self_2 ON") != std::string::npos);

    // Postgres similar
    ql::QueryBuilder qp(doc, getSchema, Dialect::Postgres);
    sql = qp.buildSelect();
    REQUIRE(sql.find("JOIN Self Self_1") != std::string::npos);
}

// Parent chain: Child (parent: Parent) → JOIN Parent ON Child.parent_id = Parent.id
TEST_CASE("Parent Chain Join", "[query_builder]") {
    auto Parent = makeSchema("Parent", nullptr, {"id"});
    auto Child = makeSchema("Child", Parent, {"id", "parent_id"});  // Inherits join via parent

    auto getSchema = [&](const std::string& n) -> std::shared_ptr<OrmSchema> {
        if (n == "Parent") return Parent;
        if (n == "Child") return Child;
        return nullptr;
    };

    ql::Doc doc;
    doc.id = "test_parent";
    ql::Query q; q.name = "Child";
    ql::Field f; f.name = "id";
    q.fields = {f};
    doc.queries = {q};

    ql::QueryBuilder qb(doc, getSchema, Dialect::SQLite);
    std::string sql = qb.buildSelect();
    REQUIRE(sql.find("JOIN Parent ON Child.parent_id = Parent.id") != std::string::npos);

    // Postgres
    ql::QueryBuilder qp(doc, getSchema, Dialect::Postgres);
    sql = qp.buildSelect();
    REQUIRE(sql.find("JOIN Parent") != std::string::npos);
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

// TEST_CASE("SelectBuilder Aggregations", "PG_SQL_AGG") {
//     // Animal -> Canine -> Dog
//     auto Animal = makeSchema("Animal", nullptr, {"ID","Age"});
//     auto Canine = makeSchema("Canine", Animal, {});
//     auto Dog    = makeSchema("Dog", Canine, {"Breed"});

//     ql::QueryBuilder::GetSchemaFn get = [&](const std::string& n)->std::shared_ptr<OrmSchema>{
//         if (n=="Animal") return Animal;
//         if (n=="Canine") return Canine;
//         if (n=="Dog")    return Dog;
//         return {};
//     };

//     // Query with aggregation & group by
//     const char* doc = "{ Dog { breed.groupby , dog_per_breed:id.count , average_age:age.avg } }";
//     std::cout << doc << std::endl << std::endl;
//     ql::QueryCompiler qc;
//     auto ast = qc.compile(doc);

//     QueryBuilder sb(get, Dialect::Postgres);
//     auto sql = sb.build_sql(ast);

//     // FROM/JOIN chain
//     REQUIRE(sql.find("FROM \"Animal\" t0\nJOIN \"Canine\" t1 ON (t0.\"ID\" = t1.\"ID\")") != std::string::npos);
//     REQUIRE(sql.find("JOIN \"Dog\" t2 ON (t1.\"ID\" = t2.\"ID\")") != std::string::npos);

//     // JSON object with aggregates (case-preserved keys)
//     REQUIRE(sql.find("jsonb_build_object('breed', t2.\"Breed\", 'dog_per_breed', count(*), 'average_age', avg(t0.\"Age\"))") != std::string::npos);

//     // Group by column expression
//     REQUIRE(sql.find("GROUP BY t2.\"Breed\"") != std::string::npos);

//     // Outer JSON array aggregation
//     REQUIRE(sql.find("SELECT jsonb_agg(obj) AS data") != std::string::npos);
// }

// TEST_CASE("QueryCompiler", "Minimal") {
//     const char* doc = "{ Golden_Retriever { Name, Age, Breed } }";
//     std::cout << doc << std::endl << std::endl;
//     ql::QueryCompiler qc;
//     auto ast = qc.compile(doc);
//     REQUIRE(ast.rootTypeName == "Golden_Retriever");
//     REQUIRE(ast.selectionSet.size()  == 3u);
//     REQUIRE(ast.selectionSet[0].name == "Name");
//     REQUIRE(ast.selectionSet[1].name == "Age");
//     REQUIRE(ast.selectionSet[2].name == "Breed");
// }

// TEST_CASE("SelectBuilder", "PG_SQL") {
//     // build chain: Animal -> Canine -> Dog -> Golden_Retriever
//     auto Animal = makeSchema("Animal", nullptr, {"ID","Name","Age"});
//     auto Canine = makeSchema("Canine", Animal, {});
//     auto Dog    = makeSchema("Dog", Canine, {"Breed"});
//     auto Gold   = makeSchema("Golden_Retriever", Dog, {});

//     SelectBuilder::GetSchemaFn get = [&](const std::string& n)->std::shared_ptr<OrmSchema>{
//         if (n=="Animal") return Animal;
//         if (n=="Canine") return Canine;
//         if (n=="Dog") return Dog;
//         if (n=="Golden_Retriever") return Gold;
//         return {};
//     };

//     const char* doc = "{ Golden_Retriever { Name, Age, Breed } }";
//     std::cout << doc << std::endl << std::endl;
//     ql::QueryCompiler qc;
//     auto ast = qc.compile(doc);

//     SelectBuilder sb(get, Dialect::Postgres);
//     auto sql = sb.build_sql(ast);

//     // sanity checks
//     REQUIRE(sql.find("FROM \"Animal\" t0\nJOIN \"Canine\" t1 ON (t0.\"ID\" = t1.\"ID\")") != std::string::npos);
//     REQUIRE(sql.find("JOIN \"Dog\" t2 ON (t1.\"ID\" = t2.\"ID\")") !=  std::string::npos);
//     REQUIRE(sql.find("JOIN \"Golden_Retriever\" t3 ON (t2.\"ID\" = t3.\"ID\")") !=  std::string::npos);
//     REQUIRE(sql.find("jsonb_build_object('Name', t0.\"Name\", 'Age', t0.\"Age\", 'Breed', t2.\"Breed\")") !=  std::string::npos);
//     REQUIRE(sql.find("SELECT jsonb_agg(obj) AS data") !=  std::string::npos);
// }

// TEST_CASE("SelectBuilder2", "SQLITE_SQL") {
//     // same chain
//     auto Animal = makeSchema("Animal", nullptr, {"ID","Name"});
//     auto Dog = makeSchema("Dog", Animal, {"Tag"});
//     SelectBuilder::GetSchemaFn get = [&](const std::string& n)->std::shared_ptr<OrmSchema>{
//         if (n=="Animal") return Animal;
//         if (n=="Dog") return Dog;
//         return {};
//     };

//     const char* doc = "{ Dog { Name, Tag } }";
//     std::cout << doc << std::endl << std::endl;
//     ql::QueryCompiler qc;
//     auto ast = qc.compile(doc);

//     SelectBuilder sb(get, Dialect::SQLite);
//     auto sql = sb.build_sql(ast);

//     REQUIRE(sql.find("FROM \"Animal\" t0\nJOIN \"Dog\" t1 ON (t0.\"ID\" = t1.\"ID\")") != std::string::npos);
//     REQUIRE(sql.find("json_object('Name', t0.\"Name\", 'Tag', t1.\"Tag\")") !=  std::string::npos);
//     REQUIRE(sql.find("SELECT json_group_array(obj) AS data") !=  std::string::npos);
// }


// TEST_CASE("SelectBuilder Aggregations last", "PG_SQL_AGG") {
//     // Animal -> Canine -> Dog
//     auto Animal = makeSchema("Animal", nullptr, {"ID","Age"});
//     auto Canine = makeSchema("Canine", Animal, {});
//     auto Dog    = makeSchema("Dog", Canine, {"Breed"});

//     SelectBuilder::GetSchemaFn get = [&](const std::string& n)->std::shared_ptr<OrmSchema>{
//         if (n=="Animal") return Animal;
//         if (n=="Canine") return Canine;
//         if (n=="Dog")    return Dog;
//         return {};
//     };

//     // Query with aggregation & group by
//     const char* doc = "{ Dog { breed.groupby , dog_per_breed:id.count , average_age:age.avg } }";
//     std::cout << doc << std::endl << std::endl;
//     ql::QueryCompiler qc;
//     auto ast = qc.compile(doc);

//     SelectBuilder sb(get, Dialect::Postgres);
//     auto sql = sb.build_sql(ast);

//     // FROM/JOIN chain
//     REQUIRE(sql.find("FROM \"Animal\" t0\nJOIN \"Canine\" t1 ON (t0.\"ID\" = t1.\"ID\")") != std::string::npos);
//     REQUIRE(sql.find("JOIN \"Dog\" t2 ON (t1.\"ID\" = t2.\"ID\")") != std::string::npos);

//     // JSON object with aggregates (case-preserved keys)
//     REQUIRE(sql.find("jsonb_build_object('breed', t2.\"Breed\", 'dog_per_breed', count(*), 'average_age', avg(t0.\"Age\"))") != std::string::npos);

//     // Group by column expression
//     REQUIRE(sql.find("GROUP BY t2.\"Breed\"") != std::string::npos);

//     // Outer JSON array aggregation
//     REQUIRE(sql.find("SELECT jsonb_agg(obj) AS data") != std::string::npos);
// }

// // Helpers to build schemas with explicit types
// static std::shared_ptr<OrmSchema> makeSchemaT(const std::string& name, std::shared_ptr<OrmSchema> parent,
//                                               std::initializer_list<std::pair<std::string,PropType>> props) {
//     auto s = std::make_shared<OrmSchema>();
//     s->name = name;
//     if (parent) s->parent = parent;
//     for (auto& kv : props) {
//         OrmProp p; p.name = kv.first; p.is_id = (kv.first == "ID");
//         p.type = kv.second;
//         p.parent = s;
//         s->fields.emplace(kv.first, std::make_shared<OrmProp>(std::move(p)));
//     }
//     return s;
// }

// TEST_CASE("DateTime funcs - Postgres", "[dtfuncs][pg]") {
//     auto Animal = makeSchemaT("Animal", nullptr, {{"ID",PropType::Integer}, {"BornAt",PropType::Dt_Time}, {"Name",PropType::String}});
//     auto Dog    = makeSchemaT("Dog", Animal, {{"Breed",PropType::String}});
//     SelectBuilder::GetSchemaFn get = [&](const std::string& n)->std::shared_ptr<OrmSchema>{
//         if (n=="Animal") return Animal;
//         if (n=="Dog") return Dog;
//         return {};
//     };
//     const char* doc = "{ Dog { Only_Date:BornAt.date, Ym:BornAt.ym, Ns:BornAt.ns, Breed } }";
//     std::cout << doc << std::endl << std::endl;
//     ql::QueryCompiler qc;
//     auto ast = qc.compile(doc);
//     SelectBuilder sb(get, Dialect::Postgres);
//     auto sql = sb.build_sql(ast);
//     REQUIRE(sql.find("to_char(t0.\"BornAt\", 'YYYY-MM-DD')") != std::string::npos);
//     REQUIRE(sql.find("to_char(t0.\"BornAt\", 'YYYY-MM')") != std::string::npos);
//     REQUIRE(sql.find("to_char(t0.\"BornAt\", 'MI:SS')") != std::string::npos);
//     REQUIRE(sql.find("Only_Date") != std::string::npos);
//     REQUIRE(sql.find("Ym") != std::string::npos);
//     REQUIRE(sql.find("Ns") != std::string::npos);
// }

// TEST_CASE("DateTime funcs - SQLite", "[dtfuncs][sqlite]") {
//     auto Animal = makeSchemaT("Animal", nullptr, {{"ID",PropType::Integer}, {"When",PropType::Dt_Time}});
//     auto Dog    = makeSchemaT("Dog", Animal, {{"Breed",PropType::String}});
//     SelectBuilder::GetSchemaFn get = [&](const std::string& n)->std::shared_ptr<OrmSchema>{
//         if (n=="Animal") return Animal;
//         if (n=="Dog") return Dog;
//         return {};
//     };
//     const char* doc = "{ Dog { Time_Ms:When.timems, NdH:When.mdh } }";
//     std::cout << doc << std::endl << std::endl;
//     ql::QueryCompiler qc;
//     auto ast = qc.compile(doc);
//     SelectBuilder sb(get, Dialect::SQLite);
//     auto sql = sb.build_sql(ast);
//     REQUIRE(sql.find("strftime('%H:%M:%f', t0.\"When\")") != std::string::npos);
//     REQUIRE(sql.find("strftime('%m-%dT%H', t0.\"When\")") != std::string::npos);
//     REQUIRE(sql.find("Time_Ms") != std::string::npos);
//     REQUIRE(sql.find("NdH") != std::string::npos);
// }


// TEST_CASE("GroupBy on Date/Time derived keys - PostgreSQL", "[groupby][dt][pg]") {
//     auto Animal = makeSchemaT("Animal", nullptr, {{"ID",PropType::Integer}, {"When",PropType::Dt_Time}});
//     auto Dog    = makeSchemaT("Dog", Animal, {{"Breed",PropType::String}});

//     SelectBuilder::GetSchemaFn get = [&](const std::string& n)->std::shared_ptr<OrmSchema>{
//         if (n=="Animal") return Animal;
//         if (n=="Dog") return Dog;
//         return {};
//     };

//     // two derived keys + one aggregate to force validation of grouping
//     const char* doc = "{ Dog { TimeMs:When.timems.groupby, NdH:When.mdh.groupby, c:ID.count } }";
//     std::cout << doc << std::endl << std::endl;
//     ql::QueryCompiler qc;
//     auto ast = qc.compile(doc);

//     // Build for Postgres
//     SelectBuilder sb(get, Dialect::Postgres);
//     auto sql = sb.build_sql(ast);

//     // SELECT projections: both derived keys aliased
//     REQUIRE(sql.find("TimeMs") != std::string::npos);
//     REQUIRE(sql.find("NdH") != std::string::npos);
//     REQUIRE(sql.find("c") != std::string::npos);
//     // Postgres uses to_char for dt transforms
//     REQUIRE(sql.find("to_char(") != std::string::npos);
//     // GROUP BY uses the same transformed scalar expressions (not raw column)
//     // timems -> HH24:MI:SS.MS ; mdh -> mask expands to something containing HH24
//     REQUIRE(sql.find("GROUP BY") != std::string::npos);
//     REQUIRE(sql.find("to_char(t0.\"When\"") != std::string::npos);
//     // Count projection
//     REQUIRE(sql.find("'c', count(*)) AS") != std::string::npos);
//     // Outer JSON packaging present
//     REQUIRE(sql.find("SELECT jsonb_agg(obj) AS data") != std::string::npos);
// }

// TEST_CASE("GroupBy on Date/Time derived keys - SQLite", "[groupby][dt][sqlite]") {
//     auto Animal = makeSchemaT("Animal", nullptr, {{"ID",PropType::Integer}, {"When",PropType::Dt_Time}});
//     auto Dog    = makeSchemaT("Dog", Animal, {{"Breed",PropType::String}});

//     SelectBuilder::GetSchemaFn get = [&](const std::string& n)->std::shared_ptr<OrmSchema>{
//         if (n=="Animal") return Animal;
//         if (n=="Dog") return Dog;
//         return {};
//     };

//     // one derived key + one aggregate
//     const char* doc = "{ Dog { NdH:When.mdh.groupby, per_slot:ID.count } }";
//     std::cout << doc << std::endl << std::endl;
//     ql::QueryCompiler qc;
//     auto ast = qc.compile(doc);

//     SelectBuilder sb(get, Dialect::SQLite);
//     auto sql = sb.build_sql(ast);

//     // strftime appears in projection and in GROUP BY
//     REQUIRE(sql.find("strftime(") != std::string::npos);
//     REQUIRE(sql.find("GROUP BY") != std::string::npos);
//     REQUIRE(sql.find("strftime('%m-%dT%H', t0.\"When\")") != std::string::npos);
//     // JSON wrapper for SQLite
//     REQUIRE(sql.find("SELECT json_group_array(obj) AS data") != std::string::npos);
// }

// TEST_CASE("Aggregates require non-agg fields to be .groupby (Postgres)", "[groupby][error][pg]") {
//     // Animal -> Canine -> Dog
//     auto Animal = makeSchema("Animal", nullptr, {"ID","Age"});
//     auto Canine = makeSchema("Canine", Animal, {});
//     auto Dog    = makeSchema("Dog", Canine, {"Breed"});

//     SelectBuilder::GetSchemaFn get = [&](const std::string& n)->std::shared_ptr<OrmSchema>{
//         if (n=="Animal") return Animal;
//         if (n=="Canine") return Canine;
//         if (n=="Dog")    return Dog;
//         return {};
//     };

//     // Missing .groupby on Breed while using aggregates -> must throw
//     const char* doc = "{ Dog { Breed , dog_per_breed:id.count , average_age:age.avg } }";
//     ql::QueryCompiler qc;
//     auto ast = qc.compile(doc);

//     SelectBuilder sb(get, Dialect::Postgres);
//     REQUIRE_THROWS_WITH(
//         sb.build_sql(ast),
//         "Non-aggregated field 'Breed' must be marked with .groupby when aggregates are used"
//     );
// }

// TEST_CASE("Aggregates require .groupby even with dt functions (SQLite)", "[groupby][error][sqlite][dt]") {
//     // Animal -> Dog, with a datetime field
//     auto Animal = makeSchemaT("Animal", nullptr, {{"ID",PropType::Integer}, {"When",PropType::Dt_Time}});
//     auto Dog    = makeSchemaT("Dog", Animal, {{"Breed",PropType::String}});

//     SelectBuilder::GetSchemaFn get = [&](const std::string& n)->std::shared_ptr<OrmSchema>{
//         if (n=="Animal") return Animal;
//         if (n=="Dog")    return Dog;
//         return {};
//     };

//     // Derived dt key without .groupby + aggregate -> must throw (message uses alias)
//     const char* doc = "{ Dog { Time_Ms:When.timems , total:id.count } }";
//     ql::QueryCompiler qc;
//     auto ast = qc.compile(doc);

//     SelectBuilder sb(get, Dialect::SQLite);
//     REQUIRE_THROWS_WITH(
//         sb.build_sql(ast),
//         "Non-aggregated field 'Time_Ms' must be marked with .groupby when aggregates are used"
//     );
// }

// // ---- SQLite: FK join + nested JSON object ----
// TEST_CASE("FK nested object selection (SQLite)", "[select][join][sqlite]") {
//     auto Animal = makeSchemaT("Animal", nullptr, {{"ID",PropType::Integer}});
//     auto Dog    = makeSchemaT("Dog", Animal, {{"ID",PropType::Integer}, {"Breed",PropType::String}, {"Owner",PropType::Object}});
//     auto Owner  = makeSchemaT("Owner", nullptr, {{"ID",PropType::Integer}, {"Name",PropType::String}, {"Address",PropType::String}, {"Phone",PropType::String}});
//     // Dog.Owner -> Owner.ID
//     {
//         auto fk = Dog->fields.at("Owner");
//         fk->type = PropType::Object; fk->parent = Dog;
//         fk->ref_Schema = Owner; fk->ref_Field = Owner->fields.at("ID");
//     }
//     SelectBuilder::GetSchemaFn get = [&](const std::string& n)->std::shared_ptr<OrmSchema>{
//         if (n=="Animal") return Animal; if (n=="Dog") return Dog; if (n=="Owner") return Owner; return {};
//     };
//     const char* doc = "{ Dog { id, breed, owner { id, name, address, phone } } }";
//     ql::QueryCompiler qc; auto ast = qc.compile(doc);
//     SelectBuilder sb(get, Dialect::SQLite); auto sql = sb.build_sql(ast);

//     REQUIRE(sql.find("LEFT JOIN \"Owner\" j0 ON (t1.\"Owner\" = j0.\"ID\")") != std::string::npos);
//     REQUIRE(sql.find("'owner', json_object('id', j0.\"ID\", 'name', j0.\"Name\", 'address', j0.\"Address\", 'phone', j0.\"Phone\")") != std::string::npos);
//     REQUIRE(sql.find("SELECT json_group_array(obj) AS data") != std::string::npos);
// }

// // ---- Postgres: FK join + nested JSON object ----
// TEST_CASE("FK nested object selection (Postgres)", "[select][join][pg]") {
//     auto Animal = makeSchemaT("Animal", nullptr, {{"ID",PropType::Integer}});
//     auto Dog    = makeSchemaT("Dog", Animal, {{"ID",PropType::Integer}, {"Breed",PropType::String}, {"Owner",PropType::Object}});
//     auto Owner  = makeSchemaT("Owner", nullptr, {{"ID",PropType::Integer}, {"Name",PropType::String}, {"Phone",PropType::String}});
//     // Dog.Owner -> Owner.ID
//     {
//         auto fk = Dog->fields.at("Owner");
//         fk->type = PropType::Object; fk->parent = Dog;
//         fk->ref_Schema = Owner; fk->ref_Field = Owner->fields.at("ID");
//     }
//     SelectBuilder::GetSchemaFn get = [&](const std::string& n)->std::shared_ptr<OrmSchema>{
//         if (n=="Animal") return Animal; if (n=="Dog") return Dog; if (n=="Owner") return Owner; return {};
//     };
//     const char* doc = "{ Dog { id, breed, owner { id, phone } } }";
//     ql::QueryCompiler qc; auto ast = qc.compile(doc);
//     SelectBuilder sb(get, Dialect::Postgres); auto sql = sb.build_sql(ast);

//     REQUIRE(sql.find("LEFT JOIN \"Owner\" j0 ON (t1.\"Owner\" = j0.\"ID\")") != std::string::npos);
//     REQUIRE(sql.find("'owner', jsonb_build_object('id', j0.\"ID\", 'phone', j0.\"Phone\")") != std::string::npos);
//     REQUIRE(sql.find("SELECT jsonb_agg(obj) AS data") != std::string::npos);
// }


// // ---- SQLite: nested groupby + nested aggregates (1:1 FK collapses to scalar) ----
// TEST_CASE("Nested FK: createdAt.date.groupby and id.count (SQLite)", "[select][join][nested][sqlite]") {
//     auto Animal = makeSchemaT("Animal", nullptr, {{"ID",PropType::Integer}});
//     auto Dog    = makeSchemaT("Dog", Animal, {{"ID",PropType::Integer}, {"Owner",PropType::Object}});
//     auto Owner  = makeSchemaT("Owner", nullptr, {
//         {"ID",PropType::Integer},
//         {"CreatedAt",PropType::Dt_Time},
//         {"Name",PropType::String}
//     });

//     // Dog.Owner -> Owner.ID
//     {
//         auto fk = Dog->fields.at("Owner");
//         fk->type = PropType::Object; fk->parent = Dog;
//         fk->ref_Schema = Owner; fk->ref_Field = Owner->fields.at("ID");
//     }

//     SelectBuilder::GetSchemaFn get = [&](const std::string& n)->std::shared_ptr<OrmSchema>{
//         if (n=="Animal") return Animal;
//         if (n=="Dog")    return Dog;
//         if (n=="Owner")  return Owner;
//         return {};
//     };

//     // alias the count for a stable key name in JSON
//     const char* doc = "{ Dog { id, owner { createdAt.date.groupby, id_count:id.count } } }";
//     ql::QueryCompiler qc;
//     auto ast = qc.compile(doc);
//     SelectBuilder sb(get, Dialect::SQLite);
//     auto sql = sb.build_sql(ast);

//     // FK join once, correct aliases (Animal t0, Dog t1, Owner j0)
//     REQUIRE(sql.find("LEFT JOIN \"Owner\" j0 ON (t1.\"Owner\" = j0.\"ID\")") != std::string::npos);

//     // nested JSON includes dt mask + scalar count (presence)
//     REQUIRE(
//         sql.find(
//             "'owner', json_object('createdAt', strftime('%Y-%m-%d', j0.\"CreatedAt\"), 'id_count', CASE WHEN j0.\"ID\" IS NULL THEN 0 ELSE 1 END)"
//         ) != std::string::npos
//     );

//     // nested .groupby does not introduce a top-level GROUP BY (1:1 FK)
//     REQUIRE(sql.find(" GROUP BY ") == std::string::npos);

//     // single-column JSON array wrapper
//     REQUIRE(sql.find("SELECT json_group_array(obj) AS data") != std::string::npos);
// }

// // ---- Postgres: nested groupby + nested aggregates (1:1 FK collapses to scalar) ----
// TEST_CASE("Nested FK: createdAt.date.groupby and id.count (Postgres)", "[select][join][nested][pg]") {
//     auto Animal = makeSchemaT("Animal", nullptr, {{"ID",PropType::Integer}});
//     auto Dog    = makeSchemaT("Dog", Animal, {{"ID",PropType::Integer}, {"Owner",PropType::Object}});
//     auto Owner  = makeSchemaT("Owner", nullptr, {
//         {"ID",PropType::Integer},
//         {"CreatedAt",PropType::Dt_Time},
//         {"Phone",PropType::String}
//     });

//     // Dog.Owner -> Owner.ID
//     {
//         auto fk = Dog->fields.at("Owner");
//         fk->type = PropType::Object; fk->parent = Dog;
//         fk->ref_Schema = Owner; fk->ref_Field = Owner->fields.at("ID");
//     }

//     SelectBuilder::GetSchemaFn get = [&](const std::string& n)->std::shared_ptr<OrmSchema>{
//         if (n=="Animal") return Animal;
//         if (n=="Dog")    return Dog;
//         if (n=="Owner")  return Owner;
//         return {};
//     };

//     const char* doc = "{ Dog { id, owner { createdAt.date.groupby, id_count:id.count } } }";
//     ql::QueryCompiler qc;
//     auto ast = qc.compile(doc);
//     SelectBuilder sb(get, Dialect::Postgres);
//     auto sql = sb.build_sql(ast);

//     // FK join once, correct aliases
//     REQUIRE(sql.find("LEFT JOIN \"Owner\" j0 ON (t1.\"Owner\" = j0.\"ID\")") != std::string::npos);

//     // nested JSON with to_char for date + scalar count (presence)
//     REQUIRE(
//         sql.find(
//             "'owner', jsonb_build_object('createdAt', to_char(j0.\"CreatedAt\", 'YYYY-MM-DD'), 'id_count', CASE WHEN j0.\"ID\" IS NULL THEN 0 ELSE 1 END)"
//         ) != std::string::npos
//     );

//     // no top-level GROUP BY introduced by nested .groupby
//     REQUIRE(sql.find(" GROUP BY ") == std::string::npos);

//     // single-column JSON array wrapper
//     REQUIRE(sql.find("SELECT jsonb_agg(obj) AS data") != std::string::npos);
// }
