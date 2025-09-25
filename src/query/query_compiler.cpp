
#include <stdexcept>
#include "query_ast.hpp"
#include "query_compiler.hpp"
#include "lib.hpp"
#include "lexer.h"

namespace ql {

    #define COMPS {ttEQUALS, ttCOLON, ttNOT_EQUALS, ttLESS, ttGREATER, ttLESS_EQUALS, ttGREATER_EQUALS }
    #define RVALUE { ttIDENTF, ttSTRING, ttINT, ttNEG_INT, ttFLOAT, ttNEG_FLOAT} // {func|field}(identf) or Value
    #define SEPARATORS {ttCOMMA, ttSPACE}

    FieldFunc parseFieldFunc(Lx &lx) { // "." function ["(", funcArgs, ")"]
        lx.expect(ttIDENTF);
        Token n = lx.curr(); // retreive the ident
        FieldFunc fc;
        fc.name = n.value;
        if(lx.accept(ttL_PAREN)) { // function have args ?
            do{
                lx.expects(RVALUE);
                Token a = lx.curr();
                fc.args.push_back(a);
            } while (lx.accepts(SEPARATORS));
        }
        return fc;
    }

    FieldArg parseFieldArg(Lx &lx) { // field_arg = [ comparator ], rvalue;
        FieldArg fa;
        lx.expects(COMPS);   // expects a comparator
        fa.comparator = lx.curr(); // retrive the comparator
        lx.expects(RVALUE);  // expects an Rvalue
        Token r = lx.curr(); // retrive the RValue

        if(lx.is(ttL_PAREN)) { // check if RValue is a funciton
            FuncCall fc; fc.name = r.value; fc.type = r.type;
            do { // get the funcion args
                Token farg = lx.next();
                if (!lib::isin(farg.type, RVALUE)) {THROW("Expected a rvalue: FieldName, String or Number");}
                fc.args.push_back(farg);
            } while (lx.accepts(SEPARATORS));
            lx.expect(ttR_PAREN);
            fa.rvalue = fc;
        } else {
            Value v; v.name = r.value; v.type = r.type;
            fa.rvalue = v;
        }
        return fa;
    }

    Field parseField(Lx &lx) {// { alias:name["(" field_args ")"]["."func_chain]["{"nested_fields"}"]
        lx.expect(ttIDENTF);
        Field fd;
        Token name_or_alias = lx.curr();
        if (lx.accept(ttCOLON)) { //alias ":" name // advance to name
            fd.alias = name_or_alias.value;
            fd.name = lx.curr().value; // curr is the name
        } else {
            fd.name = name_or_alias.value;
        }

        if(lx.accept(ttL_PAREN)) {// field_args
            do {
                FieldArg farg = parseFieldArg(lx);
                farg.field = fd;
                fd.args.push_back(farg);
            } while( lx.accepts({ttCOMMA, ttSPACE}) );
        }

        if(lx.accept(ttDOT)) { //function_chain
            do {
                FieldFunc fc = parseFieldFunc(lx);
                fd.funcs.push_back(fc);
            } while ( lx.accept(ttDOT) );
        }

        if(lx.accept(ttL_CURLY)) {// nested_fields
            do {
                Field fd = parseField(lx);
                fd.fields.push_back(fd);
            } while ( lx.accepts({ttCOMMA, ttSPACE}) );
        }

    }

    bool parseFuncCall(Lx &lx, QueryArg &queryArg, Token &lvalue) {
        if(lx.accept(ttL_PAREN)) { // functionCall()
            FuncCall fc;
            fc.name = lvalue.value;
            do {
                Token farg = lx.next();
                if (!lib::isin(farg.type, RVALUE)) {THROW("Expected a rvalue: FieldName, String or Number");}
                fc.args.push_back(farg);
            } while (lx.accept(ttCOMMA));
            lx.expect(ttR_PAREN);
            queryArg.lvalue = fc;
            return true;
        }
        return false;
    }

    QueryArg parseQueryArg(Lx &lx) {
        QueryArg qarg;
        if (lx.is(ttIDENTF)) { // field | func | funcCall
            //check if is FunctionCall with arg
            Token lt = lx.next(); // retrive the identif
            if (!parseFuncCall(lx, qarg, lt)) { // not a funcCall
                Value lv; lv.name = lt.value; lv.type = lt.type;
                qarg.lvalue = lv;
            }

            Token comp;
            if(lx.accepts(COMPS)) {
                 comp = lx.curr(); // retrieve the comparator
            } else {
                lx.error(ttCOMPARATOR, &comp);
            }

            //check if is FunctionCall with arg
            Token rt = lx.next();
            if (!lib::isin<TokenType>(rt.type, RVALUE)) {lx.error(ttRVALUE, &rt ); }
            if (!parseFuncCall(lx, qarg, rt)) { // not a funcCall
                Value rv; rv.name = rt.value; rv.type = rt.type;
                qarg.rvalue = rv;
            }
        } else {
            lx.error(ttIDENTF, &lx.lx.current);
        }
        return qarg;
    }

    Query parseQuery(Lx &lx) {
        if (lx.is(ttIDENTF)) {
            Token t = lx.next(); // get the identifier
            Query qr;
            qr.name = t.value;
            // if have query_arguments (arg, {",", arg})
            if (lx.accept(ttL_PAREN)) {
                do { // queryArgs
                    QueryArg arg = parseQueryArg(lx);
                    qr.args.push_back(arg);
                } while (lx.accept(ttCOMMA));
                lx.expect(ttR_PAREN);
            }

            lx.expect(ttL_CURLY); // "{" fieldlist "}" mandatory
            do {
                Field fd = parseField(lx);
                qr.fields.push_back(fd);
            } while ( lx.accept(ttCOMMA) || lx.accept(ttSPACE) );
    }
}

Doc QueryParser::parseDoc(Lx &lx) {
    Doc doc;
    lx.expect(ttL_CURLY);
    do {
        Query qr = parseQuery(lx);
        doc.queries.push_back(qr);
    } while (lx.accept(ttR_CURLY));
    return doc;
}