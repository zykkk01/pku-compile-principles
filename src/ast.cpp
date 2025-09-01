#include "ast.h"

#include <string>
#include <unordered_map>
#include <stdexcept>
#include <variant>
#include <cassert>

int next_reg = 0;

struct SymbolInfo {
    std::string name;
    std::string unique_name;
    int value;
    bool is_const;
};

class SymbolTable {
public:
    std::unordered_map<std::string, SymbolInfo> var_table;
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
        symbol.unique_name = symbol.name + "_" + std::to_string(++symbol_counter[symbol.name]);
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

private:
    std::vector<std::unique_ptr<SymbolTable>> table_stack;
    std::unordered_map<std::string, int> symbol_counter;
};

std::ostream& operator<<(std::ostream& os, const BaseAST& ast) {
    SymbolTableManager symbols;
    ast.generate_ir(os, symbols);
    return os;
}

std::string CompUnitAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    return func_def->generate_ir(os, symbols);
}

std::string FuncDefAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    os << "fun @" << ident << "(): ";
    func_type->generate_ir(os, symbols);
    os << "{" << std::endl;
    os << "%entry:" << std::endl;
    block->generate_ir(os, symbols);
    os << "}" << std::endl;
    return "";
}

std::string FuncTypeAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    if (type == "int") {
        os << "i32 ";
    } else {
        os << "unknown ";
    }
    return "";
}

std::string BlockAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    symbols.enter_scope();
    for (const auto& block_item : block_items) {
        block_item->generate_ir(os, symbols);
    }
    symbols.exit_scope();
    return "";
}

std::string StmtAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    if (is_return) {
        auto ret_val = exp->generate_ir(os, symbols);
        os << "  ret " << ret_val << std::endl;
    } else if (block) {
        block->generate_ir(os, symbols);
    } else if (lval) {
        auto store_val = exp->generate_ir(os, symbols);
        if (auto lval_ast = dynamic_cast<LValAST*>(lval.get())) {
            auto symbol = symbols.lookup_symbol(lval_ast->ident);
            if (symbol) {
                 os << "  store " << store_val << ", @" << symbol->unique_name << std::endl;
            }
        }
    } else if (exp) {
        exp->generate_ir(os, symbols);
    }
    return "";
}

std::string ExpAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    return lor_exp->generate_ir(os, symbols);
}

std::string PrimaryExpAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    if (exp) {
        return exp->generate_ir(os, symbols);
    } else if (lval) {
        return lval->generate_ir(os, symbols);
    } else {
        return std::to_string(number);
    }
}

std::string UnaryExpAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    if (primary_exp) {
        return primary_exp->generate_ir(os, symbols);
    } else {
        auto operand_reg = unary_exp->generate_ir(os, symbols);
        std::string result_reg = "%" + std::to_string(next_reg++);
        if (unary_op == "+") {
            os << "  " << result_reg << " = add 0, " << operand_reg << std::endl;
        } else if (unary_op == "-") {
            os << "  " << result_reg << " = sub 0, " << operand_reg << std::endl;
        } else if (unary_op == "!") {
            os << "  " << result_reg << " = eq 0, " << operand_reg << std::endl;
        }
        return result_reg;
    }
}

std::string AddExpAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    if (add_exp) {
        auto lhs_reg = add_exp->generate_ir(os, symbols);
        auto rhs_reg = mul_exp->generate_ir(os, symbols);
        std::string result_reg = "%" + std::to_string(next_reg++);
        if (add_op == "+") {
            os << "  " << result_reg << " = add " << lhs_reg << ", " << rhs_reg << std::endl;
        } else if (add_op == "-") {
            os << "  " << result_reg << " = sub " << lhs_reg << ", " << rhs_reg << std::endl;
        }
        return result_reg;
    } else {
        return mul_exp->generate_ir(os, symbols);
    }
}

std::string MulExpAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    if (mul_exp) {
        auto lhs_reg = mul_exp->generate_ir(os, symbols);
        auto rhs_reg = unary_exp->generate_ir(os, symbols);
        std::string result_reg = "%" + std::to_string(next_reg++);
        if (mul_op == "*") {
            os << "  " << result_reg << " = mul " << lhs_reg << ", " << rhs_reg << std::endl;
        } else if (mul_op == "/") {
            os << "  " << result_reg << " = div " << lhs_reg << ", " << rhs_reg << std::endl;
        } else if (mul_op == "%") {
            os << "  " << result_reg << " = mod " << lhs_reg << ", " << rhs_reg << std::endl;
        }
        return result_reg;
    } else {
        return unary_exp->generate_ir(os, symbols);
    }
}

std::string LOrExpAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    if (lor_exp) {
        auto lhs_reg = lor_exp->generate_ir(os, symbols);
        auto rhs_reg = land_exp->generate_ir(os, symbols);
        
        std::string lhs_bool = "%" + std::to_string(next_reg++);
        os << "  " << lhs_bool << " = ne 0, " << lhs_reg << std::endl;
        
        std::string rhs_bool = "%" + std::to_string(next_reg++);
        os << "  " << rhs_bool << " = ne 0, " << rhs_reg << std::endl;

        std::string result_reg = "%" + std::to_string(next_reg++);
        os << "  " << result_reg << " = or " << lhs_bool << ", " << rhs_bool << std::endl;
        return result_reg;
    } else {
        return land_exp->generate_ir(os, symbols);
    }
}

