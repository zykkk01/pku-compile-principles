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

extern FILE *yyin;
extern int yyparse(unique_ptr<BaseAST> &ast);

static ofstream ofs;
static int stack_size;
static bool is_ra_saved;
static string current_func_name;
enum class ValueType {
  STACK,
  REGISTER,
  GLOBAL
};
struct ValueInfo {
  int offset;
  std::string reg;
  ValueType type;
};
static unordered_map<const void*, ValueInfo> value_info_map;

static void Visit(const koopa_raw_program_t &program);
static void Visit(const koopa_raw_slice_t &slice);
static void Visit(const koopa_raw_function_t &func);
static void Visit(const koopa_raw_basic_block_t &bb);
static void Visit(const koopa_raw_value_t &value);
static void LoadValueToRegister(const koopa_raw_value_t &val, const string &reg);
static void SaveValueFromRegister(const koopa_raw_value_t &val, const string &reg, const string &tmp);
static void EmitSPRelativeAccess(const string &inst, const string &data_reg, int offset, const string &temp_reg);
static void MoveValueToRegister(const koopa_raw_value_t &val, const string &reg);
static void MoveValueFromRegister(const koopa_raw_value_t &val, const string &reg);

int get_array_size(const koopa_raw_type_t arr) {
  if(arr->tag == KOOPA_RTT_ARRAY) {
    return arr->data.array.len * get_array_size(arr->data.array.base);
  } else if(arr->tag == KOOPA_RTT_POINTER) {
    return get_array_size(arr->data.pointer.base);
  } else {
    return 4;
  }
}

void CalculateStackSize(const koopa_raw_function_t &func) {
  stack_size = 0;
  is_ra_saved = false;
  int stack_param_num = 0;
  for (size_t i = 0; i < func->bbs.len; ++i) {
    auto bb = reinterpret_cast<koopa_raw_basic_block_t>(func->bbs.buffer[i]);
    for (size_t j = 0; j < bb->insts.len; ++j) {
      auto inst = reinterpret_cast<koopa_raw_value_t>(bb->insts.buffer[j]);
      if (inst->kind.tag == KOOPA_RVT_CALL) {
        is_ra_saved = true;
        const auto &call = inst->kind.data.call;
        stack_param_num = max(stack_param_num, (int)call.args.len - 8);
      }
    }
  }

  for (size_t i = 0; i < func->bbs.len; ++i) {
    auto bb = reinterpret_cast<koopa_raw_basic_block_t>(func->bbs.buffer[i]);
    for (size_t j = 0; j < bb->insts.len; ++j) {
      auto inst = reinterpret_cast<koopa_raw_value_t>(bb->insts.buffer[j]);
      if (inst->kind.tag == KOOPA_RVT_ALLOC) {
        ValueInfo info;
        info.offset = stack_size + stack_param_num * 4;
        info.type = ValueType::STACK;
        value_info_map[inst] = info;
        stack_size += get_array_size(inst->ty);
      } else if (inst->ty->tag != KOOPA_RTT_UNIT) {
        ValueInfo info;
        info.offset = stack_size + stack_param_num * 4;
        info.type = ValueType::STACK;
        value_info_map[inst] = info;
        stack_size += 4;
      }
    }
  }
  stack_size = (stack_size + (int)is_ra_saved * 4 + stack_param_num * 4 + 15) / 16 * 16;
}

static void EmitSPRelativeAccess(const string &inst, const string &data_reg, int offset, const string &temp_reg) {
  if (offset >= -2048 && offset <= 2047) {
    ofs << "  " << inst << " " << data_reg << ", " << offset << "(sp)" << endl;
  } else {
    ofs << "  li " << temp_reg << ", " << offset << endl;
    ofs << "  add " << temp_reg << ", sp, " << temp_reg << endl;
    ofs << "  " << inst << " " << data_reg << ", 0(" << temp_reg << ")" << endl;
  }
}

static void Visit(const koopa_raw_program_t &program) {
  Visit(program.values);
  Visit(program.funcs);
}

