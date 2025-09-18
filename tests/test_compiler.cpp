#include "catch.hpp"
#include "./query/test_lexer.h"

TEST_CASE("Test LEXER ", "Only one returned Token") {

    test_lexer();
    REQUIRE(true);
}

TEST_CASE("Test LEXER 2", "varios returned tokens") {

        const char *text = "{ query(abc+def) { fld1 fld2 }}";
        Lexer lx = init_lexer((char*)text);
        Token tokens[14];
        Token tk;
        int i = 0;
        while (true) {
            tk = next_token(&lx);
            tokens[i] = tk;
            if (tk.type == ttEOF) {break; }
            i++;
        }

        REQUIRE(tokens[0].value[0] == '{');
        REQUIRE(tokens[0].type == ttL_CURLY);

        REQUIRE(strcmp(tokens[1].value , "query") == 0);
        REQUIRE(tokens[1].type == ttIDENTF);

        REQUIRE(tokens[2].value[0] == '(');
        REQUIRE(tokens[2].type == ttL_PAREN);

        REQUIRE(strcmp(tokens[3].value , "abc") == 0);
        REQUIRE(tokens[3].type == ttIDENTF);

        REQUIRE(tokens[4].value[0] == '+');
        REQUIRE(tokens[4].type == ttPLUS);

        REQUIRE(strcmp(tokens[5].value , "def") == 0);
        REQUIRE(tokens[5].type == ttIDENTF);

        // char *text = "{ query(abc+def) { fld1 fld2 }}";

        REQUIRE(tokens[6].value[0] == ')');
        REQUIRE(tokens[6].type == ttR_PAREN);

        REQUIRE(tokens[7].value[0] == '{');
        REQUIRE(tokens[7].type == ttL_CURLY);

        REQUIRE(strcmp(tokens[8].value , "fld1") == 0);
        REQUIRE(tokens[8].type == ttIDENTF);

        REQUIRE(strcmp(tokens[9].value , "fld2") == 0);
        REQUIRE(tokens[9].type == ttIDENTF);

        REQUIRE(tokens[10].value[0] == '}');
        REQUIRE(tokens[10].type == ttR_CURLY);

        REQUIRE(tokens[11].value[0] == '}');
        REQUIRE(tokens[11].type == ttR_CURLY);

        // REQUIRE(tokens[12].type == ttERROR);

        REQUIRE(tokens[12].type == ttEOF);

}