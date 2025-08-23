#include "orm.hpp"
#include <algorithm>
#include <optional>
#include <functional> // Required for std::reference_wrapper
#include "lib.hpp"
#include "jsonhlp.hpp"

bool OrmSchema::from_json( jdoc& doc, OrmSchema& schema) {
    // local variables
    jdaloc a = doc.GetAllocator();
    jit itnull = doc.MemberEnd(); // null iterator for defautl returns
    const jval& valnull = jval("null", rapidjson::kNullType, a); // null value to default return
    jval& j = doc;
    // local helpers
    const auto isrequired = [&](std::string name)-> bool {
        if (!j.HasMember(PROP_REQUIRED)) { return false; }
        const jval& reqs = j.FindMember(PROP_REQUIRED)->value;
        if (!reqs.IsArray()) { return false; }
        for (const auto& el: reqs.GetArray() ) {
            if (el.IsString() && el.GetString() == name) {
                return true;
            }
        }
        return false;
    };

    // --- NEW: resolve schema/table name ---
    schema.name = jhlp::get<std::string>(j, PROP_NAME, "null" );
    if (schema.name == "null" ) jhlp::get<std::string>(j, PROP_TITLE, "null");

    schema.fields.clear();

    if (!j.HasMember(PROP_PROPERTIES)) return false;
    jit props = j.FindMember(PROP_PROPERTIES);
    jit reqs = j.FindMember(PROP_REQUIRED);
    for (jit itprop = props->value.MemberBegin(); itprop != props->value.MemberEnd(); itprop++) { //} props.begin(); it != props.end(); ++it) {
        OrmProp field;
        field.name = itprop->name.GetString();
        const jval& prop = itprop->value;
        field.type = proptype( jhlp::get<std::string>(prop, "type") );
        field.encoding = jhlp::get<std::string>(prop, PROP_ENCODING);
        field.required = isrequired(field.name);
        field.is_id = jhlp::get<bool>(prop, PROP_ID_PROP);
        if (field.is_id) {
            std::string kind_str = jhlp::get<std::string>(prop, PROP_ID_KIND);
            IdKind kind = IdKind::UUIDv7;
            if (kind_str == "highlow") kind = IdKind::HighLow;
            else if (kind_str == "snowflake") kind = IdKind::Snowflake;
            else if (kind_str == "dbserial") kind = IdKind::DBSerial;
            else if (kind_str == "tbserial") kind = IdKind::TBSerial;
            field.id_kind = kind;
        }
        field.is_indexed = jhlp::get<bool>(prop, PROP_INDEX);//, false);
        field.index_type = jhlp::get<std::string>(prop, PROP_INDEX_TYPE);//, "");
        field.is_unique = jhlp::get<bool>(prop, PROP_UNIQUE);//, false);
        field.default_kind  = DefaultKind::None;
        field.default_value.clear();
        if (prop.HasMember(PROP_DEFAULT)) {
            const jval& def = prop.FindMember(PROP_DEFAULT)->value;
            if (!def.IsNull()) {
                if (def.IsString()) {
                    field.default_kind  = DefaultKind::String;
                    field.default_value = def.GetString(); //.get<std::string>();      // unquoted text
                } else if (def.IsBool()) {
                    field.default_kind  = DefaultKind::Boolean;
                    field.default_value = def.GetBool() ? "true" : "false";
                } else if (def.IsNumber()) { //|| def.IsInt() || def.IsInt64() || def.IsFloat() || def.IsDouble() ||   ) {
                    field.default_kind  = DefaultKind::Number;
                    field.default_value = jhlp::dump(def); // dumb all number types to string
                } else {
                    // arrays/objects → store JSON (PG JSONB or text-as-JSON, up to visitor)
                    field.default_kind  = DefaultKind::Raw;
                    field.default_value = jhlp::dump(def);
                }
            } else { // default prop of the field is null
                    field.default_kind  = DefaultKind::Raw;
                    field.default_value = VAL_NULL;
            }
        }
        field.index_name = jhlp::get<std::string>(prop, PROP_INDEX_NAME, "");//, "");
        schema.fields[field.name] = field;
    }
    schema.indexes.clear();
    if (j.HasMember(PROP_INDEXES)) {  // indexes is an array of objects
        const jval& idxs = j.FindMember(PROP_INDEXES)->value;
        if (idxs.IsArray()) {
            for (const auto& idx : idxs.GetArray()) {
                OrmIndex index;
                if (idx.HasMember(PROP_FIELDS)) {  // fields é outra array
                    const jval& flds = idx.FindMember(PROP_FIELDS)->value;
                    if (flds.IsArray()) {
                        for (const auto& fld : flds.GetArray()) {
                            index.fields.push_back(fld.GetString());
                        }
                    }
                }
                index.type = jhlp::get<std::string>(idx, PROP_INDEX_TYPE);
                index.unique = jhlp::get<bool>(idx, PROP_UNIQUE);
                index.index_name = jhlp::get<std::string>(idx, PROP_INDEX_NAME);
                schema.indexes.push_back(index);
            }
        }
    }
    return true;
}

// void OrmSchema::accept(DDLVisitor& visitor) const {
//     visitor.visit(*this);
// }


const std::shared_ptr<OrmProp> OrmSchema::idprop() const {
    for (auto& pair : fields) {
        if (pair.second.is_id) {
            // Return a reference to the found object
            return std::make_shared<OrmProp>(pair.second);
        }
        if (pair.second.name == "id") {
            return std::make_shared<OrmProp>(pair.second);
        }
    }
    // Return an empty optional if no ID property is found
    THROW("Schema: '%s' have no ID Prop", name);
    return nullptr;
}


PropType proptype(std::string type) {
    if (type == "string"   ) return PropType::String   ;
    if (type == "integer"  ) return PropType::Integer  ;
    if (type == "number"   ) return PropType::Number   ;
    if (type == "boolean"  ) return PropType::Bool     ;
    if (type == "date"     ) return PropType::Date     ;
    if (type == "time"     ) return PropType::Time     ;
    if (type == "datetime" ) return PropType::Dt_Time  ;
    if (type == "timestamp") return PropType::Tm_Stamp ;
    if (type == "binary"   ) return PropType::Bin      ;
    if (type == "json"     ) return PropType::Json     ;
    THROW("Invalid type name: %s" , type);
    return PropType::String;
}

std::string proptype(PropType type) {
    if (type == PropType::String  ) return  "string"   ;
    if (type == PropType::Integer ) return  "integer"  ;
    if (type == PropType::Number  ) return  "number"   ;
    if (type == PropType::Bool    ) return  "boolean"  ;
    if (type == PropType::Date    ) return  "date"     ;
    if (type == PropType::Time    ) return  "time"     ;
    if (type == PropType::Dt_Time ) return  "datetime" ;
    if (type == PropType::Tm_Stamp) return  "timestamp";
    if (type == PropType::Bin     ) return  "binary"   ;
    if (type == PropType::Json    ) return  "json"     ;
    THROW("Invalid proptype value: %d", type);
    return "";
}
