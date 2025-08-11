#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

enum class IdKind { UUIDv7, HighLow, Snowflake, DBSerial, TBSerial };

struct OrmField {
    std::string name;
    std::string type;
    std::string encoding;
    bool required = false;
    bool is_primary_key = false;
    IdKind pk_kind = IdKind::UUIDv7;
    bool is_indexed = false;
    std::string index_type;
    bool is_unique = false;
    std::string default_value;
    std::string index_name;
};

struct OrmIndex {
    std::vector<std::string> fields;
    std::string type;
    bool unique = false;
    std::string index_name;
};

class OrmSchemaVisitor; // forward declaration

class OrmSchema {
public:
    std::vector<OrmField> fields;
    std::vector<OrmIndex> indexes;

    void accept(class OrmSchemaVisitor& visitor) const;
    static bool from_json(const nlohmann::json& j, OrmSchema& schema);
};
