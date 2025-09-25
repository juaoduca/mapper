/**
 * @file parser.cpp
 * @brief An arithmetic expression parser that builds an Abstract Syntax Tree (AST).
 *
 * This file provides a complete, self-contained example of a recursive descent
 * parser for basic arithmetic operations (+, -, *, /, and parentheses).
 * It demonstrates how to consume tokens from a lexer and construct an
 * in-memory AST to represent the expression.
 */

#include <iostream>
#include <vector>
#include <string>
#include <stdexcept>
#include <memory>
// #include "lexer.h"
#include "../orm.hpp"
#include "../storage.hpp"

// // --- AST Node Classes ---

// /* BASE CLASS */
// class AST {
// public:
//     virtual ~AST() = default;
// };

// /* Number */
// class Number : public AST {
// public:
//     double value;
//     Number(double val) : value(val) {}
// };

// /* * Binary Operation Node  */
// class BinOp : public AST {
// public:
//     TokenType op_type;
//     std::unique_ptr<AST> left;
//     std::unique_ptr<AST> right;

//     BinOp(TokenType op, std::unique_ptr<AST> l, std::unique_ptr<AST> r)
//         : op_type(op), left(std::move(l)), right(std::move(r)) {}
// };

// class QuerySchema;

// class QueryItem: public AST {
//     OrmProp     *propSchema;
//     QuerySchema *owner;
//     std::string alias;
//     std::string fullname;
//     PropType    type;
//     PropType    castTo;
//     std::string order = "NONE"; // NONE ASC DESC
// };

// /* A Schema defines a Resource(Table) - Resc Query querys a Table
// *  For each Schema, create an RescQuery, and add to Query resource list
// */
// class QuerySchema: public AST {
//     public:
//         OrmSchema     *schema; // will iterate over Parent to find super-Schemas
//         QuerySchema   *parent; // holds a QuerySchema reference to its parent
//         QueryItem     *parent_ref; // the referenced Field prop
//         std::vector<QueryItem> items; //

// };

// /* An Operation can be a Query | Meth-Call | Subscription | Mutation */
// class Op: public AST {
//     public:
//         std::string name;
//         std::string type; // QUERY SUBS MUTATION
// };

// /*  A Query is an Operation that builds a SQL */
// class Query: public Op {
//     public:
//         /* A QuerySchema finds a Schema, Schema have a parent prop that builds that list
//         *  this define the from section in select:
//         *  SELECT ...
//         *  FROM x JOIN y ON (x.any_field = y.Any_field)
//         *  JOIN z ON (y.any_field = z.any_field) ...
//         */
//         std::vector<QuerySchema> schemas;
//         int limit = 100; // a default LIMIT clause to avoid lock the db
// };

// /* A Query document can have many operations */
// class Doc : public AST {
//     public:
//         std::vector<Op> ops; // a doc can have one or more operations
// };



// /* *** Parser Class for a simple math expression ****/

// class Parser {
// private:
//     Lexer& lexer;
//     Storage& storage;
//     Token curr;

//     // Helper function to consume the current token and advance to the next.
//     void next() { curr = next_token(&lexer); }

//     // Parses a factor: a number or a parenthesized expression.
//     std::unique_ptr<AST> factor() {
//         if (curr.type == ttINT || curr.type == ttFLOAT || curr.type == ttNEG_INT || curr.type == ttNEG_FLOAT) {
//             double value = std::stod(curr.value);
//             next();
//             return std::make_unique<Number>(value);
//         } else if (curr.type == ttL_PAREN) {
//             next(); // Consume '('
//             auto expr_node = expr();
//             if (curr.type != ttR_PAREN) {
//                 throw std::runtime_error("Expected ')' after expression");
//             }
//             next(); // Consume ')'
//             return expr_node;
//         } else {
//             throw std::runtime_error("Unexpected token in factor");
//         }
//     }

//     // Parses a term: factors joined by '*' or '/'.
//     std::unique_ptr<AST> term() {
//         auto left_node = factor();
//         while (curr.type == ttMULT || curr.type == ttSLASH) {
//             TokenType op = curr.type;
//             next();
//             auto right_node = factor();
//             left_node = std::make_unique<BinOp>(op, std::move(left_node), std::move(right_node));
//         }
//         return left_node;
//     }

//     // Parses an expression: terms joined by '+' or '-'.
//     std::unique_ptr<AST> expr() {
//         auto left_node = term();
//         while (curr.type == ttPLUS || curr.type == ttMINUS) {
//             TokenType op = curr.type;
//             next();
//             auto right_node = term();
//             left_node = std::make_unique<BinOp>(op, std::move(left_node), std::move(right_node));
//         }
//         return left_node;
//     }

// public:
//     Parser(Lexer &lexer, Storage &store) : lexer(lexer), storage(store) { // ctor
//         next(); // Get the first token
//     }

//     virtual std::unique_ptr<AST> parse();
//     // virtual std::unique_ptr<AST> parse() { return expr(); }

//     virtual bool getOperation(Op *op);
// };

// // --- Tree Traversal for Printing ---
// void print_ast(const AST* node, int indent = 0) {
//     if (!node) {
//         return;
//     }

//     // Print indentation
//     for (int i = 0; i < indent; ++i) {
//         std::cout << "  ";
//     }

//     if (auto num_node = dynamic_cast<const Number*>(node)) {
//         std::cout << "Number: " << num_node->value << std::endl;
//     } else if (auto op_node = dynamic_cast<const BinOp*>(node)) {
//         char op_char;
//         switch (op_node->op_type) {
//             case ttPLUS: op_char = '+'; break;
//             case ttMINUS: op_char = '-'; break;
//             case ttMULT: op_char = '*'; break;
//             case ttSLASH: op_char = '/'; break;
//             default: op_char = '?'; break;
//         }
//         std::cout << "BinaryOp: " << op_char << std::endl;
//         print_ast(op_node->left.get(), indent + 1);
//         print_ast(op_node->right.get(), indent + 1);
//     }
// }

// // --- Main function to run the example ---
// // int main() {
// //     try {
// //         // Mock token stream for the expression: (10 + 5) * 2 - 3 / 1
// //         char *expr = "(10 + 5) * 2 - 3 / 1";
// //         Lexer lx = init_lexer(expr);
// //         Storage st("", Dialect::SQLite);
// //         Parser parser(lx, st);

// //         std::cout << "Parsing expression: (10 + 5) * 2 - 3 / 1" << std::endl;
// //         std::unique_ptr<AST> ast = parser.parse();

// //         std::cout << "\nGenerated AST:" << std::endl;
// //         print_ast(ast.get());
// //         std::cout << std::endl;

// //     } catch (const std::exception& e) {
// //         std::cerr << "Error: " << e.what() << std::endl;
// //         return 1;
// //     }

// //     return 0;
// // }
