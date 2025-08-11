#include "orm.hpp"
#include "visitor.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>

bool OrmSchema::from_json(const nlohmann::json& j, OrmSchema& schema) {
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
        field.is_primary_key = v.value("primaryKey", false);
        field.is_indexed = v.value("index", false);
        field.index_type = v.value("indexType", "");
        field.is_unique = v.value("unique", false);
        field.default_value = v.contains("default") ? v["default"].dump() : "";
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

void OrmSchema::accept(OrmSchemaVisitor& visitor) const {
    visitor.visit(*this);
}
