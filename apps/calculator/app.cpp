#include "eui_neo.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace app {
namespace {

std::string entry = "0";
std::string expression;
std::string formula;
bool freshEntry = true;
bool justEvaluated = false;
bool advancedMode = false;
bool angleRadians = true;
bool inverseMode = false;

struct CalculatorState {
    std::string entry;
    std::string expression;
    std::string formula;
    bool freshEntry = true;
    bool justEvaluated = false;
    bool angleRadians = true;
    bool inverseMode = false;
};

std::vector<CalculatorState> history;

constexpr eui::Color kBg{0.07f, 0.075f, 0.085f, 1.0f};
constexpr eui::Color kButtonTop{0.20f, 0.215f, 0.225f, 1.0f};
constexpr eui::Color kButtonBottom{0.075f, 0.08f, 0.085f, 1.0f};
constexpr eui::Color kText{0.94f, 0.95f, 0.95f, 1.0f};
constexpr eui::Color kMuted{0.55f, 0.56f, 0.58f, 1.0f};
constexpr eui::Color kClear{0.0f, 0.0f, 0.0f, 0.0f};

std::string trimNumber(double value) {
    if (!std::isfinite(value)) {
        return "Error";
    }
    if (std::fabs(value) < 0.00000001) {
        value = 0.0;
    }

    std::ostringstream out;
    out << std::fixed << std::setprecision(8) << value;
    std::string text = out.str();
    while (text.size() > 1 && text.back() == '0') {
        text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
        text.pop_back();
    }
    return text;
}

std::string groupNumber(std::string text) {
    if (text == "Error") {
        return text;
    }

    const bool negative = !text.empty() && text[0] == '-';
    if (negative) {
        text.erase(text.begin());
    }

    const size_t dot = text.find('.');
    std::string whole = dot == std::string::npos ? text : text.substr(0, dot);
    const std::string decimal = dot == std::string::npos ? std::string{} : text.substr(dot);
    for (int i = static_cast<int>(whole.size()) - 3; i > 0; i -= 3) {
        whole.insert(static_cast<size_t>(i), ",");
    }
    return (negative ? "-" : "") + whole + decimal;
}

CalculatorState currentState() {
    return {entry, expression, formula, freshEntry, justEvaluated, angleRadians, inverseMode};
}

void restoreState(const CalculatorState& state) {
    entry = state.entry;
    expression = state.expression;
    formula = state.formula;
    freshEntry = state.freshEntry;
    justEvaluated = state.justEvaluated;
    angleRadians = state.angleRadians;
    inverseMode = state.inverseMode;
}

void pushHistory() {
    history.push_back(currentState());
    if (history.size() > 96) {
        history.erase(history.begin());
    }
}

void undo() {
    if (history.empty()) {
        return;
    }
    restoreState(history.back());
    history.pop_back();
}

bool isOperatorChar(char ch) {
    return ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '^';
}

std::string displayFormula(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        const char ch = text[i];
        if (ch == '*') {
            out += eui::utf8(0x00D7);
        } else if (ch == '/') {
            out += eui::utf8(0x00F7);
        } else if (ch == '-') {
            out += eui::utf8(0x2212);
        } else if (ch == 'p' && text.compare(i, 2, "pi") == 0) {
            out += eui::utf8(0x03C0);
            ++i;
        } else if (text.compare(i, 6, "pow10(") == 0) {
            out += "10^(";
            i += 5;
        } else if (text.compare(i, 4, "exp(") == 0) {
            out += "e^(";
            i += 3;
        } else if (text.compare(i, 7, "square(") == 0) {
            out += "square(";
            i += 6;
        } else if (text.compare(i, 5, "sqrt(") == 0) {
            out += eui::utf8(0x221A);
            out += "(";
            i += 4;
        } else {
            out.push_back(ch);
        }
    }
    return out;
}

void updateExpression() {
    expression = formula.empty() ? std::string{} : displayFormula(formula);
}

double value() {
    if (entry == "Error") {
        return 0.0;
    }

    char* end = nullptr;
    errno = 0;
    const double parsed = std::strtod(entry.c_str(), &end);
    if (end == entry.c_str() || *end != '\0' || errno == ERANGE || !std::isfinite(parsed)) {
        return 0.0;
    }
    return parsed;
}

