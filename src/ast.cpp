#include "ast.h"

#include <string>
#include <unordered_map>
#include <stdexcept>
#include <variant>
#include <cassert>
#include <vector>
#include <numeric>
#include <functional>

int next_reg = 0, if_stmt_count = 0, lor_stmt_count = 0, land_stmt_count = 0, while_stmt_count = 0;

struct IRResult {
    std::string value;
    bool is_terminated = false;
};

typedef enum {
    VAR_SYMBOL,
    FUNC_SYMBOL
} SymbolKind;

struct SymbolInfo {
    std::string name;
    std::string unique_name;
    int value;
    bool is_const;
    SymbolKind kind;
    std::string type;
    std::vector<int> dimensions;
};

struct SymbolTable {
    std::unordered_map<std::string, SymbolInfo> var_table;
};

struct LoopContext {
    std::string break_label;
    std::string continue_label;
};

class SymbolTableManager {
public:
    SymbolTableManager() {
        enter_scope();
    }

    ~SymbolTableManager() {
        while (!table_stack.empty()) {
            exit_scope();
        }
    }

    void enter_scope() {
        table_stack.emplace_back(std::make_unique<SymbolTable>());
        assert(!table_stack.empty());
    }

    void exit_scope() {
        if (table_stack.empty()) {
            throw std::runtime_error("No symbol table to exit");
        }
        table_stack.pop_back();
    }

    bool add_symbol(SymbolInfo& symbol) {
        if (table_stack.empty()) {
            throw std::runtime_error("No symbol table to add symbol");
        }
        SymbolTable* current_scope = table_stack.back().get();
        if (current_scope->var_table.count(symbol.name)) {
            return false;
        }
        if (is_global_scope()) {
            symbol.unique_name = symbol.name;
        } else {
            SymbolTable* global_scope = table_stack.front().get();
            std::string candidate_name;
            do {
                int count = symbol_counter[symbol.name]++;
                candidate_name = symbol.name + "_" + std::to_string(count);
            } while (global_scope->var_table.count(candidate_name));
            symbol.unique_name = candidate_name;
        }
        current_scope->var_table[symbol.name] = symbol;
        return true;
    }

    SymbolInfo* lookup_symbol(const std::string& name) {
        for (auto it = table_stack.rbegin(); it != table_stack.rend(); ++it) {
            SymbolTable* current_scope = it->get();
            if (current_scope->var_table.count(name)) {
                return &current_scope->var_table[name];
            }
        }
        return nullptr;
    }

    void enter_loop(const std::string& entry_label, const std::string& exit_label) {
        loop_stack.push_back({exit_label, entry_label});
    }

    void exit_loop() {
        if (loop_stack.empty()) {
            throw std::runtime_error("No loop to exit");
        }
        loop_stack.pop_back();
    }

    std::string get_break_label() const {
        if (loop_stack.empty()) {
            throw std::runtime_error("No loop to get break label");
        }
        return loop_stack.back().break_label;
    }

    std::string get_continue_label() const {
        if (loop_stack.empty()) {
            throw std::runtime_error("No loop to get continue label");
        }
        return loop_stack.back().continue_label;
    }

    bool is_global_scope() const {
        return table_stack.size() == 1;
    }

    void clear_symbol_counter() {
        symbol_counter.clear();
    }

private:
    std::vector<std::unique_ptr<SymbolTable>> table_stack;
    std::unordered_map<std::string, int> symbol_counter;
    std::vector<LoopContext> loop_stack;
};

std::ostream& operator<<(std::ostream& os, const BaseAST& ast) {
    SymbolTableManager symbols;
    ast.generate_ir(os, symbols);
    return os;
}

