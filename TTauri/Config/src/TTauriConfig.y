/* Copyright 2019 Pokitec
 * All rights reserved.
 */

%define api.pure full
%define parse.lac full
%define parse.error verbose
%locations
%param { yyscan_t scanner }
%param { TTauri::Config::ParseContext* context }

%code top {
  #include <stdio.h>

  #define YYPRINT(a, b, c)

  #define NEW_NODE(T, location, ...) new T({context->file, location.first_line, location.first_column}, ## __VA_ARGS__)
  #define NEW_UNARY_OPERATOR(op, location, right) NEW_NODE(ASTUnaryOperator, location, op, right)
  #define NEW_BINARY_OPERATOR(op, location, left, right) NEW_NODE(ASTBinaryOperator, location, op, left, right)
} 
%code requires {
  #include <string>
  #include <cstdint>
  #include <memory>
  #include "TTauri/Required/URL.hpp"
  #include "TTauri/Config/AST.hpp"
  #include "TTauri/Config/ParseContext.hpp"
  #include "TTauri/Config/parser.hpp"
  typedef void* yyscan_t;
  int TTauriConfig_yyfind_token(char *text);
}

%code {
  using namespace TTauri::Config;

  int yylex(YYSTYPE* yylvalp, YYLTYPE* yyllocp, yyscan_t scanner, TTauri::Config::ParseContext* context);
  void yyerror(YYLTYPE* yyllocp, yyscan_t scanner, TTauri::Config::ParseContext* context, const char* msg) {
    context->setError({context->file, yyllocp->first_line, yyllocp->first_column}, msg);
  }
}

%union {
    char *string;
    int64_t integer;
    double real;
    TTauri::URL *url;
    TTauri::Config::ASTExpression *expression;
    TTauri::Config::ASTExpressionList *expression_list;
    TTauri::Config::ASTObject *object;
    TTauri::Config::ASTArray *array;
}

%token-table
%token <string> T_IDENTIFIER
%token <integer> T_INTEGER
%token <real> T_FLOAT
%token <string> T_STRING
%token <integer> T_COLOR
%token <url> T_URL

// Same precedence order as C++ operators.
%left ';'
%right '='
%left "or"
%left "xor"
%left "and"
%left '|'
%left '^'
%left '&'
%left "==" "!="
%left '<' '>' "<=" ">="
%left "<<" ">>"
%left '+' '-'
%left '*' '/' '%'
%right '~' "not" UMINUS
%left '(' '['
%left '.'

%precedence SELECTOR

%type <object> body
%type <object> root
%type <expression> expression
%type <object> object
%type <array> array
%type <expression_list> expressions
%start root
%%

array:
      '[' ']'                                                       { $$ = NEW_NODE(ASTArray, @1); }
    | '[' expressions ']'                                           { $$ = NEW_NODE(ASTArray, @1, $2); }
    | '[' expressions ';' ']'                                       { $$ = NEW_NODE(ASTArray, @1, $2); }
    ;

object:
      '{' '}'                                                       { $$ = NEW_NODE(ASTObject, @1); }
    | '{' expressions '}'                                           { $$ = NEW_NODE(ASTObject, @1, $2); }
    | '{' expressions ';' '}'                                       { $$ = NEW_NODE(ASTObject, @1, $2); }
    ;