constexpr double kPi = 3.14159265358979323846;

double toRadians(double input) {
    return angleRadians ? input : input * kPi / 180.0;
}

double fromRadians(double input) {
    return angleRadians ? input : input * 180.0 / kPi;
}

double factorial(double input) {
    if (input < 0.0 || input > 170.0 || std::fabs(input - std::round(input)) > 0.0000001) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return std::tgamma(std::round(input) + 1.0);
}

double applyFunction(const std::string& name, double input) {
    if (name == "sin") {
        return std::sin(toRadians(input));
    }
    if (name == "cos") {
        return std::cos(toRadians(input));
    }
    if (name == "tan") {
        return std::tan(toRadians(input));
    }
    if (name == "asin") {
        return fromRadians(std::asin(input));
    }
    if (name == "acos") {
        return fromRadians(std::acos(input));
    }
    if (name == "atan") {
        return fromRadians(std::atan(input));
    }
    if (name == "log") {
        return input <= 0.0 ? std::numeric_limits<double>::quiet_NaN() : std::log10(input);
    }
    if (name == "ln") {
        return input <= 0.0 ? std::numeric_limits<double>::quiet_NaN() : std::log(input);
    }
    if (name == "sqrt") {
        return input < 0.0 ? std::numeric_limits<double>::quiet_NaN() : std::sqrt(input);
    }
    if (name == "pow10") {
        return std::pow(10.0, input);
    }
    if (name == "exp") {
        return std::exp(input);
    }
    if (name == "square") {
        return input * input;
    }
    return std::numeric_limits<double>::quiet_NaN();
}

class ExpressionParser {
public:
    explicit ExpressionParser(const std::string& source) : source_(source) {}

    bool parse(double& value) {
        pos_ = 0;
        ok_ = true;
        value = parseExpression();
        skipSpaces();
        return ok_ && pos_ == source_.size() && std::isfinite(value);
    }

private:
    void skipSpaces() {
        while (pos_ < source_.size() && std::isspace(static_cast<unsigned char>(source_[pos_]))) {
            ++pos_;
        }
    }

    bool match(char ch) {
        skipSpaces();
        if (pos_ < source_.size() && source_[pos_] == ch) {
            ++pos_;
            return true;
        }
        return false;
    }

    double parseExpression() {
        double left = parseTerm();
        while (ok_) {
            if (match('+')) {
                left += parseTerm();
            } else if (match('-')) {
                left -= parseTerm();
            } else {
                break;
            }
        }
        return left;
    }

    double parseTerm() {
        double left = parsePower();
        while (ok_) {
            if (match('*')) {
                left *= parsePower();
            } else if (match('/')) {
                const double right = parsePower();
                if (std::fabs(right) < 0.00000001) {
                    ok_ = false;
                    return std::numeric_limits<double>::quiet_NaN();
                }
                left /= right;
            } else {
                break;
            }
        }
        return left;
    }

    double parsePower() {
        double left = parseUnary();
        if (match('^')) {
            left = std::pow(left, parsePower());
        }
        return left;
    }

    double parseUnary() {
        if (match('+')) {
            return parseUnary();
        }
        if (match('-')) {
            return -parseUnary();
        }
        return parsePostfix();
    }

    double parsePostfix() {
        double value = parsePrimary();
        while (ok_) {
            if (match('!')) {
                value = factorial(value);
            } else if (match('%')) {
                value /= 100.0;
            } else {
                break;
            }
        }
        return value;
    }

    double parsePrimary() {
        skipSpaces();
        if (pos_ >= source_.size()) {
            ok_ = false;
            return 0.0;
        }

        if (match('(')) {
            const double inner = parseExpression();
            if (!match(')')) {
                ok_ = false;
            }
            return inner;
        }

        if (std::isdigit(static_cast<unsigned char>(source_[pos_])) || source_[pos_] == '.') {
            char* end = nullptr;
            const double parsed = std::strtod(source_.c_str() + pos_, &end);
            if (end == source_.c_str() + pos_ || !std::isfinite(parsed)) {
                ok_ = false;
                return 0.0;
            }
            pos_ = static_cast<size_t>(end - source_.c_str());
            return parsed;
        }

        if (std::isalpha(static_cast<unsigned char>(source_[pos_]))) {
            const size_t start = pos_;
            while (pos_ < source_.size() && std::isalnum(static_cast<unsigned char>(source_[pos_]))) {
                ++pos_;
            }
            const std::string name = source_.substr(start, pos_ - start);
            if (name == "pi") {
                return kPi;
            }
            if (name == "e") {
                return std::exp(1.0);
            }
            if (!match('(')) {
                ok_ = false;
                return 0.0;
            }
            const double input = parseExpression();
            if (!match(')')) {
                ok_ = false;
                return 0.0;
            }
            return applyFunction(name, input);
        }

        ok_ = false;
        return 0.0;
    }

