#ifndef C_TEST_LEXER_H
#define C_TEST_LEXER_H

    #ifdef __cplusplus
    extern "C" {
    #endif

    // #include <cerrno>
    #include <stdint.h>
    #include <string.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <ctype.h>
    #include "lexer.h"


// This struct defines a single test case for the lexer.
    typedef struct {
        const char* input_string;
        const char* expected_token_str;
        TokenType expected_type;
    } TestCase;

    // Forward declaration for a corrected next_token function
    // This test suite is designed to verify the behavior of a corrected implementation.
    Token corrected_next_token(Lexer* lexer);

    // Main test function to run a series of checks on the lexer.
    void test_lexer(void) {
        printf("Running lexer tests...\n\n");

        // Define a list of test cases
        TestCase tests[] = {
            {".abc", ".", ttDOT}, // dot folowed by alphas
            {"+abc", "+", ttPLUS}, // plus folowed by alphas - concat
            {"abc-def", "abc-def", ttIDENTF}, // plus folowed by alphas - concat
            {"&&", "&&", ttLOG_AND},
            {"||", "||", ttLOG_OR},
            {"<>", "<>", ttNOT_EQUALS},
            // Test cases for Numbers
            {"-.789", "-.789", ttNEG_FLOAT},
            {"+.789", "+.789", ttFLOAT},
            {"123", "123", ttINT},
            {"-456", "-456", ttNEG_INT},
            {"0.789", "0.789", ttFLOAT},
            {".789", ".789", ttFLOAT},
            {".123", ".123", ttFLOAT},
            {"-9.87", "-9.87", ttNEG_FLOAT},
            {"+5.5", "+5.5", ttFLOAT}, // Note: your code needs to handle this.
            {"1.2.3", "1.2.3", ttERROR}, // Invalid float with multiple dots
            {".abc", ".", ttDOT}, // dot folowed by alphas
            {"+abc", "+", ttPLUS}, // plus folowed by alphas - concat
            {"abc-def", "abc-def", ttIDENTF}, // plus folowed by alphas - concat

            // Test cases for Identifiers
            {"   hello   ", "hello", ttIDENTF},
            {"   _variable   ", "_variable", ttIDENTF},
            {"_Var123", "_Var123", ttIDENTF},
            {"    $select", "$select", ttSELECT}, // Your token map test
            {"not", "not", ttNOT},
            {"    NOT", "NOT", ttNOT},
            {"OR", "OR",   ttOR},
            {"or", "or",   ttOR},
            {"Or", "Or",   ttOR},
            {"oR", "oR",   ttOR},
            {"contains", "contains", ttCONTAINS},

            // Test cases for single and double-character operators
            {"==", "==", ttEQUALS},
            {"!=", "!=", ttNOT_EQUALS},
            {"<=", "<=", ttLESS_EQUALS},
            {">=", ">=", ttGREATER_EQUALS},
            {"::", "::", ttCAST},
            {"...", "...", ttSPREAD},


            // Test cases for single characters
            {"  (  ", "(", ttL_PAREN},
            {") ", ")", ttR_PAREN},
            {" {", "{", ttL_CURLY},
            {" }", "}", ttR_CURLY},
            {"   ,", ",", ttCOMMA  },
            {"    +", "+", ttPLUS   },
            {"-", "-", ttMINUS  },
            {"*", "*", ttMULT   },
            {"/", "/", ttSLASH  },
            {".", ".", ttDOT    },
            {"<", "<", ttLESS},
            {">", ">", ttGREATER},
            {"`", "`", ttACCENT},

            // Test cases for a full string with multiple tokens_map
            {"  SELECT   a  ,  b", "SELECT", ttIDENTF},
            {"10.5 + 20", "10.5", ttFLOAT},
            {"variable_name == 10", "variable_name", ttIDENTF},
            {"$filter(id=5)", "$filter", ttFILTER},
            {"2.5 - 1.5", "2.5", ttFLOAT},
            {"## a comment ##", "##", ttMLC},
            {"\"String_double_quote\"", "String_double_quote", ttSTRING},
            {"\"String  double  quote", "String  double  quote", ttERROR},
            {"'String_single_quote'", "String_single_quote", ttSTRING},
            {"'String single quote", "String single quote", ttERROR},

        };

        int num_tests = sizeof(tests) / sizeof(TestCase);
        int passed_count = 0;

        for (int i = 0; i < num_tests; ++i) {
            Lexer lexer;
            char* test_input = strdup(tests[i].input_string);
            lexer = init_lexer(test_input);

            Token received_token = next_token(&lexer);

            if (received_token.type == tests[i].expected_type && strcmp(received_token.value, tests[i].expected_token_str) == 0) {
                // printf("  ✅ PASSED\n\n");
                passed_count++;
            } else {
                printf("Test Case #%d: Input: '%s'\n", i + 1, tests[i].input_string);
                printf("  Expected Token: '%s', Type: %d\n", tests[i].expected_token_str, tests[i].expected_type);
                printf("  Received Token: '%s', Type: %d\n", received_token.value, received_token.type);
                printf("  ❌ FAILED\n\n");         }

            free(test_input);
        }

        printf("--- Test Summary ---\n");
        printf("Total Tests: %d\n", num_tests);
        printf("Passed: %d\n", passed_count);
        printf("Failed: %d\n", num_tests - passed_count);
    }

    // void test_lexer2() {
    //     const char *text = "{ query(abc+def) { fld1 fld2 }}";
    //     Lexer lx = init_lexer((char*)text);
    //     Token tokens[13];
    //     Token tk;
    //     int i = 0;
    //     while (true) {
    //         tk = next_token(&lx);
    //         tokens[i] = tk;
    //         if (tk.type == ttEOF) {break; }
    //         i++;
    //     }

    //     REQUIRE(tokens[0].value[0] == '{');
    //     REQUIRE(tokens[0].type == ttL_CURLY);

    //     REQUIRE(tokens[1].value == "query");
    //     REQUIRE(tokens[1].type == ttIDENTF);

    //     REQUIRE(tokens[2].value[0] == '(');
    //     REQUIRE(tokens[2].type == ttL_PAREN);

    //     REQUIRE(tokens[3].value == "abc");
    //     REQUIRE(tokens[3].type == ttIDENTF);

    //     REQUIRE(tokens[4].value[0] == '+');
    //     REQUIRE(tokens[4].type == ttPLUS);

    //     REQUIRE(tokens[5].value == "def");
    //     REQUIRE(tokens[5].type == ttIDENTF);

    //     // char *text = "{ query(abc+def) { fld1 fld2 }}";

    //     REQUIRE(tokens[6].value[0] == ')');
    //     REQUIRE(tokens[6].type == ttR_PAREN);

    //     REQUIRE(tokens[7].value[0] == '{');
    //     REQUIRE(tokens[7].type == ttL_CURLY);

    //     REQUIRE(tokens[8].value == "fld1");
    //     REQUIRE(tokens[8].type == ttIDENTF);

    //     REQUIRE(tokens[9].value == "fld2");
    //     REQUIRE(tokens[9].type == ttIDENTF);

    //     REQUIRE(tokens[10].value[0] == '}');
    //     REQUIRE(tokens[10].type == ttR_CURLY);

    //     REQUIRE(tokens[11].value[0] == '}');
    //     REQUIRE(tokens[11].type == ttR_CURLY);

    //     REQUIRE(tokens[12].type == ttEOF);

    // }


    #ifdef __cplusplus
    }
    #endif

#endif
