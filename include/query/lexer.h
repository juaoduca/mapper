#ifndef C_LEXER_HPP
#define C_LEXER_HPP

    #ifdef __cplusplus
    extern "C" {
    #endif

    #include <cerrno>
    #include <stdint.h>
    #include <string.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <ctype.h>

    #define TOKEN_SIZE 64
    #define CR 13
    #define LF 10
    #define BACK_SLASH 92
    #define GRAVE_ACCENT 96
    #define S_QUOTE 39
    #define D_QUOTE 34

    //ERROR CODES
    #define MISSING_CLOSING_QUOTE  1001
    #define FLOAT_FORMAT_ERROR     1002
    #define MANY_DOTS_IN_FLOAT     1002


    enum TokenType {
        ttNUL=0, // #0
        ttSOH     , // #1
        ttSTX     , // #2
        ttETX     , // #3
        ttEOT     , // #4
        ttENQ     , // #5
        ttACK     , // #6
        ttBEL     , // #7
        ttBS      , // #8
        ttHT      , // #9
        ttLF      , // #10
        ttVT      , // #11
        ttFF      , // #12
        ttCR      , // #13
        ttSO      , // #14
        ttSI      , // #15
        ttDLE     , // #16
        ttDC1     , // #17
        ttDC2     , // #18
        ttDC3     , // #19
        ttDC4     , // #20
        ttNAK     , // #21
        ttSYN     , // #22
        ttETB     , // #23
        ttCAN     , // #24
        ttEM      , // #25
        ttSUB     , // #26
        ttESC     , // #27
        ttFS      , // #28
        ttGS      , // #29
        ttRS      , // #30
        ttUS      , // #31
        ttSPACE   , // #32
        ttEXCL    , // !   #33
        ttD_QUOTE , // "   #34  // double quote
        ttHASH    , // #   #35
        ttMONEY   , // $   #36
        ttPERCENT , // %   #37
        ttAMP     , // &   #38
        ttS_QUOTE , // '   #39  //single quote
        ttL_PAREN , // (   #40
        ttR_PAREN , // )   #41
        ttMULT    , // *   #42
        ttPLUS    , // +   #43
        ttCOMMA   , // ,   #44
        ttMINUS   , // -   #45
        ttDOT     , // .   #46
        ttSLASH   , // /   #47
        ttNUM_0    , // 0
        ttNUM_1    , // 1
        ttNUM_2    , // 2
        ttNUM_3    , // 3
        ttNUM_4    , // 4
        ttNUM_5    , // 5
        ttNUM_6    , // 6
        ttNUM_7    , // 7
        ttNUM_8    , // 8
        ttNUM_9    , // 9
        ttCOLON    , // :
        ttSEMI     , // ;
        ttLESS     , // <
        ttEQUALS   , // =
        ttGREATER  , // >
        ttQUEST    , // ?
        ttAT       , // @
        tt_UPPER_A , // A
        tt_UPPER_B , // B
        tt_UPPER_C , // C
        tt_UPPER_D , // D
        tt_UPPER_E , // E
        tt_UPPER_F , // F
        tt_UPPER_G , // G
        tt_UPPER_H , // H
        tt_UPPER_I , // I
        tt_UPPER_J , // J
        tt_UPPER_K , // K
        tt_UPPER_L , // L
        tt_UPPER_M , // M
        tt_UPPER_N , // N
        tt_UPPER_O , // O
        tt_UPPER_P , // P
        tt_UPPER_Q , // Q
        tt_UPPER_R , // R
        tt_UPPER_S , // S
        tt_UPPER_T , // T
        tt_UPPER_U , // U
        tt_UPPER_V , // V
        tt_UPPER_W , // W
        tt_UPPER_X , // X
        tt_UPPER_Y , // Y
        tt_UPPER_Z , // Z
        ttL_SQUARE , // [
        ttB_SLASH  , // back_slash
        ttR_SQUARE , // ]
        ttCARET      , // ^  caret
        ttU_SCORE  , // _  underscore
        ttACCENT   , // ´  accent
        tt_LOWER_a , // a
        tt_LOWER_b , // b
        tt_LOWER_c , // c
        tt_LOWER_d , // d
        tt_LOWER_e , // e
        tt_LOWER_f , // f
        tt_LOWER_g , // g
        tt_LOWER_h , // h
        tt_LOWER_i , // i
        tt_LOWER_j , // j
        tt_LOWER_k , // k
        tt_LOWER_l , // l
        tt_LOWER_m , // m
        tt_LOWER_n , // n
        tt_LOWER_o , // o
        tt_LOWER_p , // p
        tt_LOWER_q , // q
        tt_LOWER_r , // r
        tt_LOWER_s , // s
        tt_LOWER_t , // t
        tt_LOWER_u , // u
        tt_LOWER_v , // v
        tt_LOWER_w , // w
        tt_LOWER_x , // x
        tt_LOWER_y , // y
        tt_LOWER_z , // z
        ttL_CURLY  , // }
        ttBAR      , // |
        ttR_CURLY  , // }
        ttTILDE    , // ~  Last single char index

        ttDEL     , // DEL = ASCII 127
        ttNOT_EQUALS, // != ou <> // ASCII - 128 em diante
        ttLESS_EQUALS, // <=
        ttGREATER_EQUALS, // >=
        ttIS, // is
        ttNULL, // NULL literal
        ttINT, // 1234567890
        ttFLOAT, // 1234567890.1234567890
        ttINT_DIV, // Integer Division DIV
        ttIDENTF, // Alpha numeric
        ttSTRING, // ttIdentf read from quoted string
        ttDATETIME, // Date and Time dd/mm/aa hh:nn:ss:zzz
        ttDATE, // Only date dd/mm/aa
        ttTIME, // Only Time hh:nn:ss:zzz
        ttALL, // Build a Search within all fields
        //
        ttLOG_AND, // Logical Op AND
        ttLOG_OR, // Logical Op OR
        ttLOG_NOT, // Logical Op NOT
        ttLOG_XOR, // Logical Op XOR
        //
        ttBETWEEN, //
        ttCONTAINS,
        ttSTARTS,
        ttENDS,
        ttIN,
        ttOR,
        ttNOT,
        ttADD,
        ttMUL,
        ttDIV,
        ttMOD,
        ttASC,
        ttDESC,
        // diretiva de SQL
        ttSELECT,
        ttFILTER,
        ttORDER_BY,
        ttDISTINCT,
        ttGROUP,
        ttHAVING,
        ttLIMIT,
        ttNOLIMT,
        ttLEVEL,
        ttINNER,
        ttLEFT,
        ttRIGHT,
        ttSET,
        ttCOUNT,
        ttSUM,
        ttAVG,
        ttTRUNC,
        ttROUND,
        ttCURR,
        ttYEAR,
        ttMONTH,
        ttDAY,
        ttHOUR,
        ttMINUTE,
        ttMAX,
        //
        ttSEC, // ttSEC
        ttEOL, // end of Line
        ttEOF, // end of File
        ttMLC, // multi line comment, ends with another ttMLC '##'
        ttSPREAD, // javascript spread_operator ...
        ttLINE_END,
        ttCAST, // Postgres cast operator "::"
        ttON, // graphQL FragmentSpread on InterfaceType "...frag on Funcio"
        ttNEG_INT,
        ttNEG_FLOAT,
        ttERROR
    };

    struct Token { // ATENCAO - NAO MUDAR A ORDEM - senão terá que alterar as ARRAYS assciTokens e tokens_map  acima
        char value[TOKEN_SIZE]; //fixed char buffer
        TokenType type;
    };

    struct Lexer {
        char* text; // null term sring -
        size_t len; // text len \0 included
        char* pos; // inc pointer
        char lower[TOKEN_SIZE]; // to hold the lower conversion and avoid maloc/freemem in getTokenType()
    };

    Token asciiTokens[] {
        { "0"  , ttNUL      },
        { "1"  , ttSOH      },
        { "2"  , ttSTX      },
        { "3"  , ttETX      },
        { "4"  , ttEOT      },
        { "5"  , ttENQ      },
        { "6"  , ttACK      },
        { "7"  , ttBEL      },
        { "8"  , ttBS       },
        { "9"  , ttHT       }, // 10
        { "0"  , ttLF       },
        { "1"  , ttVT       },
        { "2"  , ttFF       },
        { "3"  , ttCR       },
        { "4"  , ttSO       },
        { "5"  , ttSI       },
        { "6"  , ttDLE      },
        { "7"  , ttDC1      },
        { "8"  , ttDC2      },
        { "9"  , ttDC3      }, // 20
        { "0"  , ttDC4      },
        { "1"  , ttNAK      },
        { "2"  , ttSYN      },
        { "3"  , ttETB      },
        { "4"  , ttCAN      },
        { "5"  , ttEM       },
        { "6"  , ttSUB      },
        { "7"  , ttESC      },
        { "8"  , ttFS       },
        { "9"  , ttGS       }, // 30
        { "30" , ttRS       },
        { "31" , ttUS       },
        { " "  , ttSPACE    },
        { "!"  , ttEXCL     },
        { "34" , ttD_QUOTE  }, // double quote "string"
        { "#"  , ttHASH     },
        { "$"  , ttMONEY    },
        { "%"  , ttPERCENT  },
        { "&"  , ttAMP      },
        { "39" , ttS_QUOTE  }, // single quote 'string'
        { "("  , ttL_PAREN  }, // 40
        { ")"  , ttR_PAREN  },
        { "*"  , ttMULT     },
        { "+"  , ttPLUS     },
        { ","  , ttCOMMA    },
        { "-"  , ttMINUS    },
        { "."  , ttDOT      },
        { "/"  , ttSLASH    },
        { "0"  , ttNUM_0    },
        { "1"  , ttNUM_1    },
        { "2"  , ttNUM_2    },
        { "3"  , ttNUM_3    },
        { "4"  , ttNUM_4    },
        { "5"  , ttNUM_5    },
        { "6"  , ttNUM_6    },
        { "7"  , ttNUM_7    },
        { "8"  , ttNUM_8    },
        { "9"  , ttNUM_9    },
        { ":"  , ttCOLON    },
        { ";"  , ttSEMI     },
        { "<"  , ttLESS     },
        { "="  , ttEQUALS   },
        { ">"  , ttGREATER  },
        { "?"  , ttQUEST    },
        { "@"  , ttAT       },
        { "A"  , tt_UPPER_A },
        { "B"  , tt_UPPER_B },
        { "C"  , tt_UPPER_C },
        { "D"  , tt_UPPER_D },
        { "E"  , tt_UPPER_E },
        { "F"  , tt_UPPER_F },
        { "G"  , tt_UPPER_G },
        { "H"  , tt_UPPER_H },
        { "I"  , tt_UPPER_I },
        { "J"  , tt_UPPER_J },
        { "K"  , tt_UPPER_K },
        { "L"  , tt_UPPER_L },
        { "M"  , tt_UPPER_M },
        { "N"  , tt_UPPER_N },
        { "O"  , tt_UPPER_O },
        { "P"  , tt_UPPER_P },
        { "Q"  , tt_UPPER_Q },
        { "R"  , tt_UPPER_R },
        { "S"  , tt_UPPER_S },
        { "T"  , tt_UPPER_T },
        { "U"  , tt_UPPER_U },
        { "V"  , tt_UPPER_V },
        { "W"  , tt_UPPER_W },
        { "X"  , tt_UPPER_X },
        { "Y"  , tt_UPPER_Y },
        { "Z"  , tt_UPPER_Z },
        { "["  , ttL_SQUARE },
        { "92" , ttB_SLASH  },  //  '\'
        { "]"  , ttR_SQUARE },
        { "^"  , ttCARET    },
        { "_"  , ttU_SCORE  },
        { "`"  , ttACCENT   },
        { "a"  , tt_LOWER_a },
        { "b"  , tt_LOWER_b },
        { "c"  , tt_LOWER_c },
        { "d"  , tt_LOWER_d },
        { "e"  , tt_LOWER_e },
        { "f"  , tt_LOWER_f },
        { "g"  , tt_LOWER_g },
        { "h"  , tt_LOWER_h },
        { "i"  , tt_LOWER_i },
        { "j"  , tt_LOWER_j },
        { "k"  , tt_LOWER_k },
        { "l"  , tt_LOWER_l },
        { "m"  , tt_LOWER_m },
        { "n"  , tt_LOWER_n },
        { "o"  , tt_LOWER_o },
        { "p"  , tt_LOWER_p },
        { "q"  , tt_LOWER_q },
        { "r"  , tt_LOWER_r },
        { "s"  , tt_LOWER_s },
        { "t"  , tt_LOWER_t },
        { "u"  , tt_LOWER_u },
        { "v"  , tt_LOWER_v },
        { "w"  , tt_LOWER_w },
        { "x"  , tt_LOWER_x },
        { "y"  , tt_LOWER_y },
        { "z"  , tt_LOWER_z },
        { "{"  , ttL_CURLY  },
        { "|"  , ttBAR      },
        { "}"  , ttR_CURLY  },
        { "~"  , ttTILDE    }, // 126
        // a partir desse ponto não tem mais relação com ASCII table
        // DEL e Comparadores Logicos Composto == != <> >= <= >> <<
    };

    // ordered token map
    Token tokens_map[] = { // will be ordered by init_lexer
        { "##"         , ttMLC            },
        { "..."        , ttSPREAD         },
        { "\n"         , ttEOL            },
        { "\r\n"       , ttEOL            },
        { "!="         , ttNOT_EQUALS     },
        { "$distinct"  , ttDISTINCT       },
        { "$filter"    , ttFILTER         },
        { "$group"     , ttGROUP          },
        { "$having"    , ttHAVING         },
        { "$level"     , ttLEVEL          },
        { "$limit"     , ttLIMIT          },
        { "$nolimit"   , ttNOLIMT         },
        { "$order"     , ttORDER_BY       },
        { "$select"    , ttSELECT         },
        { "$set"       , ttSET            },
        { "&&"         , ttLOG_AND        },
        { "||"         , ttLOG_OR         },
        { "::"         , ttCAST           },
        { "<>"         , ttNOT_EQUALS     },
        { "<="         , ttLESS_EQUALS    },
        { "=="         , ttEQUALS         },
        { ">="         , ttGREATER_EQUALS },
        { "add"        , ttADD            },
        { "all"        , ttALL            },
        { "and"        , ttLOG_AND        },
        { "asc"        , ttASC            },
        { "avg"        , ttAVG            },
        { "between"    , ttBETWEEN        },
        { "bt"         , ttBETWEEN        },
        { "contains"   , ttCONTAINS       },
        { "count"      , ttCOUNT          },
        { "ct"         , ttCONTAINS       },
        { "curr"       , ttCURR           },
        { "day"        , ttDAY            },
        { "del"        , ttDEL            },
        { "desc"       , ttDESC           },
        { "div"        , ttDIV            },
        { "ends_with"  , ttENDS           },
        { "ew"         , ttENDS           },
        { "hour"       , ttHOUR           },
        { "hr"         , ttHOUR           },
        { "in"         , ttIN             },
        { "inner"      , ttINNER          },
        { "is"         , ttIS             },
        { "left"       , ttLEFT           },
        { "max"        , ttMAX            },
        { "min"        , ttMINUTE         },
        { "mod"        , ttMOD            },
        { "month"      , ttMONTH          },
        { "mth"        , ttMONTH          },
        { "mul"        , ttMUL            },
        { "not"        , ttNOT            },
        { "null"       , ttNULL           },
        { "on"         , ttON             },
        { "or"         , ttOR             },
        { "right"      , ttRIGHT          },
        { "round"      , ttROUND          },
        { "sec"        , ttSEC            },
        { "starts_with", ttSTARTS         },
        { "sw"         , ttSTARTS         },
        { "sub"        , ttSUB            },
        { "sum"        , ttSUM            },
        { "trunc"      , ttTRUNC          },
        { "xor"        , ttLOG_XOR        },
        { "year"       , ttYEAR           },
        { "yr"         , ttYEAR           }
    };

    int sortCmpTokens(const void* a, const void* b) {
            const Token* tokenA = (const Token*)a;
            const Token* tokenB = (const Token*)b;
            return strcmp(tokenA->value, tokenB->value);
    };

    size_t map_size = 0;

    Lexer init_lexer(char* text) {
        map_size = ( sizeof(tokens_map) / sizeof(tokens_map[0]) );
        qsort(tokens_map, map_size, sizeof(Token), sortCmpTokens);
        Lexer lx;
        lx.text = text;
        lx.pos = text;
        lx.len = strlen(text);
        for (int i = 0; i < TOKEN_SIZE; i++) { lx.lower[i] = 0xFF;}
        return lx;
    }

    //must pass the key in lowercase as the table is lower
    TokenType getTokenType (const char *key) {

        /********  local bsearch - no func call *********/
        int low = 0;
        int high = map_size - 1;
        Token *found = nullptr;

        while (low <= high) {
            int mid = low + (high - low) / 2; // Prevents potential overflow for large low/high
            // strcmp() returns -1 if key < token, 0 if key == token, 1 if key > token

            /******   in place implementation  of strcmp() **********/
            const char *s1 = key;
            const char *s2 = tokens_map[mid].value;
            while (*s1 && (*s1 == *s2)) {
                s1++; // Move to the next character in s1
                s2++; // Move to the next character in s2
            }
            int cmp_res =  *(const unsigned char*)s1 - *(const unsigned char*)s2;
            /******   in place implementation  of strcmp() **********/

            // if ( key == tokens_map[mid].token) { // Check if key is present at mid
            if ( cmp_res == 0) {
                found = &tokens_map[mid]; // Key found, return its index
                break; // as it was found -> end the search
            }
            // if (key > tokens_map[mid].token) // If key is greater, ignore left half
            if (cmp_res > 0 ) {
                low = mid + 1;
            } else /* if (key < tokens_map[mid].token) */ { // If key is smaller, ignore right half
                high = mid - 1;
            }
        }
        /********  local bsearch - no func call *********/

        if (found == nullptr) {
            return ttIDENTF;
        } else {
            Token* tk = (Token*)found;
            if (tk->type == ttERROR) {
                return ttIDENTF;
            } else {
                return tk->type;
            }
        }
    }

    Token next_token(Lexer* lx) {
        int len = (lx->pos - lx->text);
        if ( (len > lx->len) ) {
            Token tk = {.type = ttEOF};
            return tk;
        }
        char chr = *lx->pos;
        Token token_ = {.type = ttERROR};//to create the buffer
        char *current = token_.value; // copy each char to Token
        char *lower = lx->lower;      // copy each char as lower to find Token Type

        //white space
        while (chr == ' ') {
            // *current = *lx->pos; //donot copy
            lx->pos++; chr = *lx->pos; // advance
        }

        /************* STRING - Double or Single Quoted ****************/
        if (chr == '"' || chr == 39) { // double_quotes = string_identifier = ttString
            // *current = *lx->pos; //do not copy quote
            char q = chr;
            lx->pos++; chr = *lx->pos; // advance
            while (chr != q && chr != '\0') { // while not quote
                *current = *lx->pos; current++; //copy current - wait next
                lx->pos++; chr = *lx->pos; // advance - copy next char
            }
            if (chr != q) {
                errno = MISSING_CLOSING_QUOTE; // must end string with closing quote
                token_.type = ttERROR;
                return token_;
            }
            *current = '\0';// null terminated
            token_.type = ttSTRING;
            return token_;
        }

        // Comparators and LOG Operators  = Single or Double
        if (chr == '='  || chr == '<' || chr == '>' || chr == '!' || chr == ':' || chr == '#' || chr == '&' || chr == '|') {
            token_.type = asciiTokens[chr].type; // if its single, get type as poiter will advance
            *current = *lx->pos; current++; //copy current - wait next
            lx->pos++; chr = *lx->pos; // advance
            if (chr == '=' || chr == ':' || chr == '#' || chr == '&' || chr == '|' || chr == '<' || chr == '>' ) { // == != <= >= comparators | ## coment
                *current = *lx->pos; current++; //copy current - wait next
                lx->pos++; // advance
                *current = '\0'; // null terminating
                token_.type = getTokenType(token_.value);
                return token_;
            } // not double comparator - no need null terminate
            return token_;
        }

        // is identifier - [ ALPHA | UNDERSCORE | MONEY ]
        if ((chr >= 'a' && chr <= 'z') || (chr >= 'A' && chr <= 'Z') || chr == '_' || chr == '$') {
            *current = *lx->pos; current++; //copy current - wait next
            if (chr != '_') { *lower = chr | 32; } else { *lower = chr; }
            lower++;
            lx->pos++; chr = *lx->pos; // advance
            // [ALPHA | DIGIT | UNDERSCORE | MONEY]
            while ((chr >= 'a' && chr <= 'z') || (chr >= 'A' && chr <= 'Z') || (chr >= '0' && chr <= '9') || chr == '_' || chr == '$' || chr == '-' ) {
                *current = *lx->pos; current++; //copy current - wait next
                if (chr != '_') { *lower = chr | 32; } else { *lower = chr; }
                lower++;
                lx->pos++; chr = *lx->pos; // advance
            }
            *current = '\0'; // null terminated string
            *lower = '\0';
            token_.type = getTokenType(lx->lower);
            return token_;
        }

        // '+' SIGNAL or CONCAT
        if ( chr == '+' && (lx->len > 1)) {
            char temp = *(lx->pos+1); // peek
            if ((temp >= 'a' && temp <= 'z') || (temp >= 'A' && temp <= 'Z') || temp == '_' || temp == '$') { // isAlpha
                *current = *lx->pos; //copy
                lx->pos++; chr = *lx->pos; // advance
                token_.type = ttPLUS;
                return token_;
            }
        }

            // ... spread          // consumed
        if(chr == '.' && (lx->len >= 3) && *(lx->pos+1) == '.' && *(lx->pos+2) == '.') {
                *current = chr; current++; //'.'
                lx->pos++;
                *current = *lx->pos; current++; //copy current - wait next = '.'
                lx->pos++;
                *current = *lx->pos; current++; //copy current - wait next = '.'
                *current = '\0';
                token_.type = ttSPREAD;
                return token_;
        }

        // Fractional Number = dot + digits
        if(chr == '.' && (lx->len > 1)  && (*(lx->pos+1) >= '0' && *(lx->pos+1) <= '9')) {
            *current = *lx->pos; current++; //copy current - wait next
            lx->pos++; chr = *lx->pos; // advance
            while (chr >= '0' && chr <= '9') { // must not have other dot
                *current = *lx->pos; current++; //copy current - wait next
                lx->pos++; chr = *lx->pos; // advance
            }
            if (chr == '.') {
                errno = FLOAT_FORMAT_ERROR;
                token_.type = ttERROR;
                return token_;
            }
            *current = '\0'; //
            token_.type = ttFLOAT;
            return token_;

        }

        // Signed Fractional Number = Sign [-|+] + DOT + Digits
        if ((chr == '-' || chr == '+') && (lx->len > 2) && ( *(lx->pos+1) == '.' ) && (*(lx->pos+2) >= '0' && *(lx->pos+2) <= '9')  ) {
            bool nega = chr == '-';
            int dotcount = 0;
            *current = chr; current++; //copy current - wait next
            lx->pos++ ; chr = *lx->pos; // advance
            while (((chr >= '0' && chr <= '9') || chr == '.')) {
                if (chr == '.') { dotcount++; }
                *current = chr; current++; //copy current - wait next
                lx->pos++ ; chr = *lx->pos; // advance
            }
            if (dotcount > 1) {
                errno = FLOAT_FORMAT_ERROR;
                token_.type = ttERROR;
                return token_;
            }
            *current = '\0';
            token_.type = dotcount == 1 ? (nega ? ttNEG_FLOAT : ttFLOAT) : (nega ? ttNEG_INT : ttINT);
            return token_;
        }

        // Signed Number = sign  ( [.] + digits) | ( digits + [.] )
        if ((chr == '-' || chr == '+') && (lx->len > 1) && ( (*(lx->pos+1) >= '0' && *(lx->pos+1) <= '9') ) ) {
            bool nega = chr == '-';
            int dotcount = 0;
            *current = chr; current++; //copy current - wait next
            lx->pos++ ; chr = *lx->pos; // advance
            while (((chr >= '0' && chr <= '9') || chr == '.')) {
                *current = chr; current++; //copy current - wait next
                lx->pos++ ; chr = *lx->pos; // advance
                if (chr == '.') { dotcount++; }
            }
            if (dotcount > 1) {
                errno = FLOAT_FORMAT_ERROR;
                token_.type = ttERROR;
                return token_;
            }
            *current = '\0';
            token_.type = dotcount == 1 ? (nega ? ttNEG_FLOAT : ttFLOAT) : (nega ? ttNEG_INT : ttINT);
            return token_;
        }

        //Number = digits [DOT]
        if ( (chr >= '0' && chr <= '9') ) {
            *current = chr; current++; //copy
            lx->pos++ ; chr = *lx->pos; // advance
            int dotcount = 0;
            while ((chr >= '0' && chr <= '9') || chr == '.') {  //isDigit
                if (chr == '.') { dotcount++; }
                *current = chr; current++; //copy
                lx->pos++ ; chr = *lx->pos; // advance
            }
            if (dotcount > 1) {
                errno = FLOAT_FORMAT_ERROR;
                token_.type = ttERROR;
                return token_;
            }
            *current = '\0';
            token_.type = dotcount == 1 ? ttFLOAT : ttINT;
            return token_;
        } // Number or Float

        // check if is other single char simbol
        // if (chr == '!' || chr == '@' || chr == '#' || chr == '$' || chr == '%' || chr == '&' ||
        //     chr == '*' || chr == '(' || chr == ')' || chr == '-' || chr == '+' || chr == '=' ||
        //     chr == 96  || chr == '`' || chr == '{' || chr == '[' || chr == '^' || chr == '~' ||
        //     chr == '*' || chr == '}' || chr == ']' || chr == '?' || chr == '/' || chr == ':' ||
        //     chr == ';' || chr == '>' || chr == '<' || chr == 92  || chr == '|' || chr == ','  ) {
        if (chr >= 33 && chr <= 126 )  {
            *current = *lx->pos;  //copy current char
            lx->pos++;            //advance no copy - only one char expected
            token_.type = asciiTokens[chr].type;
            return token_;
        }


        token_.value[0] = '\0';
        token_.type = ttEOF;
        // lx->pos++;
        return token_;
    }

    #ifdef __cplusplus
    }
    #endif

#endif
