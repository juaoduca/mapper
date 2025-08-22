#pragma once
#include <string>
#include <vector>
#include <stdexcept>
#include "orm.hpp"
#include "lib.hpp"

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

    virtual dml_pair insert (const OrmSchema& schema, const jval& value) const = 0;
    virtual dml_pair upsert (OrmSchema& schema, const jval& value) const = 0;
    virtual dml_pair update (OrmSchema& schema, const jval& value) const = 0;
    virtual dml_pair remove (OrmSchema& schema, const jval& value) const = 0;

protected:
    // 1-based placeholder
    virtual std::string ph(size_t index1) const = 0;
};

class SqliteDMLVisitor final : public DMLVisitor {
public:
    dml_pair insert (const OrmSchema& schema, const jval& value) const override;
    dml_pair upsert (OrmSchema& schema, const jval& value) const override;
    dml_pair update (OrmSchema& schema, const jval& value) const override;
    dml_pair remove (OrmSchema& schema, const jval& value) const override;
private:
    std::string ph(size_t index1) const override; // ?1
};

class PgDMLVisitor final : public DMLVisitor {
public:
    dml_pair insert (const OrmSchema& schema, const jval& value) const override;
    dml_pair upsert (OrmSchema& schema, const jval& value) const override;
    dml_pair update (OrmSchema& schema, const jval& value) const override;
    dml_pair remove (OrmSchema& schema, const jval& value) const override;
private:
    std::string ph(size_t index1) const override; // $1
};
