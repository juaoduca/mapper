#pragma once
#include <string>
#include <vector>
#include <optional>
#include <functional> // Required for std::reference_wrapper
#include <string_view>
#include <stdexcept>
#include "jsonhlp.hpp"

/**********  helpers public functions - lib  ***********/
enum class IdKind { UUIDv7, HighLow, Snowflake, DBSerial, TBSerial };
enum class DefaultKind { None, String, Boolean, Number, Raw, Null };
enum class PropType { String, Integer, Number, Bool, Date, Time, Dt_Time, Tm_Stamp, Bin, Json, Object};
enum class Dialect {SQLite, Postgres};

class OrmSchema; // fw decl
class OrmProp; // fw decl

/** callback to OrmSchema::from_json() function to find an ref_schema */
// using InsertCatalogFn = std::function<bool (const OrmSchema &new_Schema)>;
using GetSchemaFn = std::function<std::shared_ptr<OrmSchema> (const std::string &schema_name)>;


struct OrmProp {
    std::string              name                            ; // prop/field name
    std::weak_ptr<OrmSchema> parent                          ; // schema name the prop belongs
    bool                     is_id        = false            ; // if the prop/field is an ID field/prop
    IdKind                   id_kind      = IdKind::UUIDv7   ; // define the generation of the ID
    PropType                 type                            ; // prop Dataype / field Datatype
    std::string              encoding                        ; // encoding type for binary data (yEnc, Base64 etc)
    std::weak_ptr<OrmSchema> ref_Schema                      ; // foreink schema
    std::weak_ptr<OrmProp>   ref_Field                       ; // foreing prop FK
    bool                     required     = false            ; //must be filled or not
    DefaultKind              default_kind = DefaultKind::None; // if have a default, the default Datatype
    std::string              default_value                   ; // if have a default, default value
    // field index props
    std::string               index_name                     ; // field index name
    std::string               index_type                     ; // index asc or desc order
    bool                      is_indexed = false             ; // the resulted table field will be indexed
    bool                      is_unique  = false             ; // index unique
};

struct OrmIndex {
    std::vector<std::string> fields;
    std::string type;
    bool unique = false;
    std::string index_name;
};

class DDLVisitor; // forward declaration

class OrmSchema {
private:
    mutable std::shared_ptr<OrmProp> id_prop = nullptr;
public:
    int64_t            id = 0; // must load the id if exists
    std::string        name; // unique
    int                version = -1; // changes control
    bool               applied = false; // avoid useless re-processing
    mutable DateTime   applied_at; // value as a chrono time_point
    std::string        json = "{}"; // JSONchema string with data structure
    std::weak_ptr<OrmSchema> parent; // pointer to parent
    mutable std::vector<OrmSchema> ladder; // the parent chain to the root parent
    // mutable std::shared_ptr<CatalogItem> catalog_item;
    std::unordered_map<std::string, std::shared_ptr<OrmProp>> fields; // keep insertion order - reflected on DB Tables
    std::vector<OrmIndex> indexes;
    const std::shared_ptr<OrmProp> idprop() const ;
    static bool from_json(std::string JSON, OrmSchema& schema, GetSchemaFn getRefSchema = nullptr);
    static bool from_json(jdoc& doc, OrmSchema& schema, GetSchemaFn getRefSchema = nullptr);
};

PropType proptype(const std::string &type); // impl on orm.cpp

std::string proptype(PropType type); // impl on orm.cpp