static void Visit(const koopa_raw_slice_t &slice) {
  for (size_t i = 0; i < slice.len; ++i) {
    auto ptr = slice.buffer[i];
    switch (slice.kind) {
      case KOOPA_RSIK_FUNCTION:
        Visit(reinterpret_cast<koopa_raw_function_t>(ptr));
        break;
      case KOOPA_RSIK_BASIC_BLOCK:
        Visit(reinterpret_cast<koopa_raw_basic_block_t>(ptr));
        break;
      case KOOPA_RSIK_VALUE:
        Visit(reinterpret_cast<koopa_raw_value_t>(ptr));
        break;
      default:
        assert(false);
    }
  }
}

static void Visit(const koopa_raw_function_t &func) {
  if (func->bbs.len == 0) return;
  current_func_name = string(func->name + 1);
  CalculateStackSize(func);

  ofs << "  .text" << endl;
  ofs << "  .globl " << current_func_name << endl;
  ofs << current_func_name << ":" << endl;

  if (stack_size > 0) {
    if (stack_size <= 2047) {
      ofs << "  addi sp, sp, " << -stack_size << endl;
    } else {
      ofs << "  li t0, " << -stack_size << endl;
      ofs << "  add sp, sp, t0" << endl;
    }
  }
  if (is_ra_saved) {
    EmitSPRelativeAccess("sw", "ra", stack_size - 4, "t0");
  }
  Visit(func->bbs);
  ofs << current_func_name << "_end:" << endl;
  if (is_ra_saved) {
    EmitSPRelativeAccess("lw", "ra", stack_size - 4, "t0");
  }
  if (stack_size > 0) {
    if (stack_size <= 2047) {
      ofs << "  addi sp, sp, " << stack_size << endl;
    } else {
      ofs << "  li t0, " << stack_size << endl;
      ofs << "  add sp, sp, t0" << endl;
    }
  }
  ofs << "  ret" << endl << endl;
}

static void Visit(const koopa_raw_basic_block_t &bb) {
  if (string(bb->name + 1) != "entry") {
    ofs << current_func_name << "_" << bb->name + 1<< ":" << endl;
  }
  Visit(bb->insts);
}

