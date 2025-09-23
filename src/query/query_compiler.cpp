
#include <string>
#include <string_view>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include "lexer.h"
#include "query_compiler.hpp"

using namespace ql;

namespace {
    // valid mask is non-empty, only letters y m d h n s
    static bool dt_mask_ok(const std::string& s) {
        if (s.empty()) return false;
        for (char c : s) {
            if (c!='y' && c!='m' && c!='d' &&
                c!='h' && c!='n' && c!='s') return false;
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
        t = next_token(&lx);
        if (t.type == ttR_CURLY) break; // empty set or end {query {}}

        Field f{};
        if (t.type != ttIDENTF) throw std::runtime_error("Expected field name or '}'");
        char peek = peek_char(&lx);
        // 1 - ALIAS
        if (peek == ':') { // field_token is an Alias => [Alias]:FieldName.Function
            f.alias = t.value; // alias name
            next_token(&lx); //advance to ignore collon
            t = next_token(&lx); // next tokem must be the field name
            if (t.type != ttIDENTF) throw std::runtime_error("Expected field name after ':'");
            f.name  = t.value; //
            //for future - next_token() can be l_paren for field args [Alias]:FieldName(args).function.function ...
        } else {
        // 1.1 - NO ALIAS - FIELD NAME
            f.name = t.value;
        }


        // 2 - chained .func parts, terminal .groupby allowed
        while (peek_char(&lx) == '.') {
            next_token(&lx); // consume '.'
            t = next_token(&lx);
            std::string fn = lib::tolower(t.value);
            if (t.type == ttIDENTF) {
                //2.1 - GROUPBY AS A TERMINATOR
                if (fn == "groupby") {
                    f.groupBy = true;
                    // terminal; break the chain (ignore further parts if user wrote them)
                    break;
                }
                //2.2 - datetime scalar functions or compact mask (ymd/ym/dh/etc)
                if (fn=="date" || fn=="time" || fn=="timems" || dt_mask_ok(fn)) {
                    DtFunc dt;
                    if      (fn=="date"  ) dt.kind = DtFuncKind::Date;
                    else if (fn=="time"  ) dt.kind = DtFuncKind::Time;
                    else if (fn=="timems") dt.kind = DtFuncKind::TimeMs;
                    else { dt.kind = DtFuncKind::Mask; dt.mask = fn; }
                    f.dt = std::move(dt);
                } else { // 2.N - FUTURE TYPES OF FUNCTIONS
                    // unknown identifier after dot: treat as error for now
                    THROW("Unknown function for field: %s Function:%s ", f.name, fn);
                }
            // 2.1 - OTHERS TYPES OF AGG FUNCs
            } else if (t.type == ttCOUNT) { f.agg = AggKind::Count; }
              else if (t.type == ttAVG  ) { f.agg = AggKind::Avg;   }
              else if (t.type == ttSUM  ) { f.agg = AggKind::Sum;   }
              else {
                  THROW("Unknown function for field: %s Function token type", f.name);
              }
        } // NO MORE DOTs - NEXT FIELD || NESTED OBJ || CLOSE CURLY ttR_CURLY }

        Token look = next_token(&lx);
        // 3 -  NESTED OBJ - CONSIDER A RECURSIVE FUNCTION CALL
        if (look.type == ttL_CURLY) {
            // --- Parse nested subselection: owner { id, name, ... }
            while (true) {
                Token nt = next_token(&lx);
                if (nt.type == ttR_CURLY) break; // if no curlies owner, ... || empty curlies {} => show the field value
                if (nt.type != ttIDENTF) throw std::runtime_error("Expected field name or '}' in subselection");

                Field nf{};
                // alias support inside nested: alias:field
                if (peek_char(&lx) == ':') {
                    next_token(&lx); // ':'
                    Token id2 = next_token(&lx);
                    if (id2.type != ttIDENTF) throw std::runtime_error("Expected field name after ':' in subselection");
                    nf.alias = nt.value;
                    nf.name  = id2.value;
                } else {
                    nf.name = nt.value;
                }

                // Optional single dt function on nested field
                if (peek_char(&lx) == '.') {
                    next_token(&lx);               // '.'
                    Token ft = next_token(&lx);    // function name
                    std::string fn2 = lib::tolower(ft.value);
                    if (ft.type == ttIDENTF) {
                        if (fn2=="date" || fn2=="time" || fn2=="timems" || dt_mask_ok(fn2)) {
                            DtFunc dt2;
                            if      (fn2=="date"  ) dt2.kind = DtFuncKind::Date;
                            else if (fn2=="time"  ) dt2.kind = DtFuncKind::Time;
                            else if (fn2=="timems") dt2.kind = DtFuncKind::TimeMs;
                            else { dt2.kind = DtFuncKind::Mask; dt2.mask = fn2; }
                            nf.dt = std::move(dt2);
                        } else if (lib::istrcmp(fn2.c_str(), "groupby")) {
                            nf.groupBy = true;
                        } else {
                            THROW("Unknown function for nested field: %s Function:%s ", nf.name, fn2);
                        }
                    } else {
                        throw std::runtime_error("Invalid token after '.' in nested field");
                    }
                }

                f.subselection.push_back(std::move(nf));

                Token sep = next_token(&lx);
                if (sep.type == ttCOMMA) continue;
                if (sep.type == ttR_CURLY) break;
                throw std::runtime_error("Expected ',' or '}' after nested field");
            }

            // after finishing nested, read the outer delimiter
            t = next_token(&lx);
        } else {
            // No nested block; treat lookahead as the delimiter
            t = look;
        }


        // Decide based on delimiter token
        if (t.type == ttCOMMA) {
            qd.selectionSet.push_back(std::move(f));
            continue;
        }
        if (t.type == ttR_CURLY) {
            qd.selectionSet.push_back(std::move(f));
            break;
        }
        THROW("Expected ',' or '}' after field: %s", f.name);

    }

    // 4) Final '}' (closing the doc)
    t = next_token(&lx);
    if (t.type != ttR_CURLY) throw std::runtime_error("Expected '}' to close query document");

    return qd;
}