void flatten_initializer(
    const BaseAST* init_node,
    const std::vector<int>& dims,
    std::vector<const BaseAST*>& flat_exps,
    int level,
    int& written_count
) {
    const std::vector<std::unique_ptr<BaseAST>>* inits = nullptr;
    const BaseAST* single_exp_node = nullptr;

    if (auto const_init_node = dynamic_cast<const ConstInitValAST*>(init_node)) {
        if (const_init_node->const_exp) {
            single_exp_node = const_init_node->const_exp.get();
        }
        inits = &const_init_node->const_inits;
    } else if (auto init_val_node = dynamic_cast<const InitValAST*>(init_node)) {
        if (init_val_node->exp) {
            single_exp_node = init_val_node->exp.get();
        }
        inits = &init_val_node->inits;
    }

    if (single_exp_node) {
        flat_exps.push_back(single_exp_node);
        written_count++;
        return;
    }

    if (!inits || inits->empty()) return;

    if (level >= dims.size()) {
        throw std::runtime_error("Excessive nesting in initializer list");
    }

    long long sub_array_size = 1;
    for (size_t i = level; i < dims.size(); ++i) {
        sub_array_size *= dims[i];
    }
    long long stride = sub_array_size / dims[level];

    if (written_count % stride != 0) {
         throw std::runtime_error("Initializer list not aligned with array dimension boundaries.");
    }

    for (const auto& item : *inits) {
        flatten_initializer(item.get(), dims, flat_exps, level + 1, written_count);
    }

    long long remainder = written_count % stride;
    if (remainder != 0) {
        int padding = stride - remainder;
        for (int i = 0; i < padding; ++i) {
            flat_exps.push_back(nullptr);
        }
        written_count += padding;
    }
}

IRResult CompUnitAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    os << R"(decl @getint(): i32
decl @getch(): i32
decl @getarray(*i32): i32
decl @putint(i32)
decl @putch(i32)
decl @putarray(i32, *i32)
decl @starttime()
decl @stoptime()

)";
    for (const auto& [name, type] : {
        std::pair{"getint", "i32"},
        {"getch", "i32"},
        {"getarray", "i32"},
        {"putint", "void"},
        {"putch", "void"},
        {"putarray", "void"},
        {"starttime", "void"},
        {"stoptime", "void"}
    }) {
        SymbolInfo symbol = {name, "", 0, false, FUNC_SYMBOL, type};
        symbols.add_symbol(symbol);
    }

    for (const auto& item : items) {
        item->generate_ir(os, symbols);
    }
    return {"", true};
}

IRResult FuncDefAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    next_reg = 0;
    if_stmt_count = 0;
    lor_stmt_count = 0;
    land_stmt_count = 0;
    while_stmt_count = 0;
    symbols.clear_symbol_counter();

    auto func_type_str = dynamic_cast<BTypeAST*>(func_type.get())->type;
    SymbolInfo symbol = {ident, "", 0, false, FUNC_SYMBOL, func_type_str};
    symbols.add_symbol(symbol);
    os << "fun @" << symbol.unique_name << "(";
    symbols.enter_scope();
    std::vector<std::tuple<std::string, std::string>> param_types;
    for (size_t i = 0; i < func_f_params.size(); ++i) {
        auto param = dynamic_cast<FuncFParamAST*>(func_f_params[i].get());
        std::string type;
        if (param->is_array) {
            type = "*i32";
        } else {
            type = "i32";
        }
        
        std::vector<int> dims;
        if (param->is_array) {
            dims.push_back(0);
            for (size_t j = 0; j < param->array_size_exps.size(); ++j) {
                dims.push_back(param->array_size_exps[j]->evaluate_const(symbols));
            }
        }
        SymbolInfo param_symbol = {param->ident, "", 0, false, VAR_SYMBOL, type, dims};
        symbols.add_symbol(param_symbol);
        std::tuple<std::string, std::string> param_type = {param_symbol.unique_name, type};
        param_types.push_back(param_type);
        os << "%" << param_symbol.unique_name << ": ";
        if (param->is_array) {
            os << "*";
        }
        param->b_type->generate_ir(os, symbols);
        if (i < func_f_params.size() - 1) {
            os << ", ";
        }
    }
    os << ")";
    if (func_type_str != "void") {
        os << ": ";
        func_type->generate_ir(os, symbols);
    }
    os << " {" << std::endl;
    os << "%entry:" << std::endl;

    for (const auto& param_type : param_types) {
        os << "  @" << std::get<0>(param_type) << " = alloc " << std::get<1>(param_type) << std::endl;
        os << "  store %" << std::get<0>(param_type) << ", @" << std::get<0>(param_type) << std::endl;
    }

    auto block_res = block->generate_ir(os, symbols);
    if (!block_res.is_terminated) {
        if (dynamic_cast<BTypeAST*>(func_type.get())->type != "void") {
            os << "  ret 0" << std::endl;
        } else {
            os << "  ret" << std::endl;
        }
    }
    os << "}" << std::endl << std::endl;
    symbols.exit_scope();
    return {"", true};
}

IRResult FuncFParamAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    return {};
}

IRResult BlockAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    symbols.enter_scope();
    bool terminated = false;
    for (const auto& block_item : block_items) {
        auto res = block_item->generate_ir(os, symbols);
        if (res.is_terminated) {
            terminated = true;
            break; 
        }
    }
    symbols.exit_scope();
    return {"", terminated};
}