    const std::string& source_;
    size_t pos_ = 0;
    bool ok_ = true;
};

bool evaluate(const std::string& text, double& result) {
    ExpressionParser parser(text);
    return parser.parse(result);
}

void clear() {
    entry = "0";
    expression.clear();
    formula.clear();
    freshEntry = true;
    justEvaluated = false;
    inverseMode = false;
}

void beginNewFormulaIfNeeded() {
    if (justEvaluated) {
        formula.clear();
        expression.clear();
        justEvaluated = false;
    }
}

bool formulaNeedsValue() {
    return formula.empty() || formula.back() == '(' || isOperatorChar(formula.back());
}

void commitEntry() {
    if (entry == "Error") {
        return;
    }
    if (!freshEntry) {
        formula += entry;
        freshEntry = true;
    }
    updateExpression();
}

std::string expressionForEvaluation() {
    std::string text = formula;
    if (!freshEntry && entry != "Error") {
        text += entry;
    } else if (!text.empty() && isOperatorChar(text.back()) && entry != "Error") {
        text += entry;
    }
    return text;
}

int parenBalance(const std::string& text) {
    int balance = 0;
    for (char ch : text) {
        if (ch == '(') {
            ++balance;
        } else if (ch == ')' && balance > 0) {
            --balance;
        }
    }
    return balance;
}

void digit(const std::string& text) {
    beginNewFormulaIfNeeded();
    if (freshEntry || entry == "Error") {
        entry = text == "00" ? "0" : text;
        freshEntry = false;
    } else if (entry.size() < 12) {
        entry = entry == "0" && text != "00" ? text : entry + text;
    }
}

void dot() {
    beginNewFormulaIfNeeded();
    if (freshEntry || entry == "Error") {
        entry = "0.";
        freshEntry = false;
    } else if (entry.find('.') == std::string::npos) {
        entry += ".";
    }
}

void backspace() {
    justEvaluated = false;
    if (entry == "Error") {
        entry = "0";
        freshEntry = true;
        return;
    }
    if (freshEntry) {
        if (!formula.empty()) {
            formula.pop_back();
            updateExpression();
        }
        return;
    }
    if (entry.size() <= 1) {
        entry = "0";
        freshEntry = true;
        return;
    }
    entry.pop_back();
    if (entry == "-" || entry.empty()) {
        entry = "0";
        freshEntry = true;
    }
}

void percent() {
    beginNewFormulaIfNeeded();
    if (freshEntry && !formula.empty() && formula.back() == ')') {
        formula.push_back('%');
        updateExpression();
        return;
    }
    entry = trimNumber(value() / 100.0);
    freshEntry = true;
}

void setOperator(char op) {
    if (entry == "Error") {
        clear();
    }
    if (justEvaluated) {
        formula = entry;
        justEvaluated = false;
    } else if (freshEntry && formula.empty()) {
        formula = entry;
    } else {
        commitEntry();
    }
    if (!formula.empty() && isOperatorChar(formula.back())) {
        formula.back() = op;
    } else if (!formula.empty() && formula.back() != '(') {
        formula.push_back(op);
    } else if (op == '-') {
        formula.push_back(op);
    }
    freshEntry = true;
    updateExpression();
}

void appendOpenParen() {
    beginNewFormulaIfNeeded();
    if (!freshEntry) {
        commitEntry();
        formula.push_back('*');
    } else if (!formula.empty() && formula.back() == ')') {
        formula.push_back('*');
    }
    formula.push_back('(');
    freshEntry = true;
    updateExpression();
}

void appendCloseParen() {
    beginNewFormulaIfNeeded();
    if (!freshEntry) {
        commitEntry();
    }
    if (parenBalance(formula) > 0 && !formula.empty() && !isOperatorChar(formula.back()) && formula.back() != '(') {
        formula.push_back(')');
    }
    freshEntry = true;
    updateExpression();
}