static void Visit(const koopa_raw_value_t &value) {
  const auto &kind = value->kind;
  switch (kind.tag) {
    case KOOPA_RVT_RETURN: {
      const auto &ret = kind.data.ret;
      if (ret.value) {
        MoveValueToRegister(ret.value, "a0");
      }
      ofs << "  j " << current_func_name << "_end" << endl;
      break;
    }
    case KOOPA_RVT_BINARY: {
      const auto &binary = kind.data.binary;
      MoveValueToRegister(binary.lhs, "t0");
      MoveValueToRegister(binary.rhs, "t1");
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
      MoveValueFromRegister(value, "t0");
      break;
    }
    case KOOPA_RVT_LOAD: {
      const auto &load = kind.data.load;
      LoadValueToRegister(load.src, "t0");
      MoveValueFromRegister(value, "t0");
      break;
    }
    case KOOPA_RVT_STORE: {
      const auto &store = kind.data.store;
      MoveValueToRegister(store.value, "t0");
      SaveValueFromRegister(store.dest, "t0", "t1");
      break;
    }
    case KOOPA_RVT_INTEGER:
    case KOOPA_RVT_ALLOC:
      break;
    case KOOPA_RVT_BRANCH: {
      const auto &branch = kind.data.branch;
      MoveValueToRegister(branch.cond, "t0");
      ofs << "  bnez t0, " << current_func_name << "_" << branch.true_bb->name + 1 << endl;
      ofs << "  j " << current_func_name << "_" << branch.false_bb->name + 1 << endl;
      break;
    }
    case KOOPA_RVT_JUMP: {
      const auto &jump = kind.data.jump;
      ofs << "  j " << current_func_name << "_" << jump.target->name + 1 << endl;
      break;
    }
    case KOOPA_RVT_CALL: {
      const auto &call = kind.data.call;
      for (size_t i = 0; i < call.args.len; ++i) {
        auto arg_val = reinterpret_cast<koopa_raw_value_t>(call.args.buffer[i]);
        if (i < 8) {
          MoveValueToRegister(arg_val, "a" + to_string(i));
        } else {
          MoveValueToRegister(arg_val, "t0");
          EmitSPRelativeAccess("sw", "t0", (int)(i - 8) * 4, "t1");
        }
      }
      ofs << "  call " << call.callee->name + 1 << endl;
      if (value->ty->tag != KOOPA_RTT_UNIT) {
        MoveValueFromRegister(value, "a0");
      }
      break;
    }
    case KOOPA_RVT_FUNC_ARG_REF: {
      int index = value->kind.data.func_arg_ref.index;
      ValueInfo info;
      if (index < 8) {
        info.type = ValueType::REGISTER;
        info.reg = "a" + to_string(index);
      } else {
        info.type = ValueType::STACK;
        info.offset = stack_size + (index - 8) * 4;
      }
      value_info_map[value] = info;
      break;
    }
    case KOOPA_RVT_GLOBAL_ALLOC: {
      ofs << "  .data" << endl;
      ofs << "  .globl " << value->name + 1 << endl;
      ofs << value->name + 1 << ":" << endl;
      ValueInfo info;
      info.type = ValueType::GLOBAL;
      value_info_map[value] = info;
      const auto &global_alloc = value->kind.data.global_alloc;
      const auto &init = global_alloc.init;
      if (init->kind.tag == KOOPA_RVT_ZERO_INIT) {
        ofs << "  .zero " << get_array_size(value->ty) << endl;
      } else if (init->kind.tag == KOOPA_RVT_AGGREGATE) {
        const auto &aggregate = init->kind.data.aggregate;
        for (size_t i = 0; i < aggregate.elems.len; ++i) {
          const auto &elem = reinterpret_cast<koopa_raw_value_t>(aggregate.elems.buffer[i]);
          ofs << "  .word " << elem->kind.data.integer.value << endl;
        }
      } else {
        ofs << "  .word " << init->kind.data.integer.value << endl;
      }
      ofs << endl;
      break;
    }
    case KOOPA_RVT_ZERO_INIT:
      break;
    case KOOPA_RVT_GET_ELEM_PTR: {
      const auto &get_elem_ptr = kind.data.get_elem_ptr;
      const auto &src = get_elem_ptr.src;
      if (value_info_map.at(src).type == ValueType::GLOBAL) {
        ofs << "  la t0, " << src->name + 1 << endl;
      } else {
        int offset = value_info_map.at(src).offset;
        if (offset >= -2048 && offset <= 2047) {
            ofs << "  addi t0, sp, " << offset << endl;
        } else {
            ofs << "  li t1, " << offset << endl;
            ofs << "  add t0, sp, t1" << endl;
        }
      }
      MoveValueToRegister(get_elem_ptr.index, "t1");
      ofs << "  li t2, 4" << endl;
      ofs << "  mul t1, t1, t2" << endl;
      ofs << "  add t0, t0, t1" << endl;
      EmitSPRelativeAccess("sw", "t0", value_info_map.at(value).offset, "t1");
      break;
    }
    case KOOPA_RVT_GET_PTR: {
      const auto &get_ptr = kind.data.get_ptr;
      const auto &src = get_ptr.src;
      if (value_info_map.at(src).type == ValueType::GLOBAL) {
        ofs << "  la t0, " << src->name + 1 << endl;
      } else {
        MoveValueToRegister(src, "t0");
      }
      MoveValueToRegister(get_ptr.index, "t1");
      ofs << "  li t2, 4" << endl;
      ofs << "  mul t1, t1, t2" << endl;
      ofs << "  add t0, t0, t1" << endl;
      EmitSPRelativeAccess("sw", "t0", value_info_map.at(value).offset, "t1");
      break;
    }
    default:
      assert(false);
  }
}

