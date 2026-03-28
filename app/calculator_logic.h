#pragma once

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>

namespace EUINEO {

struct CalculatorState {
    std::string input = "0";
    double accumulator = 0.0;
    char pendingOp = 0;
    bool startNewInput = true;
    bool hasError = false;
    std::string historyLeft;
    std::string historyOp;
    std::string historyRight;
};

class CalculatorLogic {
public:
    const CalculatorState& state() const {
        return state_;
    }

    void inputDigit(std::string_view digit) {
        if (state_.hasError) {
            state_ = CalculatorState{};
        }
        if (state_.startNewInput) {
            state_.input = std::string(digit);
            state_.startNewInput = false;
            return;
        }
        if (state_.input == "0") {
            state_.input = std::string(digit);
            return;
        }
        if (state_.input.size() < 18) {
            state_.input += std::string(digit);
        }
    }

    void inputDot() {
        if (state_.hasError) {
            state_ = CalculatorState{};
        }
        if (state_.startNewInput) {
            state_.input = "0.";
            state_.startNewInput = false;
            return;
        }
        if (state_.input.find('.') == std::string::npos) {
            state_.input += ".";
        }
    }

    void clearAll() {
        state_ = CalculatorState{};
    }

    void backspace() {
        if (state_.hasError) {
            clearAll();
            return;
        }
        if (!state_.startNewInput && !state_.input.empty() && state_.input != "0") {
            state_.input.pop_back();
            if (state_.input.empty() || state_.input == "-") {
                state_.input = "0";
            }
        }
    }

    void toggleSign() {
        if (state_.hasError) {
            clearAll();
            return;
        }
        setInputValue(-currentValue());
        state_.startNewInput = false;
    }

    void percent() {
        if (state_.hasError) {
            clearAll();
            return;
        }
        setInputValue(currentValue() / 100.0);
        state_.startNewInput = false;
    }

    void inputOperator(char op) {
        if (state_.hasError) {
            clearAll();
        }
        const double right = currentValue();
        if (state_.pendingOp != 0 && !state_.startNewInput) {
            const double result = applyBinary(state_.accumulator, right, state_.pendingOp);
            state_.historyLeft = formatNumber(state_.accumulator);
            state_.historyOp = std::string(1, operatorGlyph(state_.pendingOp));
            state_.historyRight = formatNumber(right);
            state_.accumulator = result;
            setInputValue(result);
        } else {
            state_.accumulator = right;
            state_.historyLeft = formatNumber(right);
            state_.historyRight.clear();
        }
        state_.pendingOp = op;
        state_.historyOp = std::string(1, operatorGlyph(op));
        state_.startNewInput = true;
    }

    void evaluate() {
        if (state_.hasError || state_.pendingOp == 0) {
            return;
        }
        const double right = currentValue();
        const double result = applyBinary(state_.accumulator, right, state_.pendingOp);
        state_.historyLeft = formatNumber(state_.accumulator);
        state_.historyOp = std::string(1, operatorGlyph(state_.pendingOp));
        state_.historyRight = formatNumber(right);
        setInputValue(result);
        state_.pendingOp = 0;
        state_.startNewInput = true;
    }

private:
    static std::string formatNumber(double value) {
        if (!std::isfinite(value)) {
            return "Error";
        }

        const bool negative = value < 0.0;
        const double absValue = std::abs(value);
        const double rounded = std::round(absValue);

        std::string raw;
        if (std::abs(absValue - rounded) < 1e-9) {
            raw = std::to_string(static_cast<long long>(rounded));
        } else {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(8) << absValue;
            raw = oss.str();
            while (!raw.empty() && raw.back() == '0') {
                raw.pop_back();
            }
            if (!raw.empty() && raw.back() == '.') {
                raw.pop_back();
            }
        }

        std::string integerPart = raw;
        std::string fractionPart;
        const std::size_t dotPos = raw.find('.');
        if (dotPos != std::string::npos) {
            integerPart = raw.substr(0, dotPos);
            fractionPart = raw.substr(dotPos + 1);
        }

        std::string grouped;
        grouped.reserve(integerPart.size() + integerPart.size() / 3);
        int digitCount = 0;
        for (int index = static_cast<int>(integerPart.size()) - 1; index >= 0; --index) {
            grouped.push_back(integerPart[static_cast<std::size_t>(index)]);
            digitCount++;
            if (digitCount % 3 == 0 && index > 0) {
                grouped.push_back(',');
            }
        }
        std::reverse(grouped.begin(), grouped.end());

        if (!fractionPart.empty()) {
            grouped += ".";
            grouped += fractionPart;
        }
        if (negative && grouped != "0") {
            grouped = "-" + grouped;
        }
        return grouped;
    }

    static double parseNumber(const std::string& text) {
        std::string normalized;
        normalized.reserve(text.size());
        for (char ch : text) {
            if (ch != ',') {
                normalized.push_back(ch);
            }
        }
        if (normalized.empty() || normalized == "-" || normalized == ".") {
            return 0.0;
        }
        return std::stod(normalized);
    }

    static char operatorGlyph(char op) {
        if (op == '*') {
            return 'x';
        }
        if (op == '/') {
            return '/';
        }
        return op;
    }

    static double applyBinary(double left, double right, char op) {
        if (op == '+') return left + right;
        if (op == '-') return left - right;
        if (op == '*') return left * right;
        if (op == '/') return std::abs(right) < 1e-12 ? std::numeric_limits<double>::infinity() : left / right;
        return right;
    }

    void setInputValue(double value) {
        if (!std::isfinite(value)) {
            state_.input = "Error";
            state_.hasError = true;
            state_.pendingOp = 0;
            state_.startNewInput = true;
            return;
        }
        state_.input = formatNumber(value);
        state_.hasError = false;
    }

    double currentValue() const {
        if (state_.hasError) {
            return 0.0;
        }
        return parseNumber(state_.input);
    }

    CalculatorState state_{};
};

} // namespace EUINEO