void applyUnary(const std::string& name, const std::string& displayName) {
    beginNewFormulaIfNeeded();
    const std::string functionName =
        inverseMode && name == "sin" ? "asin" :
        inverseMode && name == "cos" ? "acos" :
        inverseMode && name == "tan" ? "atan" :
        inverseMode && name == "log" ? "pow10" :
        inverseMode && name == "ln" ? "exp" :
        inverseMode && name == "sqrt" ? "square" :
        name;

    if (freshEntry || formulaNeedsValue()) {
        formula += functionName + "(";
        updateExpression();
        return;
    }

    const std::string before = entry;
    entry = trimNumber(applyFunction(functionName, value()));
    const std::string visibleName = functionName == "pow10" ? "10^" :
                                    functionName == "exp" ? "e^" :
                                    functionName == "square" ? "square" :
                                    displayName;
    expression = visibleName + "(" + groupNumber(before) + ")";
    freshEntry = true;
}

void factorialEntry() {
    beginNewFormulaIfNeeded();
    if (freshEntry && !formula.empty() && formula.back() == ')') {
        formula.push_back('!');
        updateExpression();
        return;
    }
    const std::string before = entry;
    entry = trimNumber(factorial(value()));
    expression = groupNumber(before) + "!";
    freshEntry = true;
}

void setConstant(const std::string& name, double constantValue) {
    beginNewFormulaIfNeeded();
    entry = trimNumber(constantValue);
    expression = name;
    freshEntry = false;
}

void equals() {
    std::string text = expressionForEvaluation();
    if (text.empty()) {
        return;
    }
    const int openParens = parenBalance(text);
    for (int i = 0; i < openParens; ++i) {
        text.push_back(')');
    }

    double result = 0.0;
    if (!evaluate(text, result)) {
        entry = "Error";
    } else {
        entry = trimNumber(result);
    }
    expression = displayFormula(text);
    formula.clear();
    freshEntry = true;
    justEvaluated = true;
}

unsigned int keyIcon(const std::string& key) {
    if (key == "back") {
        return 0xF55A;
    }
    if (key == "undo") {
        return 0xF2EA;
    }
    if (key == "div") {
        return 0xF529;
    }
    if (key == "mul") {
        return 0xF00D;
    }
    if (key == "sub") {
        return 0xF068;
    }
    if (key == "+") {
        return 0xF067;
    }
    if (key == "%") {
        return 0xF295;
    }
    if (key == "=") {
        return 0xF52C;
    }
    return 0;
}

std::string keyLabel(const std::string& key) {
    if (key == "sqrt") {
        return eui::utf8(0x221A);
    }
    if (key == "pi") {
        return eui::utf8(0x03C0);
    }
    if (key == "sub") {
        return eui::utf8(0x2212);
    }
    if (key == "mul") {
        return eui::utf8(0x00D7);
    }
    if (key == "div") {
        return eui::utf8(0x00F7);
    }
    return key;
}

void press(const std::string& key) {
    if (key == "undo") {
        undo();
        return;
    }

    pushHistory();
    if (key == "AC") {
        clear();
    } else if (key == "back") {
        backspace();
    } else if (key == "%") {
        percent();
    } else if (key == ".") {
        dot();
    } else if (key == "=") {
        equals();
    } else if (key == "+" || key == "sub" || key == "mul" || key == "div") {
        setOperator(key == "mul" ? '*' : key == "div" ? '/' : key == "sub" ? '-' : '+');
    } else if (key == "^") {
        setOperator('^');
    } else if (key == "sin" || key == "cos" || key == "tan" || key == "log" || key == "ln" || key == "sqrt") {
        applyUnary(key, keyLabel(key));
    } else if (key == "(") {
        appendOpenParen();
    } else if (key == ")") {
        appendCloseParen();
    } else if (key == "!") {
        factorialEntry();
    } else if (key == "pi") {
        setConstant(eui::utf8(0x03C0), kPi);
    } else if (key == "e") {
        setConstant("e", std::exp(1.0));
    } else if (key == "rad") {
        angleRadians = true;
    } else if (key == "deg") {
        angleRadians = false;
    } else if (key == "inv") {
        inverseMode = !inverseMode;
    } else {
        digit(key);
    }
}

