#ifndef COMSERVATORY_CONVERT_HPP
#define COMSERVATORY_CONVERT_HPP

#include <string>
#include <cctype>
#include <complex>
#include <cmath>
#include <array>

namespace comservatory {

inline std::string get_location(size_t column, size_t line) {
    return std::string("field ") + std::to_string(column + 1) + " of line " + std::to_string(line + 1);
}

// Assumes that 'input' is located on the opening double quote. On return,
// 'input' is left on the next character _after_ the closing double quote.
template<class Input>
std::string to_string(Input& input, size_t column, size_t line) {
    std::string output;

    while (1) {
        input.advance();
        if (!input.valid()) {
            throw std::runtime_error("truncated string in " + get_location(column, line));
        }

        char next = input.get();
        if (next == '"') {
            input.advance();
            if (!input.valid()) {
                throw std::runtime_error("line " + std::to_string(line + 1) + " should be terminated with a newline");
            }

            if (input.get() != '"') {
                break;
            } else {
                output += '"';
            }
        } else {
            output += next;
        }
    }

    return output;
}

// Assumes that 'input' is located on the first letter of the keyword (i.e.,
// before the first letter of 'lower'). On return, 'input' is left on the
// first character _after_ the end of the keyword.
template<class Input>
void expect_fixed(Input& input, const std::string& lower, const std::string& upper, size_t column, size_t line) {
    for (size_t i = 0; i < lower.size(); ++i) {
        if (!input.valid()) {
            throw std::runtime_error("truncated keyword in " + get_location(column, line));
        }
        char x = input.get();
        if (x != lower[i] && x != upper[i]) {
            throw std::runtime_error("unknown keyword in " + get_location(column, line));
        }
        input.advance();
    }
}

// Assumes that 'input' is located on the first digit. On return, 'input' is
// left on the first character _after_ the end of the number.
template<class Input>
double to_number(Input& input, size_t column, size_t line) {
    double value = 0;
    double fractional = 10;
    double exponent = 0; 
    bool negative_exponent = false;

    auto is_terminator = [](char v) -> bool {
        return v == ',' || v == '\n' || v == '+' || v == '-' || v == 'i'; // last three are terminators for complex numbers.
    };

    bool in_fraction = false;
    bool in_exponent = false;

    // We assume we're starting from a digit, after handling any preceding sign. 
    char lead = input.get();
    value += lead - '0';
    input.advance();

    while (1) {
        if (!input.valid()) {
            throw std::runtime_error("line " + std::to_string(line + 1) + " should be terminated with a newline");
        }
        char val = input.get();
        if (val == '.') {
            in_fraction = true;
            break;
        } else if (val == 'e' || val == 'E') {
            in_exponent = true;
            break;
        } else if (is_terminator(val)) {
            return value;
        } else if (!std::isdigit(val)) {
            throw std::runtime_error("invalid number containing '" + std::string(1, val) + "' at " + get_location(column, line));
        }
        value *= 10;
        value += val - '0';
        input.advance();
    }

    if (in_fraction) {
        input.advance();
        if (!input.valid()) {
            throw std::runtime_error("line " + std::to_string(line + 1) + " should be terminated with a newline");
        }

        char val = input.get();
        if (!std::isdigit(val)) {
            throw std::runtime_error("'.' must be followed by at least one digit at " + get_location(column, line));
        }
        value += (val - '0') / fractional;

        input.advance();
        while (1) {
            if (!input.valid()) {
                throw std::runtime_error("line " + std::to_string(line + 1) + " should be terminated with a newline");
            }
            char val = input.get();
            if (val == 'e' || val == 'E') {
                in_exponent = true;
                break;
            } else if (is_terminator(val)) {
                return value;
            } else if (!std::isdigit(val)) {
                throw std::runtime_error("invalid fraction containing '" + std::string(1, val) + "' at " + get_location(column, line));
            }
            fractional *= 10;
            value += (val - '0') / fractional;
            input.advance();
        } 
    }

    if (in_exponent) {
        if (value < 1 || value >= 10) {
            throw std::runtime_error("absolute value of mantissa should be within [1, 10) at " + get_location(column, line));
        }

        input.advance();
        if (!input.valid()) {
            throw std::runtime_error("line " + std::to_string(line + 1) + " should be terminated with a newline");
        }

        char val = input.get();
        if (!std::isdigit(val)) {
            if (val == '-') {
                negative_exponent = true;
            } else if (val != '+') {
                throw std::runtime_error("'e/E' should be followed by a sign or digit in number at " + get_location(column, line));
            }
            input.advance();

            if (!input.valid()) {
                throw std::runtime_error("line " + std::to_string(line + 1) + " should be terminated with a newline");
            }
            val = input.get();
            if (!std::isdigit(val)) {
                throw std::runtime_error("exponent sign must be followed by at least one digit in number at " + get_location(column, line));
            }
        }

        exponent += (val - '0');
        input.advance();
        while (1) {
            if (!input.valid()) {
                throw std::runtime_error("line " + std::to_string(line + 1) + " should be terminated with a newline");
            }
            char val = input.get();
            if (is_terminator(val)) {
                break;
            } else if (!std::isdigit(val)) {
                throw std::runtime_error("invalid exponent containing '" + std::string(1, val) + "' at " + get_location(column, line));
            }
            exponent *= 10;
            exponent += (val - '0');
            input.advance();
        } 

        if (exponent) {
            if (negative_exponent) {
                exponent *= -1;
            }
            value *= std::pow(10.0, exponent);
        }
    }

    return value;
}

}

#endif
