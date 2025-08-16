#include "orm.hpp"
#include "visitor.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>

bool OrmSchema::from_json(const nlohmann::json& j, OrmSchema& schema) {

    // --- NEW: resolve schema/table name ---
    if (auto it = j.find("name"); it != j.end() && it->is_string()) {
        schema.name = it->get<std::string>();
    } else if (auto it = j.find("title"); it != j.end() && it->is_string()) {
        schema.name = it->get<std::string>();
    } else if (auto it = j.find("$id"); it != j.end() && it->is_string()) {
        std::string v = it->get<std::string>();
        auto pos = v.find_last_of("/#");
        schema.name = (pos == std::string::npos) ? v : v.substr(pos + 1);
    } else {
        schema.name = "unnamed";
    }

    if (!j.contains("properties")) return false;

    schema.fields.clear();
    auto props = j["properties"];
    std::vector<std::string> required_list;
    if (j.contains("required")) {
        required_list = j["required"].get<std::vector<std::string>>();
    }
    for (auto it = props.begin(); it != props.end(); ++it) {
        OrmField field;
        field.name = it.key();
        const auto& v = it.value();
        field.type = v.value("type", "string");
        field.encoding = v.value("encoding", "");
        field.required = (std::find(required_list.begin(), required_list.end(), field.name) != required_list.end());
        field.is_id = v.value("idprop", false);
        if (field.is_id) {
            std::string kind_str = v.value("idkind", "uuidv7");
            IdKind kind = IdKind::UUIDv7;
            if (kind_str == "highlow") kind = IdKind::HighLow;
            else if (kind_str == "snowflake") kind = IdKind::Snowflake;
            else if (kind_str == "dbserial") kind = IdKind::DBSerial;
            else if (kind_str == "tbserial") kind = IdKind::TBSerial;
            field.id_kind = kind;
        }
        field.is_indexed = v.value("index", false);
        field.index_type = v.value("indexType", "");
        field.is_unique = v.value("unique", false);
        if (v.contains("default")) {
          const auto& d = v["default"];
          if (d.is_string()) {
              field.default_kind  = DefaultKind::String;
              field.default_value = d.get<std::string>();      // unquoted text
          } else if (d.is_boolean()) {
              field.default_kind  = DefaultKind::Boolean;
              field.default_value = d.get<bool>() ? "true" : "false";
          } else if (d.is_number()) {
              field.default_kind  = DefaultKind::Number;
              field.default_value = d.dump();                  // numeric literal
          } else if (d.is_null()) {
              field.default_kind  = DefaultKind::Raw;
              field.default_value = "NULL";
          } else {
              // arrays/objects → store JSON (PG JSONB or text-as-JSON, up to visitor)
              field.default_kind  = DefaultKind::Raw;
              field.default_value = d.dump();
          }
      } else {
          field.default_kind  = DefaultKind::None;
          field.default_value.clear();
      }
        field.index_name = v.value("indexName", "");
        schema.fields.push_back(field);
    }
    schema.indexes.clear();
    if (j.contains("indexes")) {
        for (const auto& idx : j["indexes"]) {
            OrmIndex index;
            if (idx.contains("fields")) {
                for (const auto& fld : idx["fields"]) {
                    index.fields.push_back(fld.get<std::string>());
                }
            }
            index.type = idx.value("type", "");
            index.unique = idx.value("unique", false);
            index.index_name = idx.value("indexName", "");
            schema.indexes.push_back(index);
        }
    }
    return true;
}

void OrmSchema::accept(DDLVisitor& visitor) const {
    visitor.visit(static_cast<const void*>(this));
}
