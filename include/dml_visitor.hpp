#pragma once
#include "orm.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <stdexcept>

/**
 * DML generation using only fields present in the JSON payload.
 * - If data is an array, the FIRST object defines the column set.
 * - Placeholder style:
 *     SQLite   -> ?1, ?2, ...
 *     Postgres -> $1, $2, ...
 * - Parameter order:
 *     insert/upsert: selected fields (schema order)
 *     update:        selected non-PK fields, then PK last
 *     delete:        PK only (first param)
 * - No RETURNING is emitted.
 */
class DMLVisitor {
public:
    virtual ~DMLVisitor() = default;

    virtual std::string insert (const OrmSchema& schema, const nlohmann::json& data) const = 0;
    virtual std::string upsert (const OrmSchema& schema, const nlohmann::json& data) const = 0;
    virtual std::string update (const OrmSchema& schema, const nlohmann::json& data) const = 0;
    virtual std::string remove (const OrmSchema& schema, const nlohmann::json& data) const = 0;

protected:
    static const OrmField* find_pk(const OrmSchema& schema);
    static const nlohmann::json& first_object(const nlohmann::json& data);
    static std::vector<const OrmField*> select_fields_in_order(const OrmSchema& schema,
                                                               const nlohmann::json& obj,
                                                               bool exclude_pk);
    virtual std::string ph(size_t index1) const = 0; // 1-based placeholder
};

class SqliteDMLVisitor final : public DMLVisitor {
public:
    std::string insert (const OrmSchema& schema, const nlohmann::json& data) const override;
    std::string upsert (const OrmSchema& schema, const nlohmann::json& data) const override;
    std::string update (const OrmSchema& schema, const nlohmann::json& data) const override;
    std::string remove (const OrmSchema& schema, const nlohmann::json& data) const override;
private:
    std::string ph(size_t index1) const override; // ?1
};

class PgDMLVisitor final : public DMLVisitor {
public:
    std::string insert (const OrmSchema& schema, const nlohmann::json& data) const override;
    std::string upsert (const OrmSchema& schema, const nlohmann::json& data) const override;
    std::string update (const OrmSchema& schema, const nlohmann::json& data) const override;
    std::string remove (const OrmSchema& schema, const nlohmann::json& data) const override;
private:
    std::string ph(size_t index1) const override; // $1
};
