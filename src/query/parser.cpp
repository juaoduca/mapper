#include <iostream>
#include <vector>
#include <string>
#include <stdexcept>
#include <memory>
#include "parser.hpp"
#include "../../include/query/lexer.h"
#include "../../include/storage.hpp"

std::unique_ptr<AST> Parser::parse() {
    Doc doc;
    Op *op;
    while (getOperation(op)) {
        doc.ops.push_back(*op);
    }
    return std::make_unique<Doc>(doc);
}

std::unique_ptr<Op> Parser::getOperation(Op *op) {
    if (curr.type == ttL_CURLY) {
        next();
        if (curr.type == ttIDENTF) { //  { query funcio{ fisica {name age} admited matricula }}
            if (curr.value == "query") { next(); }
            if (curr.type == ttIDENTF) {
                //retrive the schema
                OrmSchema sch;
                std::string name = curr.value;
                if ( storage.getSchema(name, sch) ) {
                    Query qr;
                    qr.name = name;
                    std::shared_ptr<const OrmSchema> prt_sch = sch.parent.lock();
                    while ( !(prt_sch = nullptr) ) {
                        qr.schemas.emplace(qr.schemas.begin(), prt_sch); // the root parent will be the first
                        prt_sch = prt_sch->parent.lock();//get the next parent if exists
                    }

                }
            }
        }
    }
}