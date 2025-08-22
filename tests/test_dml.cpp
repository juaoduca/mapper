
#include "catch.hpp"

#include "ddl_visitor.hpp"
#include "dml_visitor.hpp"
#include "storage.hpp"
#include "fake_sql.hpp"

// using Json = Json;

// ---- Helpers: schemas ----
static OrmSchema make_user_schema() {

    OrmSchema s;
    s.name = "users";
    s.version = 1;
    s.applied = true;

    OrmProp id;   id.name="id";   id.type=PropType::Integer; id.is_id=true; id.required=true; id.id_kind = IdKind::Snowflake;
    OrmProp name; name.name="name"; name.type=PropType::String; name.required=true;
    OrmProp age;  age.name="age";  age.type=PropType::Integer;

    s.fields[id.name] = id;
    s.fields[name.name] = name;
    s.fields[age.name] = age;
    return s;
}

static OrmSchema make_types_schema() {
    OrmSchema s;
    s.name = "events";
    s.version = 1; s.applied = true;

    OrmProp id  ; id  .name="id"  ; id  .type=PropType::Integer ; id.is_id=true; id.required=true; id.id_kind = IdKind::HighLow;
    OrmProp d   ; d   .name="d"   ; d   .type=PropType::Date    ;
    OrmProp t   ; t   .name="t"   ; t   .type=PropType::Time    ;
    OrmProp dt  ; dt  .name="dt"  ; dt  .type=PropType::Dt_Time ;
    OrmProp ts  ; ts  .name="ts"  ; ts  .type=PropType::Tm_Stamp;
    OrmProp j   ; j   .name="j"   ; j   .type=PropType::Json    ;
    OrmProp b   ; b   .name="b"   ; b   .type=PropType::Bin     ;
    OrmProp flag; flag.name="flag"; flag.type=PropType::Bool    ;
    OrmProp num ; num .name="num" ; num .type=PropType::Number  ;
    OrmProp str ; str .name="str" ; str .type=PropType::String  ;

    s.fields[id.name]=id;
    s.fields[d.name]=d;
    s.fields[t.name]=t;
    s.fields[dt.name]=dt;
    s.fields[ts.name]=ts;
    s.fields[j.name]=j;
    s.fields[b.name]=b;
    s.fields[flag.name]=flag;
    s.fields[num.name]=num;
    s.fields[str.name]=str;
    return s;
}

// ---- Dialect picker for constructing Storage ----
static Storage make_storage_for(Dialect d) {
    // We won't touch the real DB: tests call insert(conn, ...) overload with FakeSQLConnection.
    if (d == Dialect::Postgres){
        return Storage("host=localhost port=5432 dbname=orm user=postgres password=brlnd044", d);
    } else /*if (d == Dialect::SQLite)*/ {
        return Storage("./database.db", d);
    }
}

// ====================== TESTS ======================

// INSERT with PK absent → JSON order preserved, PK appended LAST (and bound last)
TEST_CASE("INSERT: PK absent → PK appended LAST; JSON order preserved (SQLite)", "[dml][insert][pk-absent][sqlite]") {
    OrmSchema schema = make_user_schema();
    jdoc doc;
    jhlp::parse_str(R"({"name":"Alice","age":30})", doc);

    Storage st = make_storage_for(Dialect::SQLite);
    FakeSQLConnection conn; //create a local instance - free when func returns

    int rows = st.insert(conn, schema, doc, "");
    REQUIRE(rows == 1);
    // REQUIRE(conn.last != nullptr);

    REQUIRE(conn.last.sql.find("INSERT INTO users") != std::string::npos);
    REQUIRE(conn.last.exec_calls == 1);
    REQUIRE(conn.last.bind_calls == 3);
    REQUIRE(conn.last.binds.size() == 3);

    REQUIRE(conn.last.binds[0].idx == 1);
    REQUIRE(conn.last.binds[0].valuecast == "Alice");
    REQUIRE(conn.last.binds[0].valuetype == rapidjson::Type::kStringType);
    REQUIRE(conn.last.binds[0].type == PropType::String); // age

    REQUIRE(conn.last.binds[1].idx == 2);
    REQUIRE(conn.last.binds[1].valuecast == "30");
    REQUIRE(conn.last.binds[1].valuetype == rapidjson::Type::kNumberType);
    REQUIRE(conn.last.binds[1].type == PropType::Integer); // name

    REQUIRE(conn.last.binds[2].idx == 3);
    REQUIRE(conn.last.binds[2].valuetype == rapidjson::Type::kNumberType);
    REQUIRE(conn.last.binds[2].type == PropType::Integer); // PK
}

