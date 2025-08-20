#pragma once

#include <iostream>
#include <memory>
#include <string>

// 所有 AST 的基类
class BaseAST {
 public:
  virtual ~BaseAST() = default;

  virtual void print(std::ostream& os) const = 0;
  
  friend std::ostream& operator<<(std::ostream& os, const BaseAST& ast) {
    ast.print(os);
    return os;
  }
};

// CompUnit 是 BaseAST
class CompUnitAST : public BaseAST {
 public:
  // 用智能指针管理对象
  std::unique_ptr<BaseAST> func_def;

  void print(std::ostream& os) const override {
    os << *func_def;
  }
};

// FuncDef 也是 BaseAST
class FuncDefAST : public BaseAST {
 public:
  std::unique_ptr<BaseAST> func_type;
  std::string ident;
  std::unique_ptr<BaseAST> block;

  void print(std::ostream& os) const override {
    os << "fun @" << ident << "(): ";
    os << *func_type;
    os << *block;
  }
};

class FuncTypeAST : public BaseAST {
 public:
  std::string type;

  void print(std::ostream& os) const override {
    if (type == "int") {
      os << "i32 ";
    } else {
      os << "unknown ";
    }
  }
};

class BlockAST : public BaseAST {
 public:
  std::unique_ptr<BaseAST> stmt;

  void print(std::ostream& os) const override {
    os << "{" << std::endl;
    os << "%entry:" << std::endl;
    os << *stmt;
    os << "}" << std::endl;
  }
};

class StmtAST : public BaseAST {
 public:
  int number;

  void print(std::ostream& os) const override {
    os << "  ret " << number << std::endl;
  }
};
