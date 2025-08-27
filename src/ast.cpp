#include "ast.h"

#include <string>

class SymbolTable {
public:
    int next_reg = 0;
};

std::ostream& operator<<(std::ostream& os, const BaseAST& ast) {
    SymbolTable symbols;
    ast.generate_ir(os, symbols);
    return os;
}

std::string CompUnitAST::generate_ir(std::ostream& os, SymbolTable& symbols) const {
    return func_def->generate_ir(os, symbols);
}

std::string FuncDefAST::generate_ir(std::ostream& os, SymbolTable& symbols) const {
    os << "fun @" << ident << "(): ";
    os << *func_type;
    os << *block;
    return "";
}

std::string FuncTypeAST::generate_ir(std::ostream& os, SymbolTable& symbols) const {
    if (type == "int") {
        os << "i32 ";
    } else {
        os << "unknown ";
    }
    return "";
}

std::string BlockAST::generate_ir(std::ostream& os, SymbolTable& symbols) const {
    os << "{" << std::endl;
    os << "%entry:" << std::endl;
    auto ret_val = stmt->generate_ir(os, symbols);
    os << "  ret " << ret_val << std::endl;
    os << "}" << std::endl;
    return "";
}

std::string StmtAST::generate_ir(std::ostream& os, SymbolTable& symbols) const {
    return exp->generate_ir(os, symbols);
}

std::string ExpAST::generate_ir(std::ostream& os, SymbolTable& symbols) const {
    return lor_exp->generate_ir(os, symbols);
}

std::string PrimaryExpAST::generate_ir(std::ostream& os, SymbolTable& symbols) const {
    if (exp) {
        return exp->generate_ir(os, symbols);
    } else {
        return std::to_string(number);
    }
}

std::string UnaryExpAST::generate_ir(std::ostream& os, SymbolTable& symbols) const {
    if (primary_exp) {
        return primary_exp->generate_ir(os, symbols);
    } else {
        auto operand_reg = unary_exp->generate_ir(os, symbols);
        std::string result_reg = "%" + std::to_string(symbols.next_reg++);
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

std::string AddExpAST::generate_ir(std::ostream& os, SymbolTable& symbols) const {
    if (add_exp) {
        auto lhs_reg = add_exp->generate_ir(os, symbols);
        auto rhs_reg = mul_exp->generate_ir(os, symbols);
        std::string result_reg = "%" + std::to_string(symbols.next_reg++);
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

std::string MulExpAST::generate_ir(std::ostream& os, SymbolTable& symbols) const {
    if (mul_exp) {
        auto lhs_reg = mul_exp->generate_ir(os, symbols);
        auto rhs_reg = unary_exp->generate_ir(os, symbols);
        std::string result_reg = "%" + std::to_string(symbols.next_reg++);
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

std::string LOrExpAST::generate_ir(std::ostream& os, SymbolTable& symbols) const {
    if (lor_exp) {
        auto lhs_reg = lor_exp->generate_ir(os, symbols);
        auto rhs_reg = land_exp->generate_ir(os, symbols);
        
        std::string lhs_bool = "%" + std::to_string(symbols.next_reg++);
        os << "  " << lhs_bool << " = ne 0, " << lhs_reg << std::endl;
        
        std::string rhs_bool = "%" + std::to_string(symbols.next_reg++);
        os << "  " << rhs_bool << " = ne 0, " << rhs_reg << std::endl;

        std::string result_reg = "%" + std::to_string(symbols.next_reg++);
        os << "  " << result_reg << " = or " << lhs_bool << ", " << rhs_bool << std::endl;
        return result_reg;
    } else {
        return land_exp->generate_ir(os, symbols);
    }
}

std::string LAndExpAST::generate_ir(std::ostream& os, SymbolTable& symbols) const {
    if (land_exp) {
        auto lhs_reg = land_exp->generate_ir(os, symbols);
        auto rhs_reg = eq_exp->generate_ir(os, symbols);

        std::string lhs_bool = "%" + std::to_string(symbols.next_reg++);
        os << "  " << lhs_bool << " = ne 0, " << lhs_reg << std::endl;
        
        std::string rhs_bool = "%" + std::to_string(symbols.next_reg++);
        os << "  " << rhs_bool << " = ne 0, " << rhs_reg << std::endl;

        std::string result_reg = "%" + std::to_string(symbols.next_reg++);
        os << "  " << result_reg << " = and " << lhs_bool << ", " << rhs_bool << std::endl;
        return result_reg;
    } else {
        return eq_exp->generate_ir(os, symbols);
    }
}

std::string EqExpAST::generate_ir(std::ostream& os, SymbolTable& symbols) const {
    if (eq_exp) {
        auto lhs_reg = eq_exp->generate_ir(os, symbols);
        auto rhs_reg = rel_exp->generate_ir(os, symbols);
        std::string result_reg = "%" + std::to_string(symbols.next_reg++);
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

std::string RelExpAST::generate_ir(std::ostream& os, SymbolTable& symbols) const {
    if (rel_exp) {
        auto lhs_reg = rel_exp->generate_ir(os, symbols);
        auto rhs_reg = add_exp->generate_ir(os, symbols);
        std::string result_reg = "%" + std::to_string(symbols.next_reg++);
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