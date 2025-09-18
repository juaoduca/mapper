
#include "catch.hpp"

#include "storage.hpp"

OrmSchema load_schema(const std::string& js) {
    OrmSchema s;
    jdoc doc = jhlp::parse_str(js);
    OrmSchema::from_json(doc, s);
    return s;
};

TEST_CASE("Test catalog ", "Only one returned Token") {

    // std::string jschema = R"({
    //     "name": "users",
    //     "type": "object",
    //     "properties": {
    //         "id":      { "type": "string", "idprop": true, "idkind": "UUIDv7" },
    //         "active":  { "type": "boolean", "default": true },
    //         "avatar":  { "type": "json", "encoding": "yenc" },
    //         "score":   { "type": "integer", "default": 42 },
    //         "joined":  { "type": "date" },
    //         "logins":  { "type": "integer" },
    //         "profile": { "type": "json" },
    //         "email":   { "type": "string", "unique": true, "default": "" },
    //         "last_seen": { "type": "datetime" }
    //     },
    //     "required": ["id", "email", "score"],
    //     "indexes": [
    //         { "fields": ["email"], "unique": true, "type": "btree", "indexName": "idx_email" },
    //         { "fields": ["score", "active"], "indexName": "idx_score_active" }
    //     ]
    // })";
    // OrmSchema schema = load_schema(jschema);

    Storage st("./database.db", Dialect::SQLite);

    st.init_catalog();

    //check if catalog tables was created correctly
    st.with_conn(pool::DbIntent::Read,
        [&](SQLConnection &conn) {
            char *resp = nullptr;
            std::string cat = "schema_catalog";
            std::string ver = "schema_versions";
            bool res = conn.execGET("SELECT name FROM sqlite_master WHERE type='table' AND lower(name) = '"+cat+"';", &resp);
            REQUIRE(res);
            REQUIRE(strcmp(resp, cat.c_str()) == 0);
            res = conn.execGET("SELECT name FROM sqlite_master WHERE type='table' AND lower(name) = '"+ver+"';", &resp);
            REQUIRE(res);
            REQUIRE(strcmp(resp, ver.c_str()) == 0);
            //check if catalog tables have the records for schema_catalog and schema_versions
            return true;
        });

    // REQUIRE(true);
}

