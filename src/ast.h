#pragma once

#include <iostream>
#include <memory>
#include <string>

extern int var_count;

// 所有 AST 的基类
class BaseAST {
public:
    virtual ~BaseAST() = default;
    virtual void print(std::ostream& os) const = 0;
    
    friend std::ostream& operator<<(std::ostream& os, const BaseAST& ast);
};

// CompUnit 是 BaseAST
class CompUnitAST : public BaseAST {
public:
    std::unique_ptr<BaseAST> func_def;
    void print(std::ostream& os) const override;
};

// FuncDef 也是 BaseAST
class FuncDefAST : public BaseAST {
public:
    std::unique_ptr<BaseAST> func_type;
    std::string ident;
    std::unique_ptr<BaseAST> block;
    void print(std::ostream& os) const override;
};

class FuncTypeAST : public BaseAST {
public:
    std::string type;
    void print(std::ostream& os) const override;
};

class BlockAST : public BaseAST {
public:
    std::unique_ptr<BaseAST> stmt;
    void print(std::ostream& os) const override;
};

class StmtAST : public BaseAST {
public:
    std::unique_ptr<BaseAST> exp;
    void print(std::ostream& os) const override;
};

class ExpAST : public BaseAST {
public:
    std::unique_ptr<BaseAST> lor_exp;
    void print(std::ostream& os) const override;
};

class PrimaryExpAST : public BaseAST {
public:
    std::unique_ptr<BaseAST> exp;
    int number;
    void print(std::ostream& os) const override;
};

class UnaryExpAST : public BaseAST {
public:
    std::unique_ptr<BaseAST> primary_exp;
    std::unique_ptr<BaseAST> unary_exp;
    std::string unary_op;
    void print(std::ostream& os) const override;
};

class MulExpAST : public BaseAST {
public:
    std::unique_ptr<BaseAST> unary_exp;
    std::unique_ptr<BaseAST> mul_exp;
    std::string mul_op;
    void print(std::ostream& os) const override;
};

class AddExpAST : public BaseAST {
public:
    std::unique_ptr<BaseAST> mul_exp;
    std::unique_ptr<BaseAST> add_exp;
    std::string add_op;
    void print(std::ostream& os) const override;
};

class RelExpAST : public BaseAST {
public:
    std::unique_ptr<BaseAST> add_exp;
    std::unique_ptr<BaseAST> rel_exp;
    std::string rel_op;
    void print(std::ostream& os) const override;
};

class EqExpAST : public BaseAST {
public:
    std::unique_ptr<BaseAST> rel_exp;
    std::unique_ptr<BaseAST> eq_exp;
    std::string eq_op;
    void print(std::ostream& os) const override;
};

class LAndExpAST : public BaseAST {
public:
    std::unique_ptr<BaseAST> eq_exp;
    std::unique_ptr<BaseAST> land_exp;
    std::string land_op;
    void print(std::ostream& os) const override;
};

class LOrExpAST : public BaseAST {
public:
    std::unique_ptr<BaseAST> land_exp;
    std::unique_ptr<BaseAST> lor_exp;
    std::string lor_op;
    void print(std::ostream& os) const override;
};
