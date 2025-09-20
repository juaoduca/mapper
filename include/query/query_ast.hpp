#pragma once
#include <string>
#include <vector>
#include <optional>

namespace ql {

    // New: date/time scalar function on a field
    enum class DtFuncKind { None, Date, Time, TimeMs, Mask };

    struct DtFunc {
        DtFuncKind kind{DtFuncKind::None};
        // when kind==Mask, compact letters from {y,m,d,h,n,s}, e.g. "ym", "mdh", "ns"
        std::string mask;
    };

    enum class AggKind { None, Count, Avg, Sum };

    struct Field {
        std::string name;                        // case-preserved
        std::optional<std::string> alias;        // case-preserved (alias: Name)
        std::vector<Field> subselection;         // reserved for future nesting
        AggKind agg = AggKind::None;             // None | Count | Avg | Sum
        std::optional<DtFunc> dt;                // <- NEW
        bool groupBy = false;                    // true if parsed from ".groupby"
    };

    struct QueryDoc {
        std::string rootTypeName;                // OrmSchema name (case-preserved)
        std::vector<Field> selectionSet;         // top-level selected fields
    };

} // namespace ql ..
