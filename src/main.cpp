#include <cassert>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include "ast.h"
#include "koopa.h"

using namespace std;

// 声明 lexer 的输入, 以及 parser 函数
// 为什么不引用 sysy.tab.hpp 呢? 因为首先里面没有 yyin 的定义
// 其次, 因为这个文件不是我们自己写的, 而是被 Bison 生成出来的
// 你的代码编辑器/IDE 很可能找不到这个文件, 然后会给你报错 (虽然编译不会出错)
// 看起来会很烦人, 于是干脆采用这种看起来 dirty 但实际很有效的手段
extern FILE *yyin;
extern int yyparse(unique_ptr<BaseAST> &ast);

ofstream ofs;
int reg_count = 0;
unordered_map<void*, int> reg_map;

// 函数声明
void Visit(const koopa_raw_program_t &program);
void Visit(const koopa_raw_slice_t &slice);
void Visit(const koopa_raw_function_t &func);
void Visit(const koopa_raw_basic_block_t &bb);
void Visit(const koopa_raw_value_t &value);
void Visit(const koopa_raw_return_t &ret);
void Visit(const koopa_raw_integer_t &integer);
void Visit(const koopa_raw_binary_t &binary);

// 访问 raw program
void Visit(const koopa_raw_program_t &program) {
  // 执行一些其他的必要操作
  // ...
  ofs << "  .text" << endl;
  // 访问所有全局变量
  Visit(program.values);
  // 访问所有函数
  Visit(program.funcs);
}

// 访问 raw slice
void Visit(const koopa_raw_slice_t &slice) {
  for (size_t i = 0; i < slice.len; ++i) {
    auto ptr = slice.buffer[i];
    // 根据 slice 的 kind 决定将 ptr 视作何种元素
    switch (slice.kind) {
      case KOOPA_RSIK_FUNCTION:
        // 访问函数
        Visit(reinterpret_cast<koopa_raw_function_t>(ptr));
        break;
      case KOOPA_RSIK_BASIC_BLOCK:
        // 访问基本块
        Visit(reinterpret_cast<koopa_raw_basic_block_t>(ptr));
        break;
      case KOOPA_RSIK_VALUE:
        // 访问指令
        Visit(reinterpret_cast<koopa_raw_value_t>(ptr));
        break;
      default:
        // 我们暂时不会遇到其他内容, 于是不对其做任何处理
        assert(false);
    }
  }
}

// 访问函数
void Visit(const koopa_raw_function_t &func) {
  // 执行一些其他的必要操作
  // ...
  ofs << "  .globl " << (func->name + 1) << endl;
  ofs << (func->name + 1) << ":" << endl;
  // 访问所有基本块
  Visit(func->bbs);
}

// 访问基本块
void Visit(const koopa_raw_basic_block_t &bb) {
  // 执行一些其他的必要操作
  // ...
  // 访问所有指令
  Visit(bb->insts);
}

// 访问指令
void Visit(const koopa_raw_value_t &value) {
  // 根据指令类型判断后续需要如何访问
  const auto &kind = value->kind;
  switch (kind.tag) {
    case KOOPA_RVT_RETURN:
      // 访问 return 指令
      Visit(kind.data.ret);
      break;
    case KOOPA_RVT_INTEGER:
      // 访问 integer 指令
      Visit(kind.data.integer);
      reg_map[(void *)value] = reg_map[(void *)&kind.data.integer];
      break;
    case KOOPA_RVT_BINARY:
      Visit(kind.data.binary);
      reg_map[(void *)value] = reg_map[(void *)&kind.data.binary];
      break;
    default:
      // 其他类型暂时遇不到
      assert(false);
  }
}

// 访问对应类型指令的函数定义略
// 视需求自行实现
// ...

void Visit(const koopa_raw_return_t &ret) {
  Visit(ret.value);
  ofs << "  mv a0, t" << reg_map[(void *)ret.value] << endl;
  ofs << "  ret" << endl;
}

void Visit(const koopa_raw_integer_t &integer) {
  int32_t value = integer.value;
  ofs << "  li t" << reg_count << ", " << value << endl;
  reg_map[(void *)&integer] = reg_count++;
}

void Visit(const koopa_raw_binary_t &binary) {
  if (reg_map.find((void *)&binary) != reg_map.end()) {
    return;
  }
  Visit(binary.lhs);
  Visit(binary.rhs);
  int lhs_reg = reg_map[(void *)binary.lhs], rhs_reg = reg_map[(void *)binary.rhs];
  switch (binary.op) {
    case KOOPA_RBO_ADD:
      ofs << "  add t" << reg_count << ", t" << lhs_reg << ", t" << rhs_reg << endl;
      break;
    case KOOPA_RBO_SUB:
      ofs << "  sub t" << reg_count << ", t" << lhs_reg << ", t" << rhs_reg << endl;
      break;
    case KOOPA_RBO_EQ:
      ofs << "  xor t" << reg_count++ << ", t" << lhs_reg << ", t" << rhs_reg << endl;
      ofs << "  seqz t" << reg_count << ", t" << reg_count - 1 << endl;
      break;
    default:
      assert(false);
  }
  reg_map[(void *)&binary] = reg_count++;
}

int main(int argc, const char *argv[]) {
  // 解析命令行参数. 测试脚本/评测平台要求你的编译器能接收如下参数:
  // compiler 模式 输入文件 -o 输出文件
  assert(argc == 5);
  auto mode = argv[1];
  auto input = argv[2];
  auto output = argv[4];

  // 打开输入文件, 并且指定 lexer 在解析的时候读取这个文件
  yyin = fopen(input, "r");
  assert(yyin);

  // parse input file
  unique_ptr<BaseAST> ast;
  auto ret = yyparse(ast);
  assert(!ret);

  ofs.open(output);
  if (string(mode) == "-koopa") {
    // dump AST to Koopa IR
    ofs << *ast;
  } else if (string(mode) == "-riscv") {
      stringstream ss;
      ss << *ast;

      // 解析字符串 str, 得到 Koopa IR 程序
      koopa_program_t program;
      koopa_error_code_t parse_ret = koopa_parse_from_string(ss.str().c_str(), &program);
      assert(parse_ret == KOOPA_EC_SUCCESS);  // 确保解析时没有出错
      // 创建一个 raw program builder, 用来构建 raw program
      koopa_raw_program_builder_t builder = koopa_new_raw_program_builder();
      // 将 Koopa IR 程序转换为 raw program
      koopa_raw_program_t raw = koopa_build_raw_program(builder, program);
      // 释放 Koopa IR 程序占用的内存
      koopa_delete_program(program);

      // 处理 raw program
      // ...

      Visit(raw);

      // 处理完成, 释放 raw program builder 占用的内存
      // 注意, raw program 中所有的指针指向的内存均为 raw program builder 的内存
      // 所以不要在 raw program 处理完毕之前释放 builder
      koopa_delete_raw_program_builder(builder);
  }
  ofs.close();
  return 0;
}
