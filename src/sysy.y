%code requires {
  #include <memory>
  #include <string>
  #include <vector>
  #include "ast.h"
}

%{

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include "ast.h"

int yylex();
void yyerror(std::unique_ptr<BaseAST> &ast, const char *s);

using namespace std;

%}

%parse-param { std::unique_ptr<BaseAST> &ast }

%union {
  std::string *str_val;
  int int_val;
  BaseAST *ast_val;
  std::vector<BaseAST *> *ast_list_val;
}

%token INT RETURN CONST IF ELSE WHILE BREAK CONTINUE VOID
%token <str_val> IDENT
%token <int_val> INT_CONST
%token ADD SUB NOT MUL DIV MOD EQ NE GT LT GE LE LAND LOR

%type <ast_val> FuncDef Block Stmt Exp UnaryExp PrimaryExp AddExp MulExp RelExp EqExp LAndExp LOrExp
%type <ast_val> Decl ConstDecl BType ConstDef ConstInitVal ConstExp LVal BlockItem VarDecl VarDef InitVal
%type <ast_val> MatchedStmt UnmatchedStmt FuncFParam CompUnitItem
%type <ast_list_val> BlockItemList ConstDefList VarDefList FuncFParams FuncRParams CompUnitItemList
%type <ast_list_val> ConstExpList ExpList
%type <int_val> Number
%type <str_val> UnaryOp AddOp MulOp RelOp EqOp

%%

CompUnit
  : CompUnitItemList {
    auto comp_unit = make_unique<CompUnitAST>();
    auto items = unique_ptr<vector<BaseAST *>>($1);
    for (auto &item : *items) {
      comp_unit->items.emplace_back(item);
    }
    ast = move(comp_unit);
  }
  ;

CompUnitItemList
  : /* empty */ { $$ = new vector<BaseAST *>(); }
  | CompUnitItemList CompUnitItem {
    $1->push_back($2);
    $$ = $1;
  }
  ;

CompUnitItem
  : Decl { $$ = $1; }
  | FuncDef { $$ = $1; }
  ;

FuncDef
  : BType IDENT '(' ')' Block {
    auto ast = new FuncDefAST();
    ast->func_type = unique_ptr<BaseAST>($1);
    ast->ident = *unique_ptr<string>($2);
    ast->block = unique_ptr<BaseAST>($5);
    $$ = ast;
  }
  | BType IDENT '(' FuncFParams ')' Block {
    auto ast = new FuncDefAST();
    ast->func_type = unique_ptr<BaseAST>($1);
    ast->ident = *unique_ptr<string>($2);
    auto params = unique_ptr<vector<BaseAST *>>($4);
    for (auto &param : *params) {
      ast->func_f_params.emplace_back(param);
    }
    ast->block = unique_ptr<BaseAST>($6);
    $$ = ast;
  }
  ;

FuncFParams
  : FuncFParam {
    auto list = new vector<BaseAST *>();
    list->push_back($1);
    $$ = list;
  }
  | FuncFParams ',' FuncFParam {
    $1->push_back($3);
    $$ = $1;
  }
  ;

FuncFParam
  : BType IDENT {
    auto ast = new FuncFParamAST();
    ast->b_type = unique_ptr<BaseAST>($1);
    ast->ident = *unique_ptr<string>($2);
    $$ = ast;
  }
  ;

Block
  : '{' BlockItemList '}' {
    auto ast = new BlockAST();
    auto block_items = unique_ptr<vector<BaseAST *>>($2);
    for (auto &item : *block_items) {
      ast->block_items.emplace_back(item);
    }
    $$ = ast;
  }
  ;

BlockItemList
  : /* empty */ { $$ = new vector<BaseAST *>(); }
  | BlockItemList BlockItem {
    $1->push_back($2);
    $$ = $1;
  }
  ;

BlockItem
  : Decl { $$ = $1; }
  | Stmt { $$ = $1; }
  ;

