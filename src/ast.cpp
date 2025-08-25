#include "ast.h"

int var_count = 0;

std::ostream& operator<<(std::ostream& os, const BaseAST& ast) {
    ast.print(os);
    return os;
}

void CompUnitAST::print(std::ostream& os) const {
    os << *func_def;
}

void FuncDefAST::print(std::ostream& os) const {
    os << "fun @" << ident << "(): ";
    os << *func_type;
    os << *block;
}

void FuncTypeAST::print(std::ostream& os) const {
    if (type == "int") {
        os << "i32 ";
    } else {
        os << "unknown ";
    }
}

void BlockAST::print(std::ostream& os) const {
    os << "{" << std::endl;
    os << "%entry:" << std::endl;
    os << *stmt;
    os << "}" << std::endl;
}

void StmtAST::print(std::ostream& os) const {
    os << *exp;
    os << "  ret " << "%" << var_count - 1 << std::endl;
}

void ExpAST::print(std::ostream& os) const {
    os << *add_exp;
}

void PrimaryExpAST::print(std::ostream& os) const {
    if (exp) {
        os << *exp;
    } else {
        os << "  %" << var_count++ << " = add 0, " << number << std::endl;
    }
}

void UnaryExpAST::print(std::ostream& os) const {
    if (primary_exp) {
        os << *primary_exp;
    } else {
        os << *unary_exp;
        if (unary_op == "+") {
            // do nothing
        } else if (unary_op == "-") {
            os << "  %" << var_count << " = sub 0, %" << var_count - 1 << std::endl;
            var_count++;
        } else if (unary_op == "!") {
            os << "  %" << var_count << " = eq 0, %" << var_count - 1 << std::endl;
            var_count++;
        }
    }
}

void AddExpAST::print(std::ostream& os) const {
    if (add_exp) {
        int lhs, rhs;
        os << *add_exp;
        lhs = var_count - 1;
        os << *mul_exp;
        rhs = var_count - 1;
        if (add_op == "+") {
            os << "  %" << var_count << " = add %" << lhs << ", %" << rhs << std::endl;
            var_count++;
        } else if (add_op == "-") {
            os << "  %" << var_count << " = sub %" << lhs << ", %" << rhs << std::endl;
            var_count++;
        }
    } else {
        os << *mul_exp;
    }
}

void MulExpAST::print(std::ostream& os) const {
    if (mul_exp) {
        int lhs, rhs;
        os << *mul_exp;
        lhs = var_count - 1;
        os << *unary_exp;
        rhs = var_count - 1;
        if (mul_op == "*") {
            os << "  %" << var_count << " = mul %" << lhs << ", %" << rhs << std::endl;
            var_count++;
        } else if (mul_op == "/") {
            os << "  %" << var_count << " = div %" << lhs << ", %" << rhs << std::endl;
            var_count++;
        } else if (mul_op == "%") {
            os << "  %" << var_count << " = mod %" << lhs << ", %" << rhs << std::endl;
            var_count++;
        }
    } else {
        os << *unary_exp;
    }
}