IRResult StmtAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    switch (type) {
        case StmtType::RETURN_STMT: {
            if (exp) {
                auto ret_val = exp->generate_ir(os, symbols);
                os << "  ret " << ret_val.value << std::endl;
            } else {
                os << "  ret" << std::endl;
            }
            return {"", true};
        }
        case StmtType::BLOCK_STMT:
            return block->generate_ir(os, symbols);
        case StmtType::ASSIGN_STMT: {
            auto store_val = exp->generate_ir(os, symbols);
            if (auto lval_ast = dynamic_cast<LValAST*>(lval.get())) {
                auto symbol = symbols.lookup_symbol(lval_ast->ident);
                if (symbol) {
                     if (!lval_ast->array_index_exps.empty()) {
                        std::string final_offset_reg = "0";
                        if (!symbol->dimensions.empty()) {
                             if (symbol->dimensions.size() != lval_ast->array_index_exps.size()) {
                                throw std::runtime_error("Incorrect number of dimensions for array " + lval_ast->ident);
                            }

                            std::string arr = "@" + symbol->unique_name;
                            bool is_array_param = symbol->dimensions.at(0) == 0;
                            if (is_array_param) {
                                std::string load_reg = "%" + std::to_string(next_reg++);
                                os << "  " << load_reg << " = load " << arr << std::endl;
                                arr = load_reg;
                            }
                            std::string running_offset_reg = "%" + std::to_string(next_reg++);
                            os << "  " << running_offset_reg << " = add 0, 0" << std::endl;

                            for (size_t i = 0; i < symbol->dimensions.size(); ++i) {
                                long long stride = 1;
                                for (size_t j = i + 1; j < symbol->dimensions.size(); ++j) {
                                    stride *= symbol->dimensions[j];
                                }
                                auto index_val = lval_ast->array_index_exps[i]->generate_ir(os, symbols);
                                std::string term_reg = "%" + std::to_string(next_reg++);
                                os << "  " << term_reg << " = mul " << index_val.value << ", " << stride << std::endl;
                                std::string next_offset_reg = "%" + std::to_string(next_reg++);
                                os << "  " << next_offset_reg << " = add " << running_offset_reg << ", " << term_reg << std::endl;
                                running_offset_reg = next_offset_reg;
                            }
                            final_offset_reg = running_offset_reg;
                            std::string ptr_reg = "%" + std::to_string(next_reg++);
                            os << "  " << ptr_reg << " = " << (is_array_param ? "getptr " : "getelemptr ") << arr << ", " << final_offset_reg << std::endl;
                            os << "  store " << store_val.value << ", " << ptr_reg << std::endl;
                        }
                    } else {
                        os << "  store " << store_val.value << ", @" << symbol->unique_name << std::endl;
                    }
                }
            }
            break;
        }
        case StmtType::EXPRESSION_STMT:
            if(exp) exp->generate_ir(os, symbols);
            break;
        case StmtType::EMPTY_STMT:
            break;
        case StmtType::IF_STMT: {
            int current_if_id = if_stmt_count++;
            std::string then_label = "then_" + std::to_string(current_if_id);
            std::string else_label = "else_" + std::to_string(current_if_id);
            std::string endif_label = "if_end_" + std::to_string(current_if_id);
            auto cond_val = cond_exp->generate_ir(os, symbols);
            os << "  br " << cond_val.value << ", %" << then_label << ", %" << (else_stmt ? else_label : endif_label) << std::endl;
            os << "%" << then_label << ":" << std::endl;
            IRResult if_res = if_stmt->generate_ir(os, symbols);
            if (!if_res.is_terminated) {
                os << "  jump %" << endif_label << std::endl;
            }
            IRResult else_res;
            if (else_stmt) {
                os << "%" << else_label << ":" << std::endl;
                else_res = else_stmt->generate_ir(os, symbols);
                if (!else_res.is_terminated) {
                    os << "  jump %" << endif_label << std::endl;
                }
            }
            bool terminated = if_res.is_terminated && (else_stmt ? else_res.is_terminated : false);
            if (!terminated) {
                os << "%" << endif_label << ":" << std::endl;
            }
            return {"", terminated};
        }
        case StmtType::WHILE_STMT: {
            int current_while_id = while_stmt_count++;
            std::string entry_label = "while_entry_" + std::to_string(current_while_id);
            std::string body_label = "while_body_" + std::to_string(current_while_id);
            std::string endwhile_label = "while_end_" + std::to_string(current_while_id);
            os << "  jump %" << entry_label << std::endl;
            os << "%" << entry_label << ":" << std::endl;
            auto cond_val = cond_exp->generate_ir(os, symbols);
            os << "  br " << cond_val.value << ", %" << body_label << ", %" << endwhile_label << std::endl;
            os << "%" << body_label << ":" << std::endl;
            symbols.enter_loop(entry_label, endwhile_label);
            auto body_res = while_stmt->generate_ir(os, symbols);
            symbols.exit_loop();
            if (!body_res.is_terminated) {
                os << "  jump %" << entry_label << std::endl;
            }
            os << "%" << endwhile_label << ":" << std::endl;
            return {"", false};
        }
        case StmtType::BREAK_STMT:
            os << "  jump %" << symbols.get_break_label() << std::endl;
            return {"", true};
        case StmtType::CONTINUE_STMT:
            os << "  jump %" << symbols.get_continue_label() << std::endl;
            return {"", true};
        default:
            assert(false);
    }
    return {};
}