float displayFont(float width, const std::string& text) {
    return std::clamp(width / std::max(4.0f, static_cast<float>(text.size()) * 0.56f), 52.0f, 96.0f);
}

void key(eui::Ui& ui, const std::string& id, const std::string& label,
         float x, float y, float size, bool accent = false, bool active = false) {
    const eui::Color top = accent ? eui::Color{0.86f, 0.95f, 0.90f, 1.0f} :
                           active ? eui::Color{0.34f, 0.42f, 0.40f, 1.0f} : eui::Color{0.40f, 0.415f, 0.420f, 1.0f};
    const eui::Color bottom = accent ? eui::Color{0.52f, 0.66f, 0.60f, 1.0f} :
                              active ? eui::Color{0.07f, 0.12f, 0.11f, 1.0f} : eui::Color{0.055f, 0.058f, 0.064f, 1.0f};
    const unsigned int icon = keyIcon(label);
    const std::string shownLabel = keyLabel(label);
    const bool compactLabel = shownLabel == "AC" || shownLabel == "00" || shownLabel == "sin" ||
                              shownLabel == "cos" || shownLabel == "tan" || shownLabel == "log" ||
                              shownLabel == "ln" || shownLabel == "rad" || shownLabel == "deg" ||
                              shownLabel == "inv";

    ui.stack(id)
        .x(x)
        .y(y)
        .size(size, size)
        .visualStateFrom(id + ".hit")
        .content([&] {
            ui.rect(id + ".face")
                .size(size, size)
                .gradient(top, bottom)
                .radius(size * 0.5f)
                .border(1.0f, accent ? eui::Color{0.82f, 0.96f, 0.88f, 0.48f} : eui::Color{0.78f, 0.82f, 0.84f, 0.18f})
                .shadow(size * 0.26f, 0.0f, size * 0.10f,
                        accent ? eui::Color{0.45f, 0.86f, 0.66f, 0.20f} : eui::Color{0.0f, 0.0f, 0.0f, 0.42f})
                .build();

            ui.rect(id + ".lower.depth")
                .position(0.0f, size * 0.48f)
                .size(size, size * 0.52f)
                .gradient(eui::Color{0.0f, 0.0f, 0.0f, 0.0f},
                          accent ? eui::Color{0.04f, 0.12f, 0.08f, 0.12f} : eui::Color{0.0f, 0.0f, 0.0f, 0.24f})
                .radius(size * 0.26f)
                .build();

            ui.rect(id + ".rim")
                .position(size * 0.08f, size * 0.075f)
                .size(size * 0.84f, size * 0.84f)
                .color(kClear)
                .radius(size * 0.42f)
                .border(1.0f, accent ? eui::Color{1.0f, 1.0f, 1.0f, 0.26f} : eui::Color{1.0f, 1.0f, 1.0f, 0.14f})
                .build();

            auto text = ui.text(id + ".text")
                .size(size, size)
                .fontSize(icon != 0 ? size * 0.34f : (compactLabel ? size * 0.30f : size * 0.42f))
                .lineHeight(size * 0.44f)
                .color(accent ? eui::Color{0.96f, 0.98f, 0.96f, 1.0f} :
                       active ? eui::Color{0.58f, 0.78f, 0.70f, 1.0f} : kText)
                .horizontalAlign(eui::HorizontalAlign::Center)
                .verticalAlign(eui::VerticalAlign::Center);
            if (icon != 0) {
                text.icon(icon);
            } else {
                text.text(shownLabel);
            }
            text.build();

            ui.rect(id + ".hit")
                .size(size, size)
                .states(kClear, eui::Color{1.0f, 1.0f, 1.0f, 0.04f}, eui::Color{0.0f, 0.0f, 0.0f, 0.10f})
                .radius(size * 0.5f)
                .onClick([label] { press(label); })
                .build();
        })
        .build();
}