static void LoadValueToRegister(const koopa_raw_value_t &val, const string &reg) {
  if (val->kind.tag == KOOPA_RVT_INTEGER) {
    ofs << "  li " << reg << ", " << val->kind.data.integer.value << endl;
  } else if (val->kind.tag == KOOPA_RVT_FUNC_ARG_REF) {
    if (value_info_map.find(val) == value_info_map.end()) {
        Visit(val);
    }
    auto info = value_info_map.at(val);
    if (info.type == ValueType::REGISTER) {
      ofs << "  mv " << reg << ", " << info.reg << endl;
    } else {
      EmitSPRelativeAccess("lw", reg, info.offset, "t2");
    }
  } else if (val->kind.tag == KOOPA_RVT_GLOBAL_ALLOC){
    ofs << "  la " << reg << ", " << val->name + 1 << endl;
    ofs << "  lw " << reg << ", 0(" << reg << ")" << endl;
  } else if (val->kind.tag == KOOPA_RVT_GET_ELEM_PTR) {
    EmitSPRelativeAccess("lw", reg, value_info_map.at(val).offset, "t2");
    ofs << "  lw " << reg << ", 0(" << reg << ")" << endl;
  } else if (val->kind.tag == KOOPA_RVT_GET_PTR){
    EmitSPRelativeAccess("lw", reg, value_info_map.at(val).offset, "t2");
    ofs << "  lw " << reg << ", 0(" << reg << ")" << endl;
  } else {
    EmitSPRelativeAccess("lw", reg, value_info_map.at(val).offset, "t2");
  }
}

static void SaveValueFromRegister(const koopa_raw_value_t &val, const string &reg, const string &tmp) {
  if (val->kind.tag == KOOPA_RVT_GLOBAL_ALLOC){
    ofs << "  la " << tmp << ", " << val->name + 1 << endl;
    ofs << "  sw " << reg << ", 0(" << tmp << ")" << endl;
  } else if (val->kind.tag == KOOPA_RVT_GET_ELEM_PTR) {
    EmitSPRelativeAccess("lw", tmp, value_info_map.at(val).offset, "t2");
    ofs << "  sw " << reg << ", 0(" << tmp << ")" << endl;
  } else if (val->kind.tag == KOOPA_RVT_GET_PTR) {
    EmitSPRelativeAccess("lw", tmp, value_info_map.at(val).offset, "t2");
    ofs << "  sw " << reg << ", 0(" << tmp << ")" << endl;
  } else {
    EmitSPRelativeAccess("sw", reg, value_info_map.at(val).offset, tmp);
  }
}

static void MoveValueToRegister(const koopa_raw_value_t &val, const string &reg) {
  if (val->kind.tag == KOOPA_RVT_INTEGER) {
    ofs << "  li " << reg << ", " << val->kind.data.integer.value << endl;
  } else if (val->kind.tag == KOOPA_RVT_FUNC_ARG_REF) {
    if (value_info_map.find(val) == value_info_map.end()) {
        Visit(val);
    }
    auto info = value_info_map.at(val);
    if (info.type == ValueType::REGISTER) {
      ofs << "  mv " << reg << ", " << info.reg << endl;
    } else {
      EmitSPRelativeAccess("lw", reg, info.offset, "t2");
    }
  } else {
    EmitSPRelativeAccess("lw", reg, value_info_map.at(val).offset, "t2");
  }
}

static void MoveValueFromRegister(const koopa_raw_value_t &val, const string &reg) {
  if (val->kind.tag == KOOPA_RVT_GLOBAL_ALLOC){
    ofs << "  la " << reg << ", " << val->name + 1 << endl;
  } else {
    EmitSPRelativeAccess("sw", reg, value_info_map.at(val).offset, "t2");
  }
}

int main(int argc, const char *argv[]) {
  assert(argc == 5);
  auto mode = argv[1];
  auto input = argv[2];
  auto output = argv[4];

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

    koopa_program_t program;
    koopa_error_code_t parse_ret = koopa_parse_from_string(ss.str().c_str(), &program);
    assert(parse_ret == KOOPA_EC_SUCCESS);
    koopa_raw_program_builder_t builder = koopa_new_raw_program_builder();
    koopa_raw_program_t raw = koopa_build_raw_program(builder, program);
    koopa_delete_program(program);

    Visit(raw);

    koopa_delete_raw_program_builder(builder);
  }
  ofs.close();
  return 0;
}
