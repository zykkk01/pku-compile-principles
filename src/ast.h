#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <stdexcept>

extern int var_count;

struct IRResult;
class SymbolTableManager;
class SymbolTable;

// 所有 AST 的基类
class BaseAST {
public:
    virtual ~BaseAST() = default;
    virtual IRResult generate_ir(std::ostream& os, SymbolTableManager& symbols) const = 0;
    virtual int evaluate_const(SymbolTableManager& symbols) const {
        throw std::logic_error("evaluate_const not implemented for this AST node");
    }
    
    friend std::ostream& operator<<(std::ostream& os, const BaseAST& ast);
};

// CompUnit 是 BaseAST
class CompUnitAST : public BaseAST {
public:
    std::unique_ptr<BaseAST> func_def;
    IRResult generate_ir(std::ostream& os, SymbolTableManager& symbols) const override;
};

// FuncDef 也是 BaseAST
class FuncDefAST : public BaseAST {
public:
    std::unique_ptr<BaseAST> func_type;
    std::string ident;
    std::unique_ptr<BaseAST> block;
    IRResult generate_ir(std::ostream& os, SymbolTableManager& symbols) const override;
};

class FuncTypeAST : public BaseAST {
public:
    std::string type;
    IRResult generate_ir(std::ostream& os, SymbolTableManager& symbols) const override;
};

class DeclAST : public BaseAST {
public:
    std::unique_ptr<BaseAST> const_decl;
    std::unique_ptr<BaseAST> var_decl;
    IRResult generate_ir(std::ostream& os, SymbolTableManager& symbols) const override;
};

class ConstDeclAST : public BaseAST {
public:
    std::unique_ptr<BaseAST> b_type;
    std::vector<std::unique_ptr<BaseAST>> const_defs;
    IRResult generate_ir(std::ostream& os, SymbolTableManager& symbols) const override;
};

class BTypeAST : public BaseAST {
public:
    std::string type;
    IRResult generate_ir(std::ostream& os, SymbolTableManager& symbols) const override;
};

class ConstDefAST : public BaseAST {
public:
    std::string ident;
    std::unique_ptr<BaseAST> const_init_val;
    IRResult generate_ir(std::ostream& os, SymbolTableManager& symbols) const override;
};

class ConstInitValAST : public BaseAST {
public:
    std::unique_ptr<BaseAST> const_exp;
    IRResult generate_ir(std::ostream& os, SymbolTableManager& symbols) const override;
    int evaluate_const(SymbolTableManager& symbols) const override;
};

class VarDeclAST : public BaseAST {
public:
    std::unique_ptr<BaseAST> b_type;
    std::vector<std::unique_ptr<BaseAST>> var_defs;
    IRResult generate_ir(std::ostream& os, SymbolTableManager& symbols) const override;
};

class VarDefAST : public BaseAST {
public:
    std::string ident;
    std::unique_ptr<BaseAST> init_val;
    IRResult generate_ir(std::ostream& os, SymbolTableManager& symbols) const override;
};

class InitValAST : public BaseAST {
public:
    std::unique_ptr<BaseAST> exp;
    IRResult generate_ir(std::ostream& os, SymbolTableManager& symbols) const override;
};

class BlockAST : public BaseAST {
public:
    std::vector<std::unique_ptr<BaseAST>> block_items;
    
    IRResult generate_ir(std::ostream& os, SymbolTableManager& symbols) const override;
};

class StmtAST : public BaseAST {
public:
    std::unique_ptr<BaseAST> lval;
    std::unique_ptr<BaseAST> exp;
    std::unique_ptr<BaseAST> block;
    bool is_return = false;
    std::unique_ptr<BaseAST> cond_exp;
    std::unique_ptr<BaseAST> if_stmt;
    std::unique_ptr<BaseAST> else_stmt;
    IRResult generate_ir(std::ostream& os, SymbolTableManager& symbols) const override;
};

class ExpAST : public BaseAST {
public:
    std::unique_ptr<BaseAST> lor_exp;
    IRResult generate_ir(std::ostream& os, SymbolTableManager& symbols) const override;
    int evaluate_const(SymbolTableManager& symbols) const override;
};

class ConstExpAST : public BaseAST {
public:
    std::unique_ptr<BaseAST> exp;
    IRResult generate_ir(std::ostream& os, SymbolTableManager& symbols) const override;
    int evaluate_const(SymbolTableManager& symbols) const override;
};

class LValAST : public BaseAST {
public:
    std::string ident;
    IRResult generate_ir(std::ostream& os, SymbolTableManager& symbols) const override;
    int evaluate_const(SymbolTableManager& symbols) const override;
};

class PrimaryExpAST : public BaseAST {
public:
    std::unique_ptr<BaseAST> exp;
    std::unique_ptr<BaseAST> lval;
    int number;
    IRResult generate_ir(std::ostream& os, SymbolTableManager& symbols) const override;
    int evaluate_const(SymbolTableManager& symbols) const override;
};

class UnaryExpAST : public BaseAST {
public:
    std::unique_ptr<BaseAST> primary_exp;
    std::unique_ptr<BaseAST> unary_exp;
    std::string unary_op;
    IRResult generate_ir(std::ostream& os, SymbolTableManager& symbols) const override;
    int evaluate_const(SymbolTableManager& symbols) const override;
};

class MulExpAST : public BaseAST {
public:
    std::unique_ptr<BaseAST> unary_exp;
    std::unique_ptr<BaseAST> mul_exp;
    std::string mul_op;
    IRResult generate_ir(std::ostream& os, SymbolTableManager& symbols) const override;
    int evaluate_const(SymbolTableManager& symbols) const override;
};

class AddExpAST : public BaseAST {
public:
    std::unique_ptr<BaseAST> mul_exp;
    std::unique_ptr<BaseAST> add_exp;
    std::string add_op;
    IRResult generate_ir(std::ostream& os, SymbolTableManager& symbols) const override;
    int evaluate_const(SymbolTableManager& symbols) const override;
};

class RelExpAST : public BaseAST {
public:
    std::unique_ptr<BaseAST> add_exp;
    std::unique_ptr<BaseAST> rel_exp;
    std::string rel_op;
    IRResult generate_ir(std::ostream& os, SymbolTableManager& symbols) const override;
    int evaluate_const(SymbolTableManager& symbols) const override;
};

class EqExpAST : public BaseAST {
public:
    std::unique_ptr<BaseAST> rel_exp;
    std::unique_ptr<BaseAST> eq_exp;
    std::string eq_op;
    IRResult generate_ir(std::ostream& os, SymbolTableManager& symbols) const override;
    int evaluate_const(SymbolTableManager& symbols) const override;
};

class LAndExpAST : public BaseAST {
public:
    std::unique_ptr<BaseAST> eq_exp;
    std::unique_ptr<BaseAST> land_exp;
    std::string land_op;
    IRResult generate_ir(std::ostream& os, SymbolTableManager& symbols) const override;
    int evaluate_const(SymbolTableManager& symbols) const override;
};

class LOrExpAST : public BaseAST {
public:
    std::unique_ptr<BaseAST> land_exp;
    std::unique_ptr<BaseAST> lor_exp;
    std::string lor_op;
    IRResult generate_ir(std::ostream& os, SymbolTableManager& symbols) const override;
    int evaluate_const(SymbolTableManager& symbols) const override;
};
