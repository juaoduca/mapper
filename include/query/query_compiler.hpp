#pragma once
#include <stdexcept>
#include "query_ast.hpp"
#include "lib.hpp"

namespace ql {

    class QueryCompiler {
    public:
        ql::QueryDoc compile(const char* doc); // throws std::runtime_error on parse errors
    };

} // namespace ql..