#ifdef HAVE_POSTGRESQL
TEST_CASE("INSERT: PK absent → PK appended LAST; JSON order preserved (Postgres)", "[dml][insert][pk-absent][pg]") {
    OrmSchema schema = make_user_schema();
    auto j = R"({"name": "Alice", "age": 30 })";
    jdoc doc;
    jhlp::parse_str(j, doc);

    Storage st = make_storage_for(Dialect::SQLite);
    FakeSQLConnection conn;

    int rows = st.insert(conn, schema, doc, "");
    REQUIRE(rows == 1);

    REQUIRE(conn.last.sql.find("INSERT INTO users") != std::string::npos);
    REQUIRE(conn.last.exec_calls == 1);
    REQUIRE(conn.last.bind_calls == 3);
    REQUIRE(conn.last.binds.size() == 3);

    REQUIRE(conn.last.binds[0].idx == 1);
    REQUIRE(conn.last.binds[0].valuecast == "Alice");
    REQUIRE(conn.last.binds[0].valuetype == rapidjson::Type::kStringType);
    REQUIRE(conn.last.binds[0].type == PropType::String); // age

    REQUIRE(conn.last.binds[1].idx == 2);
    REQUIRE(conn.last.binds[1].valuecast == "30");
    REQUIRE(conn.last.binds[1].valuetype == rapidjson::Type::kNumberType);
    REQUIRE(conn.last.binds[1].type == PropType::Integer); // name

    REQUIRE(conn.last.binds[2].idx == 3);
    REQUIRE(conn.last.binds[2].valuetype == rapidjson::Type::kNumberType);
    REQUIRE(conn.last.binds[2].type == PropType::Integer); // PK
}
#endif

// INSERT with PK present but invalid (0) → PK replaced in-place (order unchanged)
TEST_CASE("INSERT: PK present but invalid → value replaced; order unchanged (SQLite)", "[dml][insert][pk-invalid][sqlite]") {
    OrmSchema schema = make_user_schema();
    jdoc doc;
    jhlp::parse_str(R"({"id": 0, "name": "Bob"})", doc);

    Storage st = make_storage_for(Dialect::SQLite);
    FakeSQLConnection conn;

    int rows = st.insert(conn, schema, doc, "");
    REQUIRE(rows == 1);
    // auto stmt = conn.last;
    // REQUIRE(conn.last);

    REQUIRE(conn.last.sql.find("INSERT INTO users") != std::string::npos);
    REQUIRE(conn.last.exec_calls == 1);
    REQUIRE(conn.last.binds.size() == 2);
    REQUIRE(conn.last.bind_calls == 2);

    REQUIRE(conn.last.binds[0].idx == 1);
    REQUIRE(conn.last.binds[0].valuecast == "Bob");
    REQUIRE(conn.last.binds[0].valuetype == rapidjson::Type::kStringType);
    REQUIRE(conn.last.binds[0].type == PropType::String); // age

    REQUIRE(conn.last.binds[1].idx == 2);
    REQUIRE(conn.last.binds[1].valuetype == rapidjson::Type::kNumberType);
    REQUIRE(conn.last.binds[1].type == PropType::Integer); // PK
}

