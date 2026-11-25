#pragma once
#include <string>
#include <vector>

class Expression {
public:
    enum Op { PUSH_CONST, PUSH_VAR, ADD, SUB, MUL, DIV, POW, NEG, FUNC1, FUNC2 };
    struct Instr {
        Op op;
        double value;
        int index;
    };

    bool Compile(const std::string& text, const std::vector<std::string>& varNames, std::string& errorMessage);

    double Eval(const double* vars) const;
    bool IsCompiled() const { return !code_.empty(); }

private:
    std::vector<Instr> code_;
};