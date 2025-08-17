#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <string_view>

constexpr std::string_view DT_STR    = "string"    ;
constexpr std::string_view DT_INT    = "integer"   ;
constexpr std::string_view DT_NUM    = "number"    ;
constexpr std::string_view DT_BOOL   = "boolean"   ;
constexpr std::string_view DT_DATE   = "date"      ;
constexpr std::string_view DT_TIME   = "time"      ;
constexpr std::string_view DT_DTIME  = "datetime"  ;
constexpr std::string_view DT_TIMEST = "timestamp" ;
constexpr std::string_view DT_BIN    = "binary"    ;
constexpr std::string_view DT_JSON   = "json"      ;


enum class IdKind { UUIDv7, HighLow, Snowflake, DBSerial, TBSerial };
enum class DefaultKind { None, String, Boolean, Number, Raw };
enum class Dialect {SQLite, Postgres};

struct OrmField {
    std::string name; // prop/field name
    std::string type; // prop Dataype / field Datatype
    bool is_id = false; // if the prop/field is an ID field/prop
    IdKind id_kind = IdKind::UUIDv7; // define the generation of the ID
    bool required = false; //must be filled or not
    std::string encoding;  // encoding type for binary data (yEnc, Base64 etc)
    std::string default_value; // if have a default, default value
    DefaultKind default_kind = DefaultKind::None; // if have a default, the default Datatype
    // field index props
    bool is_indexed = false; // the resulted table field will be indexed
    std::string index_type;  // index asc or desc order
    bool is_unique = false;  // index unique
    std::string index_name; // field index name

};

struct OrmIndex {
    std::vector<std::string> fields;
    std::string type;
    bool unique = false;
    std::string index_name;
};

class DDLVisitor; // forward declaration

class OrmSchema {
public:
    int64_t id = 0; // must load the id if exists
    std::string name;
    std::unique_ptr<OrmSchema> parent;
    int version;
    bool applied;
    std::string json;
    std::vector<OrmField> fields;
    std::vector<OrmIndex> indexes;
    void accept(class DDLVisitor& visitor) const;
    static bool from_json(const nlohmann::json& j, OrmSchema& schema);
};
