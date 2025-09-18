#include "orm.hpp"
#include <algorithm>
#include <optional>
#include <functional> // Required for std::reference_wrapper

#define REF_SCHEMA_ERROR "Referenced Schema: %s referenced by: %s was not found!"

bool OrmSchema::from_json(std::string JSON, OrmSchema& schema, GetRefSchemaFn getRefSchema) {
    jdoc doc = jhlp::parse_str(JSON);
    return OrmSchema::from_json(doc, schema, getRefSchema);
}

bool OrmSchema::from_json( jdoc& doc, OrmSchema& schema, GetRefSchemaFn getRefSchema ) {
    // local variables
    // jdaloc a = doc.GetAllocator();
    jval& j = doc;
    // object level required array prop
    auto reqs_it = j.FindMember(PROP_REQUIRED);
    // local helpers
    const auto isrequired = [&](const jval &prop, std::string &name)-> bool {
        //prop member required bool prop
        auto it = prop.FindMember(PROP_REQUIRED) ;
        if (it != prop.MemberEnd()) { return it->value.GetBool(); }
        if (reqs_it == j.MemberEnd()) { return false; }
        const jval& reqs = reqs_it->value;
        if (!reqs.IsArray()) { return false; }
        for (const auto& el: reqs.GetArray() ) {
            if (el.IsString() && el.GetString() == name) {
                return true;
            }
        }
        return false;
    };

    // --- NEW: resolve schema/table name ---
    schema.name = json::getString(j, PROP_NAME, "");
    if (schema.name.empty() ) json::getString(j, PROP_TITLE, "");

    schema.fields.clear();

    if (!j.HasMember(PROP_PROPERTIES)) return false;
    auto props = j.FindMember(PROP_PROPERTIES);
    for (auto itprop = props->value.MemberBegin(); itprop != props->value.MemberEnd(); itprop++) { //} props.begin(); it != props.end(); ++it) {
        OrmProp field;
        field.name = itprop->name.GetString();
        const jval& prop = itprop->value;
        field.type = proptype( json::getString(prop, PROP_TYPE, "") );
        if(field.type == PropType::Object) {
            if (getRefSchema) {
                std::string ref_schema_name = json::getString(prop, PROP_REF_SCHEMA, "");
                std::string ref_prop_name   = json::getString(prop, PROP_REF_PROP  , "");
                std::shared_ptr<OrmSchema> refschema = getRefSchema( ref_schema_name );
                if (!refschema) {THROW(REF_SCHEMA_ERROR, ref_schema_name.c_str(), schema.name.c_str()); }
                field.ref_Field = refschema->fields.find(ref_prop_name)->second;
                field.ref_Schema = refschema;
            }
        }
        field.encoding = json::getString(prop, PROP_ENCODING, "");
        field.required = isrequired(prop, field.name);
        field.is_id = json::getBool(prop, PROP_ID_PROP, false);
        if (field.is_id) {
            std::string kind_str = json::getString(prop, PROP_ID_KIND, "");
            std::string lower_kind_str = lib::tolower(kind_str);
            IdKind kind;
            if (lower_kind_str == "highlow") kind = IdKind::HighLow;
            else if (lower_kind_str == "snowflake") kind = IdKind::Snowflake;
            else if (lower_kind_str == "dbserial") kind = IdKind::DBSerial;
            else if (lower_kind_str == "tbserial") kind = IdKind::TBSerial;
            else if (lower_kind_str == "uuidv7") kind = IdKind::UUIDv7;
            field.id_kind = kind;
        }
        field.is_indexed = json::getBool(prop, PROP_INDEX     , false)  ;//, false);
        field.index_type = json::getString(prop, PROP_INDEX_TYPE, "");//, "");
        field.index_name = json::getString(prop, PROP_INDEX_NAME, "");//, "");
        field.is_unique  = json::getBool(prop, PROP_UNIQUE    , false)  ;//, false);
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
                    field.default_value = jhlp::asString(def); // dump all number types to unquoted string
                } else {
                    // arrays/objects → store JSON (PG JSONB or text-as-JSON, up to visitor)
                    field.default_kind  = DefaultKind::Raw;
                    field.default_value = jhlp::asString(def);
                }
            } else { // treat JSON null as DEAFULT NULL
                field.default_kind  = DefaultKind::Null;
                field.default_value = "NULL";
            }
        }
        schema.fields[field.name] = std::make_shared<OrmProp>(field);
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
                index.type       = json::getString(idx, PROP_INDEX_TYPE, ""   );
                index.unique     = json::getBool  (idx, PROP_UNIQUE    , false);
                index.index_name = json::getString(idx, PROP_INDEX_NAME, ""   );

                schema.indexes.push_back(index);
            }
        }
    }
    return true;
}

const std::shared_ptr<OrmProp> OrmSchema::idprop() const {
    if (id_prop != nullptr) {
        return id_prop;
    }
    for (auto& pair : fields) {
        if (pair.second->is_id) {
            // Return a reference to the found object
            id_prop = pair.second;
            return pair.second;
        }
        if (pair.second->name == "id") {
            id_prop = pair.second;
            return pair.second;
        }
    }
    // Return an empty optional if no ID property is found
    THROW("Schema: '%s' have no ID Prop", name.c_str());
    return nullptr;
}

PropType proptype(const std::string &type) {
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
    if (type == "object"   ) return PropType::Object   ;
    THROW("Invalid type name: %s" , type.c_str());
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
    if (type == PropType::Object  ) return  "object"   ;
    THROW("Invalid proptype value: %d", type);
    return "";
}
