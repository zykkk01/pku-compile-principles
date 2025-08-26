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
    os << *lor_exp;
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

void LOrExpAST::print(std::ostream& os) const {
    if (lor_exp) {
        int lhs, rhs;
        os << *lor_exp;
        lhs = var_count - 1;
        os << *land_exp;
        rhs = var_count - 1;
        os << "  %" << var_count << " = ne 0, %" << lhs << std::endl;
        int lhs_bool = var_count++;
        os << "  %" << var_count << " = ne 0, %" << rhs << std::endl;
        int rhs_bool = var_count++;
        os << "  %" << var_count << " = or %" << lhs_bool << ", %" << rhs_bool << std::endl; 
        var_count++;
    } else {
        os << *land_exp;
    }
}

void LAndExpAST::print(std::ostream& os) const {
    if (land_exp) {
        int lhs, rhs;
        os << *land_exp;
        lhs = var_count - 1;
        os << *eq_exp;
        rhs = var_count - 1;
        os << "  %" << var_count << " = ne 0, %" << lhs << std::endl;
        int lhs_bool = var_count++;
        os << "  %" << var_count << " = ne 0, %" << rhs << std::endl;
        int rhs_bool = var_count++;
        os << "  %" << var_count << " = and %" << lhs_bool << ", %" << rhs_bool << std::endl; 
        var_count++;
    } else {
        os << *eq_exp;
    }
}

void EqExpAST::print(std::ostream& os) const {
    if (eq_exp) {
        int lhs, rhs;
        os << *eq_exp;
        lhs = var_count - 1;
        os << *rel_exp;
        rhs = var_count - 1;
        if (eq_op == "==") {
            os << "  %" << var_count << " = eq %" << lhs << ", %" << rhs << std::endl;
            var_count++;
        } else if (eq_op == "!=") {
            os << "  %" << var_count << " = ne %" << lhs << ", %" << rhs << std::endl;
            var_count++;
        }
    } else {
        os << *rel_exp;
    }
}

void RelExpAST::print(std::ostream& os) const {
    if (rel_exp) {
        int lhs, rhs;
        os << *rel_exp;
        lhs = var_count - 1;
        os << *add_exp;
        rhs = var_count - 1;
        if (rel_op == "<") {
            os << "  %" << var_count << " = lt %" << lhs << ", %" << rhs << std::endl;
            var_count++;
        } else if (rel_op == ">") {
            os << "  %" << var_count << " = gt %" << lhs << ", %" << rhs << std::endl;
            var_count++;
        } else if (rel_op == "<=") {
            os << "  %" << var_count << " = le %" << lhs << ", %" << rhs << std::endl;
            var_count++;
        } else if (rel_op == ">=") {
            os << "  %" << var_count << " = ge %" << lhs << ", %" << rhs << std::endl;
            var_count++;
        }
    } else {
        os << *add_exp;
    }
}