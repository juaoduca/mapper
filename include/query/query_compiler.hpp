#pragma once
#include <stdexcept>
#include "query_ast.hpp"
#include "lib.hpp"

namespace ql {

    struct Lx{
        Lexer lx;
        explicit Lx(char *doc) {lx = lexer_init(doc); }
        Token next () { lexer_next(&lx); return lx.current; }
        Token peek () { return lexer_peek(&lx); }
        Token curr () { return lx.current; }
        bool accept(TokenType expected) { return lexer_accept(&lx, expected); }
        void expect(TokenType expected) { lexer_expect(&lx, expected); }
        bool is    (TokenType expected) { return peek().type == expected; }
        void error (TokenType expected, Token *t) { lexer_error(expected, t); }
        bool accepts(std::initializer_list<TokenType> expected_tokens) {
            if (!lib::isin<TokenType>(peek().type, expected_tokens)) {
                return false;
            }
            next();
            return true;
        }
        void expects(std::initializer_list<TokenType> expected_tokens) {
            Token t = peek();
            if (!lib::isin<TokenType>(t.type, expected_tokens)) {
                int i=0;
                std::ostringstream ss;
                ss << "Expected one of: [";
                for (TokenType tp: expected_tokens) {
                    std::string tks = getTokenStr(tp);
                    if (i == 0) {
                        ss << tks; i++;
                    } else {
                        ss << tks << ", ";
                    }
                }
                ss << "] but got: \"" << getTokenStr(t.type) << "\"";
                ss << " value: " << t.value;
                throw std::runtime_error(ss.str());
            }
        }
    };

    using GetSchemaFn = std::function<std::shared_ptr<OrmSchema> (const std::string &schema_name)>;

    class QueryParser {
        public:
            QueryParser(GetSchemaFn fn, char* doc): getSchema(fn), _doc(doc) {
                Lx lx(_doc);
            }

            Doc parseDoc(Lx &lx);

        private:
            GetSchemaFn getSchema;
            char *_doc;
    };

};