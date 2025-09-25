#include <iostream>
#include <vector>
#include <string>
#include <stdexcept>
#include <memory>
// #include "parser.hpp"
// #include "../../include/storage.hpp"

// std::unique_ptr<AST> Parser::parse() {
//     Doc doc;
//     Op *op;
//     while (getOperation(op)) {
//         doc.ops.push_back(*op);
//     }
//     return std::make_unique<Doc>(doc);
// }

// bool Parser::getOperation(Op *op) {
//     if (curr.type != ttL_CURLY) { THROW("Doc must start with a open curly '{' |"); }
//     next();
//     if (curr.type != ttIDENTF) { THROW("Expected an identifier 'Query' or 'SchemaName'!"); }
//     if (curr.value == "query") { next(); }
//     if (curr.type != ttIDENTF) { THROW("Expected a 'SchemaName' !"); }
//     //retrive the schema
//     OrmSchema sch;
//     std::string name = curr.value;
//     if ( !storage.getSchema(name, sch) ) { THROW("No Schema found for: %s", name); }
//     Query qr;
//     qr.name = name;
//     qr.type = "QUERY";
//     std::shared_ptr<OrmSchema> prt_sch = sch.parent.lock();
//     while ( !(prt_sch == nullptr) ) {
//         QuerySchema qs;
//         qs.schema = &*prt_sch;
//         qr.schemas.insert(qr.schemas.begin(), qs); // the root parent will be the first
//         prt_sch = prt_sch->parent.lock();//get the next parent if exists
//     }
//     // getSchemaFunctions
//     // getSelectionList

//     return true;
// }