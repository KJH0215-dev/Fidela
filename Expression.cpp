#include "Expression.h"
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace {

    double fSin(double x) { return std::sin(x); }
    double fCos(double x) { return std::cos(x); }
    double fTan(double x) { return std::tan(x); }
    double fAsin(double x) { return std::asin(x); }
    double fAcos(double x) { return std::acos(x); }
    double fAtan(double x) { return std::atan(x); }
    double fSinh(double x) { return std::sinh(x); }
    double fCosh(double x) { return std::cosh(x); }
    double fTanh(double x) { return std::tanh(x); }
    double fExp(double x) { return std::exp(x); }
    double fLn(double x) { return std::log(x); }
    double fLog10(double x) { return std::log10(x); }
    double fSqrt(double x) { return std::sqrt(x); }
    double fCbrt(double x) { return std::cbrt(x); }
    double fAbs(double x) { return std::fabs(x); }
    double fSign(double x) { return (x > 0.0) ? 1.0 : ((x < 0.0) ? -1.0 : 0.0); }
    double fFloor(double x) { return std::floor(x); }
    double fCeil(double x) { return std::ceil(x); }

    double fPow(double a, double b) { return std::pow(a, b); }
    double fAtan2(double a, double b) { return std::atan2(a, b); }
    double fMin(double a, double b) { return (a < b) ? a : b; }
    double fMax(double a, double b) { return (a > b) ? a : b; }

    struct Func1 { const char* name; double (*fn)(double); };
    struct Func2 { const char* name; double (*fn)(double, double); };

    const Func1 kFunc1[] = {
        {"sin", fSin}, {"cos", fCos}, {"tan", fTan},
        {"asin", fAsin}, {"acos", fAcos}, {"atan", fAtan},
        {"sinh", fSinh}, {"cosh", fCosh}, {"tanh", fTanh},
        {"exp", fExp}, {"ln", fLn}, {"log", fLog10},
        {"sqrt", fSqrt}, {"cbrt", fCbrt}, {"abs", fAbs}, {"sign", fSign},
        {"floor", fFloor}, {"ceil", fCeil}
    };
    const int kFunc1Count = (int)(sizeof(kFunc1) / sizeof(kFunc1[0]));

    const Func2 kFunc2[] = {
        {"pow", fPow}, {"atan2", fAtan2}, {"min", fMin}, {"max", fMax}
    };
    const int kFunc2Count = (int)(sizeof(kFunc2) / sizeof(kFunc2[0]));

    enum TokType { T_END, T_NUM, T_IDENT, T_OP };
    struct Token {
        TokType type = T_END;
        double num = 0.0;
        std::string text;
        char op = 0;
        size_t pos = 0;
    };

    const int kMaxStack = 64;

    class ExprCompiler {
    public:
        ExprCompiler(const std::string& text, const std::vector<std::string>& vars,
            std::vector<Expression::Instr>& code, std::string& err)
            : text_(text), vars_(vars), code_(code), err_(err) {
        }

        bool Run() {
            if (!Tokenize()) return false;
            if (toks_.size() == 1) {
                return Fail("식이 비어 있어요", 0);
            }
            if (!ParseExpr()) return false;
            if (Cur().type != T_END) {
                return Fail("여기서 식이 끝나야 해요", Cur().pos);
            }
            if (maxDepth_ >= kMaxStack - 2) {
                return Fail("식이 너무 복잡해요", 0);
            }
            return true;
        }

    private:
        const std::string& text_;
        const std::vector<std::string>& vars_;
        std::vector<Expression::Instr>& code_;
        std::string& err_;
        std::vector<Token> toks_;
        size_t cur_ = 0;
        int depth_ = 0;
        int maxDepth_ = 0;

        bool Fail(const std::string& msg, size_t pos) {
            if (err_.empty()) {
                err_ = "식 오류: " + msg + " (" + std::to_string(pos + 1) + "번째 글자 근처)";
            }
            return false;
        }

        const Token& Cur() const { return toks_[cur_]; }
        void Advance() { if (cur_ + 1 < toks_.size()) cur_++; }
        bool IsOp(char c) const { return Cur().type == T_OP && Cur().op == c; }

        void Emit(Expression::Op op, double value, int index, int depthDelta) {
            Expression::Instr in;
            in.op = op;
            in.value = value;
            in.index = index;
            code_.push_back(in);
            depth_ += depthDelta;
            if (depth_ > maxDepth_) maxDepth_ = depth_;
        }

        bool Tokenize() {
            size_t i = 0;
            const size_t n = text_.size();
            while (i < n) {
                const unsigned char ch = (unsigned char)text_[i];
                if (std::isspace(ch)) { i++; continue; }

                Token t;
                t.pos = i;
                if (std::isdigit(ch) || ch == '.') {
                    size_t j = i;
                    while (j < n && std::isdigit((unsigned char)text_[j])) j++;
                    if (j < n && text_[j] == '.') {
                        j++;
                        while (j < n && std::isdigit((unsigned char)text_[j])) j++;
                    }
                    if (j < n && (text_[j] == 'e' || text_[j] == 'E')) {
                        size_t k = j + 1;
                        if (k < n && (text_[k] == '+' || text_[k] == '-')) k++;
                        if (k < n && std::isdigit((unsigned char)text_[k])) {
                            while (k < n && std::isdigit((unsigned char)text_[k])) k++;
                            j = k;
                        }
                    }
                    const std::string numText = text_.substr(i, j - i);
                    if (numText == ".") return Fail("숫자가 올바르지 않아요", i);
                    t.type = T_NUM;
                    t.num = std::strtod(numText.c_str(), nullptr);
                    i = j;
                }
                else if (std::isalpha(ch) || ch == '_') {
                    size_t j = i;
                    while (j < n && (std::isalnum((unsigned char)text_[j]) || text_[j] == '_')) j++;
                    t.type = T_IDENT;
                    t.text = text_.substr(i, j - i);
                    for (char& c : t.text) c = (char)std::tolower((unsigned char)c);
                    i = j;
                }
                else if (ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '^' ||
                    ch == '(' || ch == ')' || ch == ',') {
                    t.type = T_OP;
                    t.op = (char)ch;
                    i++;
                }
                else {
                    return Fail(std::string("사용할 수 없는 문자예요: '") + text_[i] + "'", i);
                }
                toks_.push_back(t);
            }
            Token end;
            end.type = T_END;
            end.pos = n;
            toks_.push_back(end);
            return true;
        }

        bool ParseExpr() {
            if (!ParseTerm()) return false;
            while (IsOp('+') || IsOp('-')) {
                const char op = Cur().op;
                Advance();
                if (!ParseTerm()) return false;
                Emit(op == '+' ? Expression::ADD : Expression::SUB, 0.0, 0, -1);
            }
            return true;
        }

        bool StartsPrimary() const {
            return Cur().type == T_NUM || Cur().type == T_IDENT || IsOp('(');
        }

        bool ParseTerm() {
            if (!ParseUnary()) return false;
            while (true) {
                if (IsOp('*') || IsOp('/')) {
                    const char op = Cur().op;
                    Advance();
                    if (!ParseUnary()) return false;
                    Emit(op == '*' ? Expression::MUL : Expression::DIV, 0.0, 0, -1);
                }
                else if (StartsPrimary()) {
                    if (Cur().type == T_NUM && cur_ > 0 && toks_[cur_ - 1].type == T_NUM) {
                        return Fail("숫자 두 개가 붙어 있어요", Cur().pos);
                    }
                    if (!ParsePower()) return false;
                    Emit(Expression::MUL, 0.0, 0, -1);
                }
                else {
                    break;
                }
            }
            return true;
        }

        bool ParseUnary() {
            if (IsOp('-')) {
                Advance();
                if (!ParseUnary()) return false;
                Emit(Expression::NEG, 0.0, 0, 0);
                return true;
            }
            if (IsOp('+')) {
                Advance();
                return ParseUnary();
            }
            return ParsePower();
        }

        bool ParsePower() {
            if (!ParsePrimary()) return false;
            if (IsOp('^')) {
                Advance();
                if (!ParseUnary()) return false;
                Emit(Expression::POW, 0.0, 0, -1);
            }
            return true;
        }

        int FindVar(const std::string& name) const {
            for (size_t i = 0; i < vars_.size(); i++) {
                if (vars_[i] == name) return (int)i;
            }
            return -1;
        }
        int FindFunc1(const std::string& name) const {
            for (int i = 0; i < kFunc1Count; i++) if (name == kFunc1[i].name) return i;
            return -1;
        }
        int FindFunc2(const std::string& name) const {
            for (int i = 0; i < kFunc2Count; i++) if (name == kFunc2[i].name) return i;
            return -1;
        }

        bool Expect(char c, const char* what) {
            if (!IsOp(c)) return Fail(what, Cur().pos);
            Advance();
            return true;
        }

        bool ParsePrimary() {
            const Token tok = Cur();
            if (tok.type == T_NUM) {
                Advance();
                Emit(Expression::PUSH_CONST, tok.num, 0, 1);
                return true;
            }
            if (tok.type == T_OP && tok.op == '(') {
                Advance();
                if (!ParseExpr()) return false;
                return Expect(')', "닫는 괄호 ')' 가 필요해요");
            }
            if (tok.type == T_IDENT) {
                Advance();
                const int f1 = FindFunc1(tok.text);
                const int f2 = FindFunc2(tok.text);
                if ((f1 >= 0 || f2 >= 0) && IsOp('(')) {
                    Advance();
                    if (!ParseExpr()) return false;
                    if (f1 >= 0) {
                        if (!Expect(')', "함수 뒤에 닫는 괄호 ')' 가 필요해요")) return false;
                        Emit(Expression::FUNC1, 0.0, f1, 0);
                    }
                    else {
                        if (!Expect(',', "이 함수는 값이 2개 필요해요 (쉼표로 구분)")) return false;
                        if (!ParseExpr()) return false;
                        if (!Expect(')', "함수 뒤에 닫는 괄호 ')' 가 필요해요")) return false;
                        Emit(Expression::FUNC2, 0.0, f2, -1);
                    }
                    return true;
                }
                const int v = FindVar(tok.text);
                if (v >= 0) {
                    Emit(Expression::PUSH_VAR, 0.0, v, 1);
                    return true;
                }
                if (tok.text == "pi") {
                    Emit(Expression::PUSH_CONST, 3.14159265358979323846, 0, 1);
                    return true;
                }
                if (tok.text == "e") {
                    Emit(Expression::PUSH_CONST, 2.71828182845904523536, 0, 1);
                    return true;
                }
                if (f1 >= 0 || f2 >= 0) {
                    return Fail("함수 이름 뒤에는 괄호 ( ) 가 필요해요: " + tok.text, tok.pos);
                }
                return Fail("알 수 없는 이름이에요: " + tok.text, tok.pos);
            }
            if (tok.type == T_END) {
                return Fail("식이 중간에 끝났어요", tok.pos);
            }
            return Fail(std::string("여기에는 올 수 없는 기호예요: '") + tok.op + "'", tok.pos);
        }
    };

}

bool Expression::Compile(const std::string& text, const std::vector<std::string>& varNames, std::string& errorMessage) {
    std::vector<Instr> newCode;
    std::string err;
    ExprCompiler compiler(text, varNames, newCode, err);
    if (!compiler.Run()) {
        errorMessage = err;
        return false;
    }
    code_ = newCode;
    errorMessage.clear();
    return true;
}

double Expression::Eval(const double* vars) const {
    constexpr double nan = std::numeric_limits<double>::quiet_NaN();
    double st[kMaxStack] = {};
    int sp = 0;
    for (const Instr& in : code_) {
        switch (in.op) {
        case PUSH_CONST:
            if (sp >= kMaxStack) return nan;
            st[sp] = in.value;
            sp++;
            break;
        case PUSH_VAR:
            if (sp >= kMaxStack) return nan;
            st[sp] = vars[in.index];
            sp++;
            break;
        case ADD:
            if (sp < 2) return nan;
            st[sp - 2] = st[sp - 2] + st[sp - 1];
            sp--;
            break;
        case SUB:
            if (sp < 2) return nan;
            st[sp - 2] = st[sp - 2] - st[sp - 1];
            sp--;
            break;
        case MUL:
            if (sp < 2) return nan;
            st[sp - 2] = st[sp - 2] * st[sp - 1];
            sp--;
            break;
        case DIV:
            if (sp < 2) return nan;
            st[sp - 2] = st[sp - 2] / st[sp - 1];
            sp--;
            break;
        case POW:
            if (sp < 2) return nan;
            st[sp - 2] = std::pow(st[sp - 2], st[sp - 1]);
            sp--;
            break;
        case NEG:
            if (sp < 1) return nan;
            st[sp - 1] = -st[sp - 1];
            break;
        case FUNC1:
            if (sp < 1) return nan;
            st[sp - 1] = kFunc1[in.index].fn(st[sp - 1]);
            break;
        case FUNC2:
            if (sp < 2) return nan;
            st[sp - 2] = kFunc2[in.index].fn(st[sp - 2], st[sp - 1]);
            sp--;
            break;
        }
    }
    return (sp > 0) ? st[sp - 1] : nan;
}