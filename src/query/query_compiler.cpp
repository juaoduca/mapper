
#include <string>
#include <string_view>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include "lexer.h"
#include "query_compiler.hpp"

using namespace ql;

namespace {
    // simple ASCII case-insensitive compare for keywords like "groupby"
    static bool iequals(std::string_view a, std::string_view b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            unsigned char ca = static_cast<unsigned char>(a[i]);
            unsigned char cb = static_cast<unsigned char>(b[i]);
            if (std::tolower(ca) != std::tolower(cb)) return false;
        }
        return true;
    }
}

ql::QueryDoc QueryCompiler::compile(const char* doc) {
    if (!doc) throw std::runtime_error("null document");
    Lexer lx = init_lexer(const_cast<char*>(doc));

    // 1) '{'
    Token t = next_token(&lx);
    if (t.type != ttL_CURLY) throw std::runtime_error("Query must start with '{'");

    // 2) Root identifier (OrmSchema name)
    t = next_token(&lx);
    if (t.type != ttIDENTF) throw std::runtime_error("Expected root type identifier after '{'");
    QueryDoc qd;
    qd.rootTypeName = t.value; // preserve case

    // 3) SelectionSet: '{' field (',' field)* '}'
    t = next_token(&lx);
    if (t.type != ttL_CURLY) throw std::runtime_error("Expected '{' to start selection set");

    // fields
    while (true) {
        Token look = next_token(&lx);
        if (look.type == ttR_CURLY) break; // empty set or end

        Field f{};
        if (look.type != ttIDENTF) throw std::runtime_error("Expected field name or '}'");
        char peek = peek_char(&lx);
        if (peek == ':') {
            f.alias = look.value; // alias name
            next_token(&lx); //advance to ignore collon
            Token fieldName = next_token(&lx);
            if (fieldName.type != ttIDENTF) throw std::runtime_error("Expected field name after ':'");
            f.name  = fieldName.value;
        } else {
            f.name = look.value;
        }

        Token tok = next_token(&lx); // maybe '.' or a delimiter
        if (tok.type == ttDOT) {
            Token suf = next_token(&lx);
            if (suf.type == ttCOUNT) { f.agg = AggKind::Count;}
            else if (suf.type == ttAVG) { f.agg = AggKind::Avg;}
            else if (suf.type == ttSUM) { f.agg = AggKind::Sum;}
            else if (suf.type == ttIDENTF && lib::istrcmp(suf.value, "groupby")) { f.groupBy = true; }
            else { throw std::runtime_error("Unknown suffix after '.'"); }
            tok = next_token(&lx); // maybe '.' or a delimiter
        }



        // if (tok.type == ttCOLON) { // alias:   <alias> : <field> [ .suffix ]
        //     Token fieldName = next_token(&lx);
        //     if (fieldName.type != ttIDENTF) throw std::runtime_error("Expected field name after ':'");
        //     f.alias = left;
        //     f.name  = fieldName.value;

        //     tok = next_token(&lx); // maybe '.' or a delimiter
        //     if (tok.type == ttDOT) {
        //         Token suf = next_token(&lx);
        //         if (suf.type == ttCOUNT) { f.agg = AggKind::Count;}
        //         else if (suf.type == ttAVG) { f.agg = AggKind::Avg;}
        //         else if (suf.type == ttSUM) { f.agg = AggKind::Sum;}
        //         else if (suf.type == ttIDENTF && lib::istrcmp(suf.value, "groupby")) { f.groupBy = true; }
        //         else { throw std::runtime_error("Unknown suffix after '.'"); }
        //         tok = next_token(&lx); // delimiter after suffix
        //     }
        // } else { // no alias:   <field> [ .suffix ]
        //     f.name = left;
        //     if (tok.type == ttDOT) {
        //         Token suf = next_token(&lx);
        //         if (suf.type == ttCOUNT) { f.agg = AggKind::Count;}
        //         else if (suf.type == ttAVG) { f.agg = AggKind::Avg;}
        //         else if (suf.type == ttSUM) { f.agg = AggKind::Sum;}
        //         else if (suf.type == ttIDENTF && lib::istrcmp(suf.value, "groupby")) { f.groupBy = true; }
        //         else { throw std::runtime_error("Unknown suffix after '.'"); }
        //         tok = next_token(&lx); // delimiter after suffix
        //     }
        // }

        // delimiters
        if (tok.type == ttCOMMA || (tok.type != ttR_CURLY && peek_char(&lx) == ' ') ) {
            qd.selectionSet.push_back(std::move(f));
            continue;
        } else if (tok.type == ttR_CURLY) {
            qd.selectionSet.push_back(std::move(f));
            break;
        } else {
            throw std::runtime_error("Expected ',' or '}' after field");
        }
    }

    // 4) Final '}' (closing the doc)
    t = next_token(&lx);
    if (t.type != ttR_CURLY) throw std::runtime_error("Expected '}' to close query document");

    return qd;
}