void roundToolButton(eui::Ui& ui, const std::string& id, const std::string& label,
                     float x, float y, float size, bool active, std::function<void()> onClick,
                     unsigned int icon = 0) {
    ui.stack(id)
        .x(x)
        .y(y)
        .size(size, size)
        .visualStateFrom(id + ".hit")
        .content([&] {
            ui.rect(id + ".face")
                .size(size, size)
                .gradient(active ? eui::Color{0.34f, 0.42f, 0.40f, 1.0f} : eui::Color{0.36f, 0.375f, 0.385f, 1.0f},
                          active ? eui::Color{0.07f, 0.12f, 0.10f, 1.0f} : eui::Color{0.05f, 0.052f, 0.058f, 1.0f})
                .radius(size * 0.5f)
                .border(1.0f, active ? eui::Color{0.78f, 0.96f, 0.88f, 0.36f} : eui::Color{0.78f, 0.82f, 0.84f, 0.14f})
                .shadow(size * 0.22f, 0.0f, size * 0.08f,
                        active ? eui::Color{0.30f, 0.74f, 0.58f, 0.16f} : eui::Color{0.0f, 0.0f, 0.0f, 0.34f})
                .build();

            ui.rect(id + ".rim")
                .position(size * 0.10f, size * 0.09f)
                .size(size * 0.80f, size * 0.80f)
                .color(kClear)
                .radius(size * 0.40f)
                .border(1.0f, active ? eui::Color{0.96f, 1.0f, 0.98f, 0.20f} : eui::Color{1.0f, 1.0f, 1.0f, 0.10f})
                .build();

            auto text = ui.text(id + ".text")
                .size(size, size)
                .fontSize(icon != 0 ? size * 0.34f : size * 0.36f)
                .lineHeight(size * 0.38f)
                .color(active ? eui::Color{0.68f, 0.86f, 0.78f, 1.0f} : kMuted)
                .horizontalAlign(eui::HorizontalAlign::Center)
                .verticalAlign(eui::VerticalAlign::Center);
            if (icon != 0) {
                text.icon(icon);
            } else {
                text.text(label);
            }
            text.build();

            ui.rect(id + ".hit")
                .size(size, size)
                .states(kClear, eui::Color{1.0f, 1.0f, 1.0f, 0.04f}, eui::Color{0.0f, 0.0f, 0.0f, 0.10f})
                .radius(size * 0.5f)
                .onClick(std::move(onClick))
                .build();
        })
        .build();
}

} // namespace

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = DslAppConfig{}
        .title("Calculator")
        .pageId("calculator")
        .clearColor(kBg)
        .windowSize(430, 760)
        .fps(90.0);
    return config;
}