IRResult ExpAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    return lor_exp->generate_ir(os, symbols);
}

IRResult PrimaryExpAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    if (exp) {
        return exp->generate_ir(os, symbols);
    } else if (lval) {
        return lval->generate_ir(os, symbols);
    } else {
        return {std::to_string(number), false};
    }
}

IRResult UnaryExpAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    if (primary_exp) {
        return primary_exp->generate_ir(os, symbols);
    } else if (unary_exp) { 
        auto operand = unary_exp->generate_ir(os, symbols);
        std::string result_reg = "%" + std::to_string(next_reg++);
        if (unary_op == "+") {
            os << "  " << result_reg << " = add 0, " << operand.value << std::endl;
        } else if (unary_op == "-") {
            os << "  " << result_reg << " = sub 0, " << operand.value << std::endl;
        } else if (unary_op == "!") {
            os << "  " << result_reg << " = eq 0, " << operand.value << std::endl;
        }
        return {result_reg, false};
    } else {
        std::vector<std::string> param_values;
        for (auto& param : func_r_params) {
            auto param_res = param->generate_ir(os, symbols);
            param_values.push_back(param_res.value);
        }
        SymbolInfo* func_symbol = symbols.lookup_symbol(ident);
        if (!func_symbol) {
            throw std::runtime_error("Undefined function: " + ident);
        }
        if (func_symbol->kind != FUNC_SYMBOL) {
             throw std::runtime_error(ident + " is not a function");
        }

        std::string call_instruction = "call @" + func_symbol->unique_name + "(";
        for (size_t i = 0; i < param_values.size(); ++i) {
            call_instruction += param_values[i];
            if (i < param_values.size() - 1) {
                call_instruction += ", ";
            }
        }
        call_instruction += ")";

        if (func_symbol->type != "void") {
            std::string result_reg = "%" + std::to_string(next_reg++);
            os << "  " << result_reg << " = " << call_instruction << std::endl;
            return {result_reg, false};
        } else {
            os << "  " << call_instruction << std::endl;
            return {"", false};
        }
    }
}

IRResult AddExpAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    if (add_exp) {
        auto lhs = add_exp->generate_ir(os, symbols);
        auto rhs = mul_exp->generate_ir(os, symbols);
        std::string result_reg = "%" + std::to_string(next_reg++);
        if (add_op == "+") {
            os << "  " << result_reg << " = add " << lhs.value << ", " << rhs.value << std::endl;
        } else if (add_op == "-") {
            os << "  " << result_reg << " = sub " << lhs.value << ", " << rhs.value << std::endl;
        }
        return {result_reg, false};
    } else {
        return mul_exp->generate_ir(os, symbols);
    }
}

IRResult MulExpAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    if (mul_exp) {
        auto lhs = mul_exp->generate_ir(os, symbols);
        auto rhs = unary_exp->generate_ir(os, symbols);
        std::string result_reg = "%" + std::to_string(next_reg++);
        if (mul_op == "*") {
            os << "  " << result_reg << " = mul " << lhs.value << ", " << rhs.value << std::endl;
        } else if (mul_op == "/") {
            os << "  " << result_reg << " = div " << lhs.value << ", " << rhs.value << std::endl;
        } else if (mul_op == "%") {
            os << "  " << result_reg << " = mod " << lhs.value << ", " << rhs.value << std::endl;
        }
        return {result_reg, false};
    } else {
        return unary_exp->generate_ir(os, symbols);
    }
}