Decl
  : ConstDecl {
    auto ast = new DeclAST();
    ast->const_decl = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | VarDecl {
    auto ast = new DeclAST();
    ast->var_decl = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  ;

ConstDecl
  : CONST BType ConstDefList ';' {
    auto ast = new ConstDeclAST();
    ast->b_type = unique_ptr<BaseAST>($2);
    auto defs = unique_ptr<vector<BaseAST *>>($3);
    for (auto &def : *defs) {
      ast->const_defs.emplace_back(def);
    }
    $$ = ast;
  }
  ;

ConstDefList
  : ConstDef {
    auto list = new vector<BaseAST *>();
    list->push_back($1);
    $$ = list;
  }
  | ConstDefList ',' ConstDef {
    $1->push_back($3);
    $$ = $1;
  }
  ;

BType
  : INT {
    auto ast = new BTypeAST();
    ast->type = "int";
    $$ = ast;
  }
  | VOID {
    auto ast = new BTypeAST();
    ast->type = "void";
    $$ = ast;
  }
  ;

ConstDef
  : IDENT '=' ConstInitVal {
    auto ast = new ConstDefAST();
    ast->ident = *unique_ptr<string>($1);
    ast->const_init_val = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  | IDENT '[' ConstExp ']' '=' ConstInitVal {
    auto ast = new ConstDefAST();
    ast->ident = *unique_ptr<string>($1);
    ast->array_size_exp = unique_ptr<BaseAST>($3);
    ast->const_init_val = unique_ptr<BaseAST>($6);
    $$ = ast;
  }
  ;

ConstInitVal
  : ConstExp {
    auto ast = new ConstInitValAST();
    ast->const_exp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | '{' '}' {
    auto ast = new ConstInitValAST();
    $$ = ast;
  }
  | '{' ConstExpList '}' {
    auto ast = new ConstInitValAST();
    auto exps = unique_ptr<vector<BaseAST *>>($2);
    for (auto &exp : *exps) {
      ast->const_exps.emplace_back(exp);
    }
    $$ = ast;
  }
  ;

ConstExpList
  : ConstExp {
    auto list = new vector<BaseAST *>();
    list->push_back($1);
    $$ = list;
  }
  | ConstExpList ',' ConstExp {
    $1->push_back($3);
    $$ = $1;
  }
  ;

ConstExp
  : Exp {
    auto ast = new ConstExpAST();
    ast->exp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  ;

VarDecl
  : BType VarDefList ';' {
    auto ast = new VarDeclAST();
    ast->b_type = unique_ptr<BaseAST>($1);
    auto defs = unique_ptr<vector<BaseAST *>>($2);
    for (auto &def : *defs) {
      ast->var_defs.emplace_back(def);
    }
    $$ = ast;
  }
  ;

VarDefList
  : VarDef {
    auto list = new vector<BaseAST *>();
    list->push_back($1);
    $$ = list;
  }
  | VarDefList ',' VarDef {
    $1->push_back($3);
    $$ = $1;
  }
  ;

VarDef
  : IDENT {
    auto ast = new VarDefAST();
    ast->ident = *unique_ptr<string>($1);
    $$ = ast;
  }
  | IDENT '[' ConstExp ']' {
    auto ast = new VarDefAST();
    ast->ident = *unique_ptr<string>($1);
    ast->array_size_exp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  | IDENT '=' InitVal {
    auto ast = new VarDefAST();
    ast->ident = *unique_ptr<string>($1);
    ast->init_val = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  | IDENT '[' ConstExp ']' '=' InitVal {
    auto ast = new VarDefAST();
    ast->ident = *unique_ptr<string>($1);
    ast->array_size_exp = unique_ptr<BaseAST>($3);
    ast->init_val = unique_ptr<BaseAST>($6);
    $$ = ast;
  }
  ;

InitVal
  : Exp {
    auto ast = new InitValAST();
    ast->exp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | '{' '}' {
    auto ast = new InitValAST();
    $$ = ast;
  }
  | '{' ExpList '}' {
    auto ast = new InitValAST();
    auto exps = unique_ptr<vector<BaseAST *>>($2);
    for (auto &exp : *exps) {
      ast->exps.emplace_back(exp);
    }
    $$ = ast;
  }
  ;

ExpList
  : Exp {
    auto list = new vector<BaseAST *>();
    list->push_back($1);
    $$ = list;
  }
  | ExpList ',' Exp {
    $1->push_back($3);
    $$ = $1;
  }
  ;

Stmt
  : MatchedStmt
  | UnmatchedStmt
  ;

MatchedStmt
  : LVal '=' Exp ';' {
    auto ast = new StmtAST();
    ast->type = StmtType::ASSIGN_STMT;
    ast->lval = unique_ptr<BaseAST>($1);
    ast->exp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  | Exp ';' {
    auto ast = new StmtAST();
    ast->type = StmtType::EXPRESSION_STMT;
    ast->exp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | ';' {
    auto ast = new StmtAST();
    ast->type = StmtType::EMPTY_STMT;
    $$ = ast;
  }
  | Block {
    auto ast = new StmtAST();
    ast->type = StmtType::BLOCK_STMT;
    ast->block = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | IF '(' Exp ')' MatchedStmt ELSE MatchedStmt {
    auto ast = new StmtAST();
    ast->type = StmtType::IF_STMT;
    ast->cond_exp = unique_ptr<BaseAST>($3);
    ast->if_stmt = unique_ptr<BaseAST>($5);
    ast->else_stmt = unique_ptr<BaseAST>($7);
    $$ = ast;
  }
  | WHILE '(' Exp ')' Stmt {
    auto ast = new StmtAST();
    ast->type = StmtType::WHILE_STMT;
    ast->cond_exp = unique_ptr<BaseAST>($3);
    ast->while_stmt = unique_ptr<BaseAST>($5);
    $$ = ast;
  }
  | BREAK ';' {
    auto ast = new StmtAST();
    ast->type = StmtType::BREAK_STMT;
    $$ = ast;
  }
  | CONTINUE ';' {
    auto ast = new StmtAST();
    ast->type = StmtType::CONTINUE_STMT;
    $$ = ast;
  }
  | RETURN Exp ';' {
    auto ast = new StmtAST();
    ast->type = StmtType::RETURN_STMT;
    ast->exp = unique_ptr<BaseAST>($2);
    $$ = ast;
  }
  ;

UnmatchedStmt
  : IF '(' Exp ')' Stmt {
    auto ast = new StmtAST();
    ast->type = StmtType::IF_STMT;
    ast->cond_exp = unique_ptr<BaseAST>($3);
    ast->if_stmt = unique_ptr<BaseAST>($5);
    $$ = ast;
  }
  | IF '(' Exp ')' MatchedStmt ELSE UnmatchedStmt {
    auto ast = new StmtAST();
    ast->type = StmtType::IF_STMT;
    ast->cond_exp = unique_ptr<BaseAST>($3);
    ast->if_stmt = unique_ptr<BaseAST>($5);
    ast->else_stmt = unique_ptr<BaseAST>($7);
    $$ = ast;
  }
  ;

Exp
  : LOrExp {
    auto ast = new ExpAST();
    ast->lor_exp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  ;

LOrExp
  : LAndExp {
    auto ast = new LOrExpAST();
    ast->land_exp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | LOrExp LOR LAndExp {
    auto ast = new LOrExpAST();
    ast->lor_exp = unique_ptr<BaseAST>($1);
    ast->lor_op = "||";
    ast->land_exp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  ;

LAndExp
  : EqExp {
    auto ast = new LAndExpAST();
    ast->eq_exp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | LAndExp LAND EqExp {
    auto ast = new LAndExpAST();
    ast->land_exp = unique_ptr<BaseAST>($1);
    ast->land_op = "&&";
    ast->eq_exp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  ;

EqExp
  : RelExp {
    auto ast = new EqExpAST();
    ast->rel_exp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | EqExp EqOp RelExp {
    auto ast = new EqExpAST();
    ast->eq_exp = unique_ptr<BaseAST>($1);
    ast->eq_op = *unique_ptr<string>($2);
    ast->rel_exp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  ;

RelExp
  : AddExp {
    auto ast = new RelExpAST();
    ast->add_exp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | RelExp RelOp AddExp {
    auto ast = new RelExpAST();
    ast->rel_exp = unique_ptr<BaseAST>($1);
    ast->rel_op = *unique_ptr<string>($2);
    ast->add_exp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  ;

AddExp
  : MulExp {
    auto ast = new AddExpAST();
    ast->mul_exp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | AddExp AddOp MulExp {
    auto ast = new AddExpAST();
    ast->add_exp = unique_ptr<BaseAST>($1);
    ast->add_op = *unique_ptr<string>($2);
    ast->mul_exp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  ;

MulExp
  : UnaryExp {
    auto ast = new MulExpAST();
    ast->unary_exp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | MulExp MulOp UnaryExp {
    auto ast = new MulExpAST();
    ast->mul_exp = unique_ptr<BaseAST>($1);
    ast->mul_op = *unique_ptr<string>($2);
    ast->unary_exp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  ;

UnaryExp
  : PrimaryExp {
    auto ast = new UnaryExpAST();
    ast->primary_exp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | UnaryOp UnaryExp {
    auto ast = new UnaryExpAST();
    ast->unary_op = *unique_ptr<string>($1);
    ast->unary_exp = unique_ptr<BaseAST>($2);
    $$ = ast;
  }
  | IDENT '(' ')' {
    auto ast = new UnaryExpAST();
    ast->ident = *unique_ptr<string>($1);
    $$ = ast;
  }
  | IDENT '(' FuncRParams ')' {
    auto ast = new UnaryExpAST();
    ast->ident = *unique_ptr<string>($1);
    auto params = unique_ptr<vector<BaseAST *>>($3);
    for (auto &param : *params) {
      ast->func_r_params.emplace_back(param);
    }
    $$ = ast;
  }
  ;

FuncRParams
  : Exp {
    auto list = new vector<BaseAST *>();
    list->push_back($1);
    $$ = list;
  }
  | FuncRParams ',' Exp {
    $1->push_back($3);
    $$ = $1;
  }
  ;

PrimaryExp
  : '(' Exp ')' {
    auto ast = new PrimaryExpAST();
    ast->exp = unique_ptr<BaseAST>($2);
    $$ = ast;
  }
  | LVal {
    auto ast = new PrimaryExpAST();
    ast->lval = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | Number {
    auto ast = new PrimaryExpAST();
    ast->number = $1;
    $$ = ast;
  }
  ;

LVal
  : IDENT {
    auto ast = new LValAST();
    ast->ident = *unique_ptr<string>($1);
    $$ = ast;
  }
  | IDENT '[' Exp ']' {
    auto ast = new LValAST();
    ast->ident = *unique_ptr<string>($1);
    ast->array_index_exp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  ;

Number
  : INT_CONST {
    $$ = $1;
  }
  ;

UnaryOp
  : ADD { $$ = new string("+"); }
  | SUB { $$ = new string("-"); }
  | NOT { $$ = new string("!"); }
  ;

AddOp
  : ADD { $$ = new string("+"); }
  | SUB { $$ = new string("-"); }
  ;

MulOp
  : MUL { $$ = new string("*"); }
  | DIV { $$ = new string("/"); }
  | MOD { $$ = new string("%"); }
  ;

RelOp
  : LT { $$ = new string("<"); }
  | GT { $$ = new string(">"); }
  | LE { $$ = new string("<="); }
  | GE { $$ = new string(">="); }
  ;

EqOp
  : EQ { $$ = new string("=="); }
  | NE { $$ = new string("!="); }
  ;

%%

void yyerror(unique_ptr<BaseAST> &ast, const char *s) {
  cerr << "error: " << s << endl;
}