std::string LAndExpAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    if (land_exp) {
        auto lhs_reg = land_exp->generate_ir(os, symbols);
        auto rhs_reg = eq_exp->generate_ir(os, symbols);

        std::string lhs_bool = "%" + std::to_string(next_reg++);
        os << "  " << lhs_bool << " = ne 0, " << lhs_reg << std::endl;
        
        std::string rhs_bool = "%" + std::to_string(next_reg++);
        os << "  " << rhs_bool << " = ne 0, " << rhs_reg << std::endl;

        std::string result_reg = "%" + std::to_string(next_reg++);
        os << "  " << result_reg << " = and " << lhs_bool << ", " << rhs_bool << std::endl;
        return result_reg;
    } else {
        return eq_exp->generate_ir(os, symbols);
    }
}

std::string EqExpAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    if (eq_exp) {
        auto lhs_reg = eq_exp->generate_ir(os, symbols);
        auto rhs_reg = rel_exp->generate_ir(os, symbols);
        std::string result_reg = "%" + std::to_string(next_reg++);
        if (eq_op == "==") {
            os << "  " << result_reg << " = eq " << lhs_reg << ", " << rhs_reg << std::endl;
        } else if (eq_op == "!=") {
            os << "  " << result_reg << " = ne " << lhs_reg << ", " << rhs_reg << std::endl;
        }
        return result_reg;
    } else {
        return rel_exp->generate_ir(os, symbols);
    }
}

std::string RelExpAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    if (rel_exp) {
        auto lhs_reg = rel_exp->generate_ir(os, symbols);
        auto rhs_reg = add_exp->generate_ir(os, symbols);
        std::string result_reg = "%" + std::to_string(next_reg++);
        if (rel_op == "<") {
            os << "  " << result_reg << " = lt " << lhs_reg << ", " << rhs_reg << std::endl;
        } else if (rel_op == ">") {
            os << "  " << result_reg << " = gt " << lhs_reg << ", " << rhs_reg << std::endl;
        } else if (rel_op == "<=") {
            os << "  " << result_reg << " = le " << lhs_reg << ", " << rhs_reg << std::endl;
        } else if (rel_op == ">=") {
            os << "  " << result_reg << " = ge " << lhs_reg << ", " << rhs_reg << std::endl;
        }
        return result_reg;
    } else {
        return add_exp->generate_ir(os, symbols);
    }
}

std::string DeclAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    if (const_decl) {
        return const_decl->generate_ir(os, symbols);
    } else if (var_decl) {
        return var_decl->generate_ir(os, symbols);
    }
    return "";
}

std::string ConstDeclAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    for (const auto& const_def : const_defs) {
        const_def->generate_ir(os, symbols);
    }
    return "";
}

std::string ConstDefAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    int val = const_init_val->evaluate_const(symbols);
    SymbolInfo symbol = {ident, "", val, true};
    symbols.add_symbol(symbol);
    return "";
}

std::string ConstInitValAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    return "";
}

std::string ConstExpAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    return std::to_string(evaluate_const(symbols));
}

int LValAST::evaluate_const(SymbolTableManager& symbols) const {
    auto symbol = symbols.lookup_symbol(ident);
    if (!symbol) {
        throw std::runtime_error("Undefined variable: " + ident);
    }
    return symbol->value;
}

std::string VarDeclAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    for (const auto& var_def : var_defs) {
        var_def->generate_ir(os, symbols);
    }
    return "";
}

std::string VarDefAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    SymbolInfo symbol = {ident, "", 0, false};
    symbols.add_symbol(symbol);
    auto new_symbol = symbols.lookup_symbol(ident);
    os << "  @" << new_symbol->unique_name << " = alloc i32" << std::endl;
    if (init_val) {
        auto store_val = init_val->generate_ir(os, symbols);
        os << "  store " << store_val << ", @" << new_symbol->unique_name << std::endl;
    }
    return "";
}

std::string InitValAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    return exp->generate_ir(os, symbols);
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
    } else {
        int val = unary_exp->evaluate_const(symbols);
        if (unary_op == "+") return val;
        if (unary_op == "-") return -val;
        if (unary_op == "!") return !val;
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
    if (lor_exp) {
        return lor_exp->evaluate_const(symbols);
    }
    return 0;
}

int ConstInitValAST::evaluate_const(SymbolTableManager& symbols) const {
    if (const_exp) {
        return const_exp->evaluate_const(symbols);
    }
    return 0;
}

int ConstExpAST::evaluate_const(SymbolTableManager& symbols) const {
    return exp->evaluate_const(symbols);
}

std::string BTypeAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    return "";
}

std::string LValAST::generate_ir(std::ostream& os, SymbolTableManager& symbols) const {
    auto symbol = symbols.lookup_symbol(ident);
    if (!symbol) {
        throw std::runtime_error("Undefined variable: " + ident);
    }
    if (symbol->is_const) {
        return std::to_string(symbol->value);
    }

    std::string result_reg = "%" + std::to_string(next_reg++);
    os << "  " << result_reg << " = load @" << symbol->unique_name << std::endl;
    return result_reg;
}
