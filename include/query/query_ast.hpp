#pragma once
#include <string>
#include <vector>
#include <optional>

namespace ql {

enum class AggKind { None, Count, Avg, Sum };

struct Field {
    std::string name;                        // case-preserved
    std::optional<std::string> alias;        // case-preserved (alias: Name)
    std::vector<Field> subselection;         // reserved for future nesting
    AggKind agg = AggKind::None;             // None | Count | Avg
    bool groupBy = false;                    // true if parsed from ".groupby"
};

struct QueryDoc {
    std::string rootTypeName;                // OrmSchema name (case-preserved)
    std::vector<Field> selectionSet;         // top-level selected fields
};

} // namespace ql ..
