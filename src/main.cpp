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

static ofstream ofs;
static unordered_map<const void*, int> value_offsets;
static int stack_size = 0;

// 函数声明
static void Visit(const koopa_raw_program_t &program);
static void Visit(const koopa_raw_slice_t &slice);
static void Visit(const koopa_raw_function_t &func);
static void Visit(const koopa_raw_basic_block_t &bb);
static void Visit(const koopa_raw_value_t &value);
static void LoadValueToRegister(const koopa_raw_value_t &val, const string &reg);

static void CalculateStackSize(const koopa_raw_function_t &func) {
  stack_size = 0;
  value_offsets.clear();
  for (size_t i = 0; i < func->bbs.len; ++i) {
    auto bb = reinterpret_cast<koopa_raw_basic_block_t>(func->bbs.buffer[i]);
    for (size_t j = 0; j < bb->insts.len; ++j) {
      auto inst = reinterpret_cast<koopa_raw_value_t>(bb->insts.buffer[j]);
      if (inst->ty->tag != KOOPA_RTT_UNIT) {
        value_offsets[inst] = stack_size;
        stack_size += 4;
      }
    }
  }
}

// 访问 raw program
static void Visit(const koopa_raw_program_t &program) {
  // 执行一些其他的必要操作
  // ...
  ofs << "  .text" << endl;
  // 访问所有全局变量
  Visit(program.values);
  // 访问所有函数
  Visit(program.funcs);
}

// 访问 raw slice
static void Visit(const koopa_raw_slice_t &slice) {
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
static void Visit(const koopa_raw_function_t &func) {
  // 执行一些其他的必要操作
  // ...
  CalculateStackSize(func);
  
  int aligned_stack_size = (stack_size + 15) / 16 * 16;
  if (aligned_stack_size == 0) aligned_stack_size = 16;

  ofs << "  .globl " << (func->name + 1) << endl;
  ofs << (func->name + 1) << ":" << endl;

  if (aligned_stack_size > 0) {
    if (aligned_stack_size <= 2047) {
      ofs << "  addi sp, sp, " << -aligned_stack_size << endl;
    } else {
      ofs << "  li t0, " << -aligned_stack_size << endl;
      ofs << "  add sp, sp, t0" << endl;
    }
  }
  // 访问所有基本块
  Visit(func->bbs);
}

// 访问基本块
static void Visit(const koopa_raw_basic_block_t &bb) {
  // 执行一些其他的必要操作
  // ...
  if (string(bb->name + 1) != "entry") {
    ofs << bb->name + 1<< ":" << endl;
  }
  // 访问所有指令
  Visit(bb->insts);
}

// 访问指令
static void Visit(const koopa_raw_value_t &value) {
  // 根据指令类型判断后续需要如何访问
  const auto &kind = value->kind;
  switch (kind.tag) {
    case KOOPA_RVT_RETURN: {
      // 访问 return 指令
      const auto &ret = kind.data.ret;
      if (ret.value) {
        LoadValueToRegister(ret.value, "a0");
      }
      int aligned_stack_size = (stack_size + 15) / 16 * 16;
      if (aligned_stack_size == 0) aligned_stack_size = 16;
      if (aligned_stack_size > 0) {
        if (aligned_stack_size <= 2047) {
          ofs << "  addi sp, sp, " << aligned_stack_size << endl;
        } else {
          ofs << "  li t0, " << aligned_stack_size << endl;
          ofs << "  add sp, sp, t0" << endl;
        }
      }
      ofs << "  ret" << endl;
      break;
    }
    case KOOPA_RVT_BINARY: {
      // 访问 integer 指令
      const auto &binary = kind.data.binary;
      LoadValueToRegister(binary.lhs, "t0");
      LoadValueToRegister(binary.rhs, "t1");
      switch (binary.op) {
        case KOOPA_RBO_ADD: ofs << "  add t0, t0, t1" << endl; break;
        case KOOPA_RBO_SUB: ofs << "  sub t0, t0, t1" << endl; break;
        case KOOPA_RBO_MUL: ofs << "  mul t0, t0, t1" << endl; break;
        case KOOPA_RBO_DIV: ofs << "  div t0, t0, t1" << endl; break;
        case KOOPA_RBO_MOD: ofs << "  rem t0, t0, t1" << endl; break;
        case KOOPA_RBO_EQ: ofs << "  xor t0, t0, t1" << endl; ofs << "  seqz t0, t0" << endl; break;
        case KOOPA_RBO_NOT_EQ: ofs << "  xor t0, t0, t1" << endl; ofs << "  snez t0, t0" << endl; break;
        case KOOPA_RBO_GT: ofs << "  sgt t0, t0, t1" << endl; break;
        case KOOPA_RBO_LT: ofs << "  slt t0, t0, t1" << endl; break;
        case KOOPA_RBO_GE: ofs << "  slt t0, t0, t1" << endl; ofs << "  seqz t0, t0" << endl; break;
        case KOOPA_RBO_LE: ofs << "  sgt t0, t0, t1" << endl; ofs << "  seqz t0, t0" << endl; break;
        case KOOPA_RBO_AND: ofs << "  snez t0, t0" << endl; ofs << "  snez t1, t1" << endl; ofs << "  and t0, t0, t1" << endl; break;
        case KOOPA_RBO_OR: ofs << "  or t0, t0, t1" << endl; ofs << "  snez t0, t0" << endl; break;
        default: assert(false);
      }
      ofs << "  sw t0, " << value_offsets.at(value) << "(sp)" << endl;
      break;
    }
    case KOOPA_RVT_LOAD: {
      const auto &load = kind.data.load;
      ofs << "  lw t0, " << value_offsets.at(load.src) << "(sp)" << endl;
      ofs << "  sw t0, " << value_offsets.at(value) << "(sp)" << endl;
      break;
    }
    case KOOPA_RVT_STORE: {
      const auto &store = kind.data.store;
      LoadValueToRegister(store.value, "t0");
      ofs << "  sw t0, " << value_offsets.at(store.dest) << "(sp)" << endl;
      break;
    }
    case KOOPA_RVT_INTEGER:
    case KOOPA_RVT_ALLOC:
      break;
    case KOOPA_RVT_BRANCH: {
      const auto &branch = kind.data.branch;
      LoadValueToRegister(branch.cond, "t0");
      ofs << "  bnez t0, " << branch.true_bb->name + 1<< endl;
      ofs << "  j " << branch.false_bb->name + 1<< endl;
      break;
    }
    case KOOPA_RVT_JUMP: {
      const auto &jump = kind.data.jump;
      ofs << "  j " << jump.target->name + 1<< endl;
      break;
    }
    default:
      // 其他类型暂时遇不到
      assert(false);
  }
}

static void LoadValueToRegister(const koopa_raw_value_t &val, const string &reg) {
  if (val->kind.tag == KOOPA_RVT_INTEGER) {
    ofs << "  li " << reg << ", " << val->kind.data.integer.value << endl;
  } else {
    ofs << "  lw " << reg << ", " << value_offsets.at(val) << "(sp)" << endl;
  }
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

  unique_ptr<BaseAST> ast;
  auto ret = yyparse(ast);
  assert(!ret);

  ofs.open(output);
  if (string(mode) == "-koopa") {
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

    // 处理完成, 释放 raw program builder 占
      // 注意, raw program 中所有的指针指向的内存均为 raw program builder 的内存
      // 所以不要在 raw program 处理完毕之前释放 builder
      koopa_delete_raw_program_builder(builder);
  }
  ofs.close();
  return 0;
}