void compose(eui::Ui& ui, const eui::Screen& screen) {
    const float margin = std::clamp(screen.width * 0.045f, 16.0f, 24.0f);
    const float w = std::max(260.0f, std::min(screen.width - margin * 2.0f, advancedMode ? 540.0f : 460.0f));
    const float h = std::max(520.0f, screen.height - margin * 2.0f);
    const int cols = advancedMode ? 5 : 4;
    const int rows = advancedMode ? 7 : 5;
    const float gap = std::clamp(w * (advancedMode ? 0.038f : 0.052f), advancedMode ? 10.0f : 14.0f, advancedMode ? 18.0f : 22.0f);
    const float buttonByWidth = (w - gap * static_cast<float>(cols - 1)) / static_cast<float>(cols);
    const float buttonByHeight = (h - 174.0f - gap * static_cast<float>(rows - 1)) / static_cast<float>(rows);
    const float button = std::clamp(std::min(buttonByWidth, buttonByHeight), advancedMode ? 42.0f : 54.0f, advancedMode ? 82.0f : 96.0f);
    const float gridW = button * static_cast<float>(cols) + gap * static_cast<float>(cols - 1);
    const float gridH = button * static_cast<float>(rows) + gap * static_cast<float>(rows - 1);
    const float gridX = (w - gridW) * 0.5f;
    const float gridY = h - gridH - 18.0f;
    const float headerH = std::max(140.0f, gridY - 8.0f);
    const std::string shown = groupNumber(entry);
    const std::string exp = expression.empty() ? " " : expression;

    ui.stack("root")
        .size(screen.width, screen.height)
        .align(eui::Align::CENTER, eui::Align::CENTER)
        .content([&] {
            ui.stack("calc")
                .size(w, h)
                .content([&] {
                    const float toolSize = std::clamp(button * 0.52f, 34.0f, 44.0f);
                    const float exprY = advancedMode ? std::max(50.0f, headerH - 102.0f) : std::max(58.0f, headerH - 136.0f);
                    const float exprH = advancedMode ? 36.0f : 48.0f;
                    const float resultY = advancedMode ? std::max(exprY + exprH + 6.0f, headerH - 62.0f)
                                                       : std::max(106.0f, headerH - 78.0f);
                    const float displayKeyGap = advancedMode ? 12.0f : 22.0f;
                    const float desiredResultH = advancedMode ? 82.0f : 92.0f;
                    const float resultH = std::max(16.0f,
                        std::min(desiredResultH, gridY - resultY - displayKeyGap));
                    const float resultFontFloor = advancedMode ? 52.0f : 72.0f;
                    const float resultFontLimit = std::max(resultFontFloor, resultH * 1.50f);
                    roundToolButton(ui, "tool.undo", "",
                                    w - toolSize * 2.0f - 12.0f, 14.0f, toolSize, !history.empty(), [] {
                                        undo();
                                    }, 0xF2EA);
                    roundToolButton(ui, "tool.advanced", advancedMode ? "123" : "fx",
                                    w - toolSize, 14.0f, toolSize, advancedMode, [] {
                                        advancedMode = !advancedMode;
                                    });

                    ui.text("expr")
                        .x(22.0f)
                        .y(exprY)
                        .size(w - 44.0f, exprH)
                        .text(exp)
                        .fontSize(advancedMode ? 26.0f : 34.0f)
                        .lineHeight(advancedMode ? 32.0f : 42.0f)
                        .color(kMuted)
                        .horizontalAlign(eui::HorizontalAlign::Right)
                        .verticalAlign(eui::VerticalAlign::Center)
                        .build();

                    ui.rect("cursor")
                        .x(w - 18.0f)
                        .y(exprY + 10.0f)
                        .size(2.0f, advancedMode ? 30.0f : 42.0f)
                        .color(eui::Color{0.68f, 0.86f, 0.80f, 0.78f})
                        .radius(1.0f)
                        .build();

                    ui.text("result")
                        .x(16.0f)
                        .y(resultY)
                        .size(w - 32.0f, resultH)
                        .text(shown)
                        .fontSize(std::min({displayFont(w - 32.0f, shown),
                                            advancedMode ? 72.0f : 96.0f,
                                            resultFontLimit}))
                        .lineHeight(resultH)
                        .color(eui::Color{1.0f, 1.0f, 1.0f, 1.0f})
                        .horizontalAlign(eui::HorizontalAlign::Right)
                        .verticalAlign(eui::VerticalAlign::Center)
                        .build();

                    if (advancedMode) {
                        const char* labels[7][5] = {
                            {"sin", "cos", "tan", "rad", "deg"},
                            {"log", "ln", "(", ")", "inv"},
                            {"!", "AC", "%", "undo", "div"},
                            {"^", "7", "8", "9", "mul"},
                            {"sqrt", "4", "5", "6", "sub"},
                            {"pi", "1", "2", "3", "+"},
                            {"e", "00", "0", ".", "="}
                        };
                        for (int row = 0; row < 7; ++row) {
                            for (int col = 0; col < 5; ++col) {
                                const std::string label = labels[row][col];
                                const bool active = (label == "rad" && angleRadians) ||
                                                    (label == "deg" && !angleRadians) ||
                                                    (label == "inv" && inverseMode);
                                key(ui, "key.adv." + std::to_string(row) + "." + std::to_string(col), label,
                                    gridX + static_cast<float>(col) * (button + gap),
                                    gridY + static_cast<float>(row) * (button + gap),
                                    button, label == "=", active);
                            }
                        }
                    } else {
                        const char* labels[5][4] = {
                            {"AC", "%", "back", "div"},
                            {"7", "8", "9", "mul"},
                            {"4", "5", "6", "sub"},
                            {"1", "2", "3", "+"},
                            {"00", "0", ".", "="}
                        };
                        for (int row = 0; row < 5; ++row) {
                            for (int col = 0; col < 4; ++col) {
                                const std::string label = labels[row][col];
                                key(ui, "key.basic." + std::to_string(row) + "." + std::to_string(col), label,
                                    gridX + static_cast<float>(col) * (button + gap),
                                    gridY + static_cast<float>(row) * (button + gap),
                                    button, label == "=");
                            }
                        }
                    }
                })
                .build();
        })
        .build();
}

} // namespace app