IRResult LOrExpAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    if (lor_exp) {
        int current_id = lor_stmt_count++;
        std::string eval_rhs_label = "%lor_eval_rhs_" + std::to_string(current_id);
        std::string end_label = "%lor_end_" + std::to_string(current_id);
        
        std::string result_ptr = "@lor_res_" + std::to_string(current_id);
        os << "  " << result_ptr << " = alloc i32" << std::endl;

        auto lhs_res = lor_exp->generate_ir(os, symbols);
        std::string lhs_bool = "%" + std::to_string(next_reg++);
        os << "  " << lhs_bool << " = ne 0, " << lhs_res.value << std::endl;
        os << "  store " << lhs_bool << ", " << result_ptr << std::endl;
        os << "  br " << lhs_bool << ", " << end_label << ", " << eval_rhs_label << std::endl;

        os << eval_rhs_label << ":" << std::endl;
        auto rhs_res = land_exp->generate_ir(os, symbols);
        std::string rhs_bool = "%" + std::to_string(next_reg++);
        os << "  " << rhs_bool << " = ne 0, " << rhs_res.value << std::endl;
        os << "  store " << rhs_bool << ", " << result_ptr << std::endl;
        os << "  jump " << end_label << std::endl;

        os << end_label << ":" << std::endl;
        std::string final_result_reg = "%" + std::to_string(next_reg++);
        os << "  " << final_result_reg << " = load " << result_ptr << std::endl;

        return {final_result_reg, false};

    } else {
        return land_exp->generate_ir(os, symbols);
    }
}

IRResult LAndExpAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    if (land_exp) {
        int current_id = land_stmt_count++;
        std::string eval_rhs_label = "%land_eval_rhs_" + std::to_string(current_id);
        std::string end_label = "%land_end_" + std::to_string(current_id);

        std::string result_ptr = "@land_res_" + std::to_string(current_id);
        os << "  " << result_ptr << " = alloc i32" << std::endl;

        auto lhs_res = land_exp->generate_ir(os, symbols);
        std::string lhs_bool = "%" + std::to_string(next_reg++);
        os << "  " << lhs_bool << " = ne 0, " << lhs_res.value << std::endl;
        os << "  store " << lhs_bool << ", " << result_ptr << std::endl;
        os << "  br " << lhs_bool << ", " << eval_rhs_label << ", " << end_label << std::endl;

        os << eval_rhs_label << ":" << std::endl;
        auto rhs_res = eq_exp->generate_ir(os, symbols);
        std::string rhs_bool = "%" + std::to_string(next_reg++);
        os << "  " << rhs_bool << " = ne 0, " << rhs_res.value << std::endl;
        os << "  store " << rhs_bool << ", " << result_ptr << std::endl;
        os << "  jump " << end_label << std::endl;

        os << end_label << ":" << std::endl;
        std::string final_result_reg = "%" + std::to_string(next_reg++);
        os << "  " << final_result_reg << " = load " << result_ptr << std::endl;

        return {final_result_reg, false};
    } else {
        return eq_exp->generate_ir(os, symbols);
    }
}

IRResult EqExpAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    if (eq_exp) {
        auto lhs = eq_exp->generate_ir(os, symbols);
        auto rhs = rel_exp->generate_ir(os, symbols);
        std::string result_reg = "%" + std::to_string(next_reg++);
        if (eq_op == "==") {
            os << "  " << result_reg << " = eq " << lhs.value << ", " << rhs.value << std::endl;
        } else if (eq_op == "!=") {
            os << "  " << result_reg << " = ne " << lhs.value << ", " << rhs.value << std::endl;
        }
        return {result_reg, false};
    } else {
        return rel_exp->generate_ir(os, symbols);
    }
}

IRResult RelExpAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    if (rel_exp) {
        auto lhs = rel_exp->generate_ir(os, symbols);
        auto rhs = add_exp->generate_ir(os, symbols);
        std::string result_reg = "%" + std::to_string(next_reg++);
        if (rel_op == "<") {
            os << "  " << result_reg << " = lt " << lhs.value << ", " << rhs.value << std::endl;
        } else if (rel_op == ">") {
            os << "  " << result_reg << " = gt " << lhs.value << ", " << rhs.value << std::endl;
        } else if (rel_op == "<=") {
            os << "  " << result_reg << " = le " << lhs.value << ", " << rhs.value << std::endl;
        } else if (rel_op == ">=") {
            os << "  " << result_reg << " = ge " << lhs.value << ", " << rhs.value << std::endl;
        }
        return {result_reg, false};
    } else {
        return add_exp->generate_ir(os, symbols);
    }
}