//UPSERT when PK present and valid → follow JSON order; SQL has ON CONFLICT
TEST_CASE("UPSERT: PK present and valid → JSON order; ON CONFLICT present (SQLite)", "[dml][upsert][sqlite]") {
    OrmSchema schema = make_user_schema();
    jdoc doc;
    jhlp::parse_str(R"({"id": 42, "name":"Carol", "age": 25})", doc);

    Storage st = make_storage_for(Dialect::SQLite);
    FakeSQLConnection conn;

    int rows = st.insert(conn, schema, doc, "");
    REQUIRE(rows == 1);

    // auto stmt = conn.last;
    // REQUIRE(stmt);
    REQUIRE(conn.last.sql.find("ON CONFLICT(") != std::string::npos);

    REQUIRE(conn.last.exec_calls == 1);
    REQUIRE(conn.last.binds.size() == 3);
    REQUIRE(conn.last.bind_calls == 3);

    REQUIRE(conn.last.binds[0].idx == 1); // ID
    REQUIRE(conn.last.binds[0].type == PropType::Integer);
    REQUIRE(conn.last.binds[0].valuetype == rapidjson::Type::kNumberType);
    REQUIRE(conn.last.binds[0].valuecast == "42");

    REQUIRE(conn.last.binds[1].idx == 2); // name
    REQUIRE(conn.last.binds[1].valuecast == "Carol");
    REQUIRE(conn.last.binds[1].valuetype == rapidjson::Type::kStringType);
    REQUIRE(conn.last.binds[1].type == PropType::String);

    REQUIRE(conn.last.binds[2].idx == 3); // age
    REQUIRE(conn.last.binds[2].valuecast == "25");
    REQUIRE(conn.last.binds[2].valuetype == rapidjson::Type::kNumberType);
    REQUIRE(conn.last.binds[2].type == PropType::Integer);

}

// Type mapping: DATE/TIME/DTIME/TIMEST/JSON/BOOL/NUM/STR/BIN must bind correct "type" tag
TEST_CASE("Bind types: date/time/datetime/timestamp/Json/bool/num/str/bin (SQLite)", "[dml][types][sqlite]") {
    OrmSchema schema = make_types_schema();

    // PK absent; JSON order must be preserved; PK appended last
    auto j = R"({
        "d"   : "2025-08-18"            ,
        "t"   : "12:34:56"              ,
        "dt"  : "2025-08-18 12:34:56"   ,
        "ts"  : "2025-08-18T12:34:56Z"  ,
        "j"   : {"k": 1}                ,
        "b"   : "YWJj=="                ,
        "flag": true                    ,
        "num" : 3.14159                 ,
        "str" : "hello"                })";
    jdoc doc;
    jhlp::parse_str(j, doc);

    Storage st = make_storage_for(Dialect::SQLite);
    FakeSQLConnection conn;

    int rows = st.insert(conn, schema, doc, "");
    REQUIRE(rows == 1);

    // auto stmt = conn.last;
    // REQUIRE(stmt);
    // 9 JSON fields + 1 PK
    REQUIRE(conn.last.binds.size() == 10);
 //                       0         1      2       3     4     5     6         7       8    9
//enum class PropType { String, Integer, Number, Bool, Date, Time, Dt_Time, Tm_Stamp, Bin, Json};
    REQUIRE(conn.last.binds[0].type == PropType::Date     ); // b
    REQUIRE(conn.last.binds[1].type == PropType::Time     ); // d
    REQUIRE(conn.last.binds[2].type == PropType::Dt_Time  ); // dt
    REQUIRE(conn.last.binds[3].type == PropType::Tm_Stamp ); // flag
    REQUIRE(conn.last.binds[4].type == PropType::Json     ); // j
    REQUIRE(conn.last.binds[5].type == PropType::Bin      ); // num
    REQUIRE(conn.last.binds[6].type == PropType::Bool     ); // str
    REQUIRE(conn.last.binds[7].type == PropType::Number   ); // t
    REQUIRE(conn.last.binds[8].type == PropType::String   ); // ts
    REQUIRE(conn.last.binds[9].type == PropType::Integer  ); // PK
}
