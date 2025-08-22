#pragma once
#include <string>
#include <vector>
#include <optional>
#include <functional> // Required for std::reference_wrapper
#include <string_view>
#include <stdexcept>
#include "jsonhlp.hpp"

/****************** LIERAL CONSTS */
#define VAL_NULL        "NULL"
#define PROP_NAME       "name"
#define PROP_TITLE      "title"
#define PROP_PROPERTIES "properties"
#define PROP_INDEXES    "indexes"
#define PROP_DEFAULT    "default"
#define PROP_REQUIRED   "required"

#define PROP_INDEX      "index"
#define PROP_INDEX_NAME "indexName"
#define PROP_INDEX_TYPE "indexType"
#define PROP_FIELDS     "fields"
#define PROP_UNIQUE     "unique"

#define PROP_ENCODING   "encoding"
#define PROP_ID_PROP    "idprop"
#define PROP_ID_KIND    "idkind"


/**********  helpers public functions - lib  ***********/
enum class IdKind { UUIDv7, HighLow, Snowflake, DBSerial, TBSerial };
enum class DefaultKind { None, String, Boolean, Number, Raw };
enum class PropType { String, Integer, Number, Bool, Date, Time, Dt_Time, Tm_Stamp, Bin, Json};
enum class Dialect {SQLite, Postgres};

class OrmSchema; // fw decl

struct OrmProp {
    std::string name; // prop/field name
    std::string schema_name; // schema name the prop belongs
    PropType    type; // prop Dataype / field Datatype
    bool        is_id = false; // if the prop/field is an ID field/prop
    IdKind      id_kind = IdKind::UUIDv7; // define the generation of the ID
    bool        required = false; //must be filled or not
    std::string encoding; // encoding type for binary data (yEnc, Base64 etc)
    std::string default_value; // if have a default, default value
    DefaultKind default_kind = DefaultKind::None; // if have a default, the default Datatype
    // field index props
    bool        is_indexed = false; // the resulted table field will be indexed
    std::string index_type; // index asc or desc order
    bool        is_unique = false; // index unique
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
    OrmSchema* parent; // pointer to parent
    int version;
    bool applied;
    std::string json;
    std::unordered_map<std::string, OrmProp> fields;
    std::vector<OrmIndex> indexes;

    // void accept(class DDLVisitor& visitor) const;
    const OrmProp& idprop() const;
    static bool from_json(jdoc& doc, OrmSchema& schema);
};

PropType proptype(std::string type);
// {
//     if (type == "string"   ) return PropType::String   ;
//     if (type == "integer"  ) return PropType::Integer  ;
//     if (type == "number"   ) return PropType::Number   ;
//     if (type == "boolean"  ) return PropType::Bool     ;
//     if (type == "date"     ) return PropType::Date     ;
//     if (type == "time"     ) return PropType::Time     ;
//     if (type == "datetime" ) return PropType::Dt_Time  ;
//     if (type == "timestamp") return PropType::Tm_Stamp ;
//     if (type == "binary"   ) return PropType::Bin      ;
//     if (type == "json"     ) return PropType::Json     ;
//     throw std::runtime_error("Invalid type name: "+type);
// }

std::string proptype(PropType type);
// {
//     if (type == PropType::String  ) return  "string"   ;
//     if (type == PropType::Integer ) return  "integer"  ;
//     if (type == PropType::Number  ) return  "number"   ;
//     if (type == PropType::Bool    ) return  "boolean"  ;
//     if (type == PropType::Date    ) return  "date"     ;
//     if (type == PropType::Time    ) return  "time"     ;
//     if (type == PropType::Dt_Time ) return  "datetime" ;
//     if (type == PropType::Tm_Stamp) return  "timestamp";
//     if (type == PropType::Bin     ) return  "binary"   ;
//     if (type == PropType::Json    ) return  "json"     ;
//     throw std::runtime_error("Invalid proptype name: "+int(type));
// }