IRResult DeclAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    if (const_decl) {
        return const_decl->generate_ir(os, symbols);
    } else if (var_decl) {
        return var_decl->generate_ir(os, symbols);
    }
    return {};
}

IRResult ConstDeclAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    for (const auto& const_def : const_defs) {
        const_def->generate_ir(os, symbols);
    }
    return {};
}

IRResult ConstDefAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    if (!array_size_exps.empty()) {
        SymbolInfo symbol = {ident, "", 0, true, VAR_SYMBOL, "*i32"};
        
        long long total_size = 1;
        for (const auto& exp : array_size_exps) {
            int dim_size = exp->evaluate_const(symbols);
            symbol.dimensions.push_back(dim_size);
            total_size *= dim_size;
        }
        symbols.add_symbol(symbol);
        
        std::vector<const BaseAST*> flat_inits;
        if (const_init_val) {
            int written_count = 0;
            flatten_initializer(const_init_val.get(), symbol.dimensions, flat_inits, 0, written_count);
        }

        if (symbols.is_global_scope()) {
            os << "global @" << symbol.unique_name << " = alloc [i32, " << total_size << "]" << ", ";
            if (const_init_val) {
                os << "{";
                for (int i = 0; i < total_size; i++) {
                    if (i < flat_inits.size() && flat_inits[i]) {
                        os << flat_inits[i]->evaluate_const(symbols);
                    } else {
                        os << "0";
                    }
                    if (i < total_size - 1) os << ", ";
                }
                os << "}";
            } else {
                os << "zeroinit";
            }
            os << std::endl << std::endl;
        } else {
            os << "  @" << symbol.unique_name << " = alloc [i32, " << total_size << "]" << std::endl;
            if (const_init_val) {
                for (int i = 0; i < total_size; ++i) {
                    std::string result_reg = "%" + std::to_string(next_reg++);
                    os << "  " << result_reg << " = getelemptr @" << symbol.unique_name << ", " << i << std::endl;
                    if (i < flat_inits.size() && flat_inits[i]) {
                        auto init_val_exp = flat_inits[i]->generate_ir(os, symbols);
                        os << "  store " << init_val_exp.value << ", " << result_reg << std::endl;
                    } else {
                        os << "  store 0, " << result_reg << std::endl;
                    }
                }
            }
        }
        return {};
    }
    int val = const_init_val->evaluate_const(symbols);
    SymbolInfo symbol = {ident, "", val, true, VAR_SYMBOL, "i32"};
    symbols.add_symbol(symbol);
    return {};
}

IRResult ConstInitValAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    if(const_exp) return const_exp->generate_ir(os, symbols);
    return {};
}

IRResult ConstExpAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    return {std::to_string(evaluate_const(symbols)), false};
}

int LValAST::evaluate_const(SymbolTableManager& symbols) const {
    auto symbol = symbols.lookup_symbol(ident);
    if (!symbol) {
        throw std::runtime_error("Undefined variable: " + ident);
    }
    if (!array_index_exps.empty()) {
        throw std::logic_error("Cannot evaluate array element in constant expression");
    }
    return symbol->value;
}

IRResult VarDeclAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    for (const auto& var_def : var_defs) {
        var_def->generate_ir(os, symbols);
    }
    return {};
}