expression:
      '(' expression ')'                                            { $$ = $2; }
    | object                                                        { $$ = $1; }
    | array                                                         { $$ = $1; }
    | T_INTEGER                                                     { $$ = NEW_NODE(ASTInteger, @1, $1); }
    | T_FLOAT                                                       { $$ = NEW_NODE(ASTFloat, @1, $1); }
    | T_COLOR                                                       { $$ = NEW_NODE(ASTColor, @1, static_cast<uint32_t>($1)); }
    | T_URL                                                         { $$ = NEW_NODE(ASTURL, @1, $1); }
    | "true"                                                        { $$ = NEW_NODE(ASTBoolean, @1, true); }
    | "false"                                                       { $$ = NEW_NODE(ASTBoolean, @1, false); }
    | "null"                                                        { $$ = NEW_NODE(ASTNull, @1); }
    | T_STRING                                                      { $$ = NEW_NODE(ASTString, @1, $1); }
    | T_IDENTIFIER                                                  { $$ = NEW_NODE(ASTName, @1, $1); }
    | '~' expression                                                { $$ = NEW_UNARY_OPERATOR(ASTUnaryOperator::Operator::NOT, @1, $2); }
    | '-' expression %prec UMINUS                                   { $$ = NEW_UNARY_OPERATOR(ASTUnaryOperator::Operator::NEG, @1, $2); }
    | '.' T_IDENTIFIER                                              { $$ = NEW_NODE(ASTMember, @1, new TTauri::Config::ASTRootObject({context->file, @1.first_line, @1.first_column}), $2); }
    | '$' T_IDENTIFIER                                              { $$ = NEW_NODE(ASTMember, @1, new TTauri::Config::ASTVariableObject({context->file, @1.first_line, @1.first_column}), $2); }
    | "not" expression                                              { $$ = NEW_UNARY_OPERATOR(ASTUnaryOperator::Operator::LOGICAL_NOT, @1, $2); }
    | expression '=' expression                                     { $$ = NEW_NODE(ASTAssignment, @2, $1, $3); }
    | expression '.' T_IDENTIFIER                                   { $$ = NEW_NODE(ASTMember, @2, $1, $3); }
    | expression '*' expression                                     { $$ = NEW_BINARY_OPERATOR(ASTBinaryOperator::Operator::MUL, @2, $1, $3 ); }
    | expression '/' expression                                     { $$ = NEW_BINARY_OPERATOR(ASTBinaryOperator::Operator::DIV, @2, $1, $3 ); }
    | expression '%' expression                                     { $$ = NEW_BINARY_OPERATOR(ASTBinaryOperator::Operator::MOD, @2, $1, $3 ); }
    | expression '+' expression                                     { $$ = NEW_BINARY_OPERATOR(ASTBinaryOperator::Operator::ADD, @2, $1, $3 ); }
    | expression '-' expression                                     { $$ = NEW_BINARY_OPERATOR(ASTBinaryOperator::Operator::SUB, @2, $1, $3 ); }
    | expression "<<" expression                                    { $$ = NEW_BINARY_OPERATOR(ASTBinaryOperator::Operator::SHL, @2, $1, $3 ); }
    | expression ">>" expression                                    { $$ = NEW_BINARY_OPERATOR(ASTBinaryOperator::Operator::SHR, @2, $1, $3 ); }
    | expression '<' expression                                     { $$ = NEW_BINARY_OPERATOR(ASTBinaryOperator::Operator::LT, @2, $1, $3 ); }
    | expression '>' expression                                     { $$ = NEW_BINARY_OPERATOR(ASTBinaryOperator::Operator::GT, @2, $1, $3 ); }
    | expression "<=" expression                                    { $$ = NEW_BINARY_OPERATOR(ASTBinaryOperator::Operator::LE, @2, $1, $3 ); }
    | expression ">=" expression                                    { $$ = NEW_BINARY_OPERATOR(ASTBinaryOperator::Operator::GE, @2, $1, $3 ); }
    | expression "==" expression                                    { $$ = NEW_BINARY_OPERATOR(ASTBinaryOperator::Operator::EQ, @2, $1, $3 ); }
    | expression "!=" expression                                    { $$ = NEW_BINARY_OPERATOR(ASTBinaryOperator::Operator::NE, @2, $1, $3 ); }
    | expression '&' expression                                     { $$ = NEW_BINARY_OPERATOR(ASTBinaryOperator::Operator::AND, @2, $1, $3 ); }
    | expression '^' expression                                     { $$ = NEW_BINARY_OPERATOR(ASTBinaryOperator::Operator::XOR, @2, $1, $3 ); }
    | expression '|' expression                                     { $$ = NEW_BINARY_OPERATOR(ASTBinaryOperator::Operator::OR, @2, $1, $3 ); }
    | expression "and" expression                                   { $$ = NEW_BINARY_OPERATOR(ASTBinaryOperator::Operator::LOGICAL_AND, @2, $1, $3 ); }
    | expression "xor" expression                                   { $$ = NEW_BINARY_OPERATOR(ASTBinaryOperator::Operator::LOGICAL_XOR, @2, $1, $3 ); }
    | expression "or" expression                                    { $$ = NEW_BINARY_OPERATOR(ASTBinaryOperator::Operator::LOGICAL_OR, @2, $1, $3 ); }
    | expression '[' expression ']'                                 { $$ = NEW_NODE(ASTIndex, @2, $1, $3); }
    | expression '[' ']'                                            { $$ = NEW_NODE(ASTIndex, @2, $1); }
    | expression '(' expressions ')'                                { $$ = NEW_NODE(ASTCall, @2, $1, $3); }
    ;

expressions:
      expression                                                    { $$ = NEW_NODE(ASTExpressionList, @1, $1); }
    | expressions ';' expression                                    { $1->add($3); $$ = $1; }
    ;

body:
      %empty                                                        { $$ = new TTauri::Config::ASTObject({context->file, 1, 1}); }
    | expressions                                                   { $$ = NEW_NODE(ASTObject, @1, $1); }
    | expressions ';'                                               { $$ = NEW_NODE(ASTObject, @1, $1); }
    ;

root:
    body                                                            { $$ = $1; context->object = $$; }
    ;

%%

int TTauriConfig_yyfind_token(char *text)
{
    size_t size = strlen(text);

    for (size_t symbolNumber = 0; symbolNumber < YYNTOKENS; symbolNumber++) {
        if (
            (yytname[symbolNumber] != 0) &&
            (yytname[symbolNumber][0] == '"') &&
            (strncmp (yytname[symbolNumber] + 1, text, size) == 0) &&
            (yytname[symbolNumber][size + 1] == '"') &&
            (yytname[symbolNumber][size + 2] == 0)
        ) {
            return yytoknum[symbolNumber];
        }
    }
    return yytoknum[1]; // "error" is the second entry in yytname.
}