IRResult VarDefAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    SymbolInfo symbol = {ident, "", 0, false, VAR_SYMBOL, "i32"};

    if (!array_size_exps.empty()) {
        long long total_size = 1;
        for (const auto& exp : array_size_exps) {
            int dim_size = exp->evaluate_const(symbols);
            symbol.dimensions.push_back(dim_size);
            total_size *= dim_size;
        }
        symbols.add_symbol(symbol);

        std::vector<const BaseAST*> flat_inits;
        if (init_val) {
            int written_count = 0;
            flatten_initializer(init_val.get(), symbol.dimensions, flat_inits, 0, written_count);
        }

        if (symbols.is_global_scope()) {
            os << "global @" << symbol.unique_name << " = alloc [i32, " << total_size << "]" << ", ";
            if (init_val) {
                os << "{";
                for (int i = 0; i < total_size; i++) {
                    if (i < flat_inits.size() && flat_inits[i]) {
                        os << flat_inits[i]->evaluate_const(symbols);
                    } else {
                        os << "0";
                    }
                    if (i < total_size - 1) os << ", ";
                }
                os << "}";
            } else {
                os << "zeroinit";
            }
             os << std::endl << std::endl;
        } else {
            os << "  @" << symbol.unique_name << " = alloc [i32, " << total_size << "]" << std::endl;
            if (init_val) {
                for (int i = 0; i < total_size; ++i) {
                    std::string result_reg = "%" + std::to_string(next_reg++);
                    os << "  " << result_reg << " = getelemptr @" << symbol.unique_name << ", " << i << std::endl;
                    if (i < flat_inits.size() && flat_inits[i]) {
                        auto init_val_exp = flat_inits[i]->generate_ir(os, symbols);
                        os << "  store " << init_val_exp.value << ", " << result_reg << std::endl;
                    } else {
                        os << "  store 0, " << result_reg << std::endl;
                    }
                }
            }
        }
    } else {
        symbols.add_symbol(symbol);
        if (symbols.is_global_scope()) {
            os << "global @" << symbol.unique_name << " = alloc i32, ";
            if (init_val) {
                int val = init_val->evaluate_const(symbols);
                os << val;
            } else {
                os << "zeroinit";
            }
            os << std::endl << std::endl;
        } else {
            os << "  @" << symbol.unique_name << " = alloc i32" << std::endl;
            if (init_val) {
                auto store_val = init_val->generate_ir(os, symbols);
                os << "  store " << store_val.value << ", @" << symbol.unique_name << std::endl;
            }
        }
    }
    return {};
}


IRResult InitValAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    if(exp) return exp->generate_ir(os, symbols);
    return {};
}

int PrimaryExpAST::evaluate_const(SymbolTableManager& symbols) const {
    if (exp) {
        return exp->evaluate_const(symbols);
    } else if (lval) {
        return lval->evaluate_const(symbols);
    } else {
        return number;
    }
}

int UnaryExpAST::evaluate_const(SymbolTableManager& symbols) const {
    if (primary_exp) {
        return primary_exp->evaluate_const(symbols);
    } else if (unary_exp) {
        int val = unary_exp->evaluate_const(symbols);
        if (unary_op == "+") return val;
        if (unary_op == "-") return -val;
        if (unary_op == "!") return !val;
    } else {
        throw std::logic_error("Cannot evaluate function call in constant expression");
    }
    return 0;
}

int AddExpAST::evaluate_const(SymbolTableManager& symbols) const {
    if (add_exp) {
        int lhs = add_exp->evaluate_const(symbols);
        int rhs = mul_exp->evaluate_const(symbols);
        if (add_op == "+") return lhs + rhs;
        if (add_op == "-") return lhs - rhs;
    } else {
        return mul_exp->evaluate_const(symbols);
    }
    return 0;
}

int MulExpAST::evaluate_const(SymbolTableManager& symbols) const {
    if (mul_exp) {
        int lhs = mul_exp->evaluate_const(symbols);
        int rhs = unary_exp->evaluate_const(symbols);
        if (mul_op == "*") return lhs * rhs;
        if (mul_op == "/") return lhs / rhs;
        if (mul_op == "%") return lhs % rhs;
    } else {
        return unary_exp->evaluate_const(symbols);
    }
    return 0;
}

int RelExpAST::evaluate_const(SymbolTableManager& symbols) const {
    if (rel_exp) {
        int lhs = rel_exp->evaluate_const(symbols);
        int rhs = add_exp->evaluate_const(symbols);
        if (rel_op == "<") return lhs < rhs;
        if (rel_op == ">") return lhs > rhs;
        if (rel_op == "<=") return lhs <= rhs;
        if (rel_op == ">=") return lhs >= rhs;
    } else {
        return add_exp->evaluate_const(symbols);
    }
    return 0;
}

int EqExpAST::evaluate_const(SymbolTableManager& symbols) const {
    if (eq_exp) {
        int lhs = eq_exp->evaluate_const(symbols);
        int rhs = rel_exp->evaluate_const(symbols);
        if (eq_op == "==") return lhs == rhs;
        if (eq_op == "!=") return lhs != rhs;
    } else {
        return rel_exp->evaluate_const(symbols);
    }
    return 0;
}

int LAndExpAST::evaluate_const(SymbolTableManager& symbols) const {
    if (land_exp) {
        int lhs = land_exp->evaluate_const(symbols);
        int rhs = eq_exp->evaluate_const(symbols);
        return lhs && rhs;
    } else {
        return eq_exp->evaluate_const(symbols);
    }
}

int LOrExpAST::evaluate_const(SymbolTableManager& symbols) const {
    if (lor_exp) {
        int lhs = lor_exp->evaluate_const(symbols);
        int rhs = land_exp->evaluate_const(symbols);
        return lhs || rhs;
    } else {
        return land_exp->evaluate_const(symbols);
    }
}

int ExpAST::evaluate_const(SymbolTableManager& symbols) const {
    return lor_exp->evaluate_const(symbols);
}

int ConstInitValAST::evaluate_const(SymbolTableManager& symbols) const {
    if (const_exp) return const_exp->evaluate_const(symbols);
    throw std::logic_error("Cannot evaluate initializer list in constant expression");
}

int ConstExpAST::evaluate_const(SymbolTableManager& symbols) const {
    return exp->evaluate_const(symbols);
}

int InitValAST::evaluate_const(SymbolTableManager& symbols) const {
    if(exp) return exp->evaluate_const(symbols);
    throw std::logic_error("Cannot evaluate initializer list in constant expression");
}

IRResult BTypeAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    if (type == "int") {
        os << "i32";
    } else if (type == "void") {
        os << "void";
    } else {
        os << "unknown";
    }
    return {"", false};
}

IRResult LValAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    auto symbol = symbols.lookup_symbol(ident);
    if (!symbol) {
        throw std::runtime_error("Undefined variable: " + ident);
    }

    if (!array_index_exps.empty()) {
        if (symbol->dimensions.empty()) {
            throw std::runtime_error("Indexing non-array variable " + ident);
        }
        std::string arr = "@" + symbol->unique_name;
        bool is_array_param = symbol->dimensions.at(0) == 0;
        if (is_array_param) {
            std::string load_reg = "%" + std::to_string(next_reg++);
            os << "  " << load_reg << " = load " << arr << std::endl;
            arr = load_reg;
        }

        std::string running_offset_reg = "%" + std::to_string(next_reg++);
        os << "  " << running_offset_reg << " = add 0, 0" << std::endl;

        for (size_t i = 0; i < array_index_exps.size(); ++i) {
            long long stride = 1;
            for (size_t j = i + 1; j < symbol->dimensions.size(); ++j) {
                stride *= symbol->dimensions[j];
            }
            auto index_val = array_index_exps[i]->generate_ir(os, symbols);
            
            std::string term_reg;
            if (stride > 1) {
                term_reg = "%" + std::to_string(next_reg++);
                os << "  " << term_reg << " = mul " << index_val.value << ", " << stride << std::endl;
            } else {
                term_reg = index_val.value;
            }

            std::string next_offset_reg = "%" + std::to_string(next_reg++);
            os << "  " << next_offset_reg << " = add " << running_offset_reg << ", " << term_reg << std::endl;
            running_offset_reg = next_offset_reg;
        }

        std::string ptr_reg = "%" + std::to_string(next_reg++);
        os << "  " << ptr_reg << " = " << (is_array_param ? "getptr " : "getelemptr ") << arr << ", " << running_offset_reg << std::endl;

        if (array_index_exps.size() < symbol->dimensions.size()) {
            return {ptr_reg, false};
        } else {
            std::string result_reg = "%" + std::to_string(next_reg++);
            os << "  " << result_reg << " = load " << ptr_reg << std::endl;
            return {result_reg, false};
        }
    } else if (!symbol->dimensions.empty()) {
        bool is_array_param = symbol->dimensions.at(0) == 0;
        if (is_array_param) {
            std::string load_reg = "%" + std::to_string(next_reg++);
            os << "  " << load_reg << " = load @" + symbol->unique_name << std::endl;
            std::string ptr_reg = "%" + std::to_string(next_reg++);
            os << "  " << ptr_reg << " = getptr " << load_reg << ", 0" << std::endl;
            return {ptr_reg, false};
        } else {
            std::string ptr_reg = "%" + std::to_string(next_reg++);
            os << "  " << ptr_reg << " = getelemptr @" << symbol->unique_name << ", 0" << std::endl;
            return {ptr_reg, false};
        }
    }

    if (symbol->is_const) {
        return {std::to_string(symbol->value), false};
    }
    std::string result_reg = "%" + std::to_string(next_reg++);
    os << "  " << result_reg << " = load @" << symbol->unique_name << std::endl;
    return {result_reg, false};
}
