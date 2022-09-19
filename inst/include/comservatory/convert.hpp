#ifndef COMSERVATORY_CONVERT_HPP
#define COMSERVATORY_CONVERT_HPP

#include <string>
#include <cctype>
#include <complex>
#include <cmath>
#include <array>

namespace comservatory {

template<bool escape_quote=true>
std::string to_string(const char* x, size_t n) {
    if (n < 2 || x[0] != '"' || x[n-1] != '"') {
        throw std::runtime_error("string entry must start and end with a double quote");
    }
    ++x;
    n -= 2;

    if (escape_quote) {
        std::string output;
        output.reserve(n);
        
        bool last_quote = false;
        for (size_t i = 0; i < n; ++i) {
            if (x[i] == '"') {
                if (last_quote) {
                    output += '"';
                    last_quote = false;
                } else {
                    last_quote = true;
                }
            } else {
                if (last_quote) {
                    throw std::runtime_error("all double quotes should be escaped by another double quote");
                }
                output += x[i];
            }
        }

        if (last_quote) {
            throw std::runtime_error("all double quotes should be escaped by another double quote");
        }
        return output;
    } else {
        return std::string(x, x + n);
    }
}

template<bool integer=false>
double to_number_simple(const char* x, size_t n) {
    double val = 0;
    bool is_minus = false;
    bool past_decimal = false;
    bool has_number_before_decimal = false;
    bool has_number_after_decimal = false;
    double frac = 0.1;

    for (size_t i = 0; i < n; ++i) {
        char c = x[i];
        if (i == 0) {
            if (c == '+' || c == '-') {
                is_minus = (c == '-');
                continue;
            }
        }

        if (c == '.') {
            if (integer) {
                throw std::runtime_error("detected a decimal point when only integer values are allowed");
            }
            if (!past_decimal) {
                if (!has_number_before_decimal) {
                    throw std::runtime_error("numbers should be present before the decimal point");
                }
                past_decimal = true;
            } else {
                throw std::runtime_error("multiple decimal points detected");
            }
        } else if (std::isdigit(c)) {
            double latest = c - '0'; 
            if (past_decimal) {
                has_number_after_decimal = true;
                val += frac * latest;
                frac /= 10;
            } else {
                has_number_before_decimal = true;
                val *= 10;
                val += latest;
            }
        } else {
            throw std::runtime_error("invalid character detected (" + std::string(1, x[i]) + ")");
        }
    }

    if (past_decimal && !has_number_after_decimal) {
        throw std::runtime_error("numbers should be present after the decimal point"); 
    }
    if (!past_decimal && !has_number_before_decimal) {
        throw std::runtime_error("no numbers detected in the input string");
    }

    if (is_minus) {
        val *= -1;
    }
    return val;
} 

inline std::pair<bool, double> to_number_special(const char* x, size_t n) {
    bool okay = false;
    double val = 0;

    if (n && n >= 3 && n <= 4) {
        bool negate = (x[0] == '-');
        if (negate) {
            ++x;
            --n;
        }

        if (n == 3) {
            std::array<char, 3> lowered;
            for (size_t i = 0; i < 3; ++i) {
                lowered[i] = std::tolower(x[i]);
            }

            constexpr std::array<char, 3> nan = { 'n', 'a', 'n' };
            constexpr std::array<char, 3> inf = { 'i', 'n', 'f' };

            if (lowered == nan) {
                okay = true;
                val = std::numeric_limits<double>::quiet_NaN();
            } else if (lowered == inf) {
                okay = true;
                val = std::numeric_limits<double>::infinity();
                if (negate) {
                    val *= -1;
                }
            }
        }
    }

    return std::make_pair(okay, val);
}

inline double to_number(const char* x, size_t n) {
    // First pass through to identify any 'e' values.
    int e_found = -1;
    for (size_t i = 0; i < n; ++i) {
        if (x[i] == 'e' || x[i] == 'E') {
            e_found = i;
            break;
        }
    }

    // Figuring out what to do with it.
    double val;
    if (e_found < 0) {
        auto specs = to_number_special(x, n);
        if (specs.first) {
            val = specs.second;
        } else {
            try {
                val = to_number_simple<false>(x, n);
            } catch (std::exception& e) {
                throw std::runtime_error("failed to convert string to number:\n  - " + std::string(e.what()));
            }
        }

    } else {
        double first = 0;
        try {
            first = to_number_simple<false>(x, e_found);
        } catch (std::exception& e) {
            throw std::runtime_error("failed to convert mantissa to number:\n  - " + std::string(e.what()));
        }

        double afirst = std::abs(first);
        if (afirst < 1 || afirst >= 10) {
            throw std::runtime_error("absolute value of mantissa should be within [1, 10)");
        }

        double second = 0;
        try {
            second = to_number_simple<true>(x + e_found + 1, n - e_found - 1);
        } catch (std::exception& e) {
            throw std::runtime_error("failed to convert exponent to an integer:\n  - " + std::string(e.what()));
        }

        val = first * std::pow(10.0, second); 
    }

    return val;
}

inline std::complex<double> to_complex(const char* x, size_t n) {
    // First pass through to identify the _second_ +/- sign.
    // We skip the first character, as this is either a number
    // for the real part or the first +/- sign.
    int separator = -1;
    for (size_t i = 1; i < n; ++i) {
        if (x[i] == '-' || x[i] == '+') {
            separator = i;
            break;
        }
    }

    if (n && x[n-1] != 'i') {
        throw std::runtime_error("last character of a complex number should be 'i'");
    }

    if (separator < 0) {
        throw std::runtime_error("could not find separator between the real and imaginary parts");
    }

    double real;
    try {
        real = to_number(x, separator);
    } catch (std::exception& e) {
        throw std::runtime_error("failed to parse the real part:\n  - " + std::string(e.what()));
    }

    double imag;
    try {
        imag = to_number(x + separator, n - separator - 1); // include the separator to define the sign, but ignore the trailing 'i'.
    } catch (std::exception& e) {
        throw std::runtime_error("failed to parse the imaginary part:\n  - " + std::string(e.what()));
    }

    return std::complex<double>(real, imag);
}

inline bool to_boolean(const char* x, size_t n) {
    if (n == 4 && std::tolower(x[0]) == 't' 
        && std::tolower(x[1]) == 'r' 
        && std::tolower(x[2]) == 'u'
        && std::tolower(x[3]) == 'e') {
        return true;
    }

    if (n == 5 && std::tolower(x[0]) == 'f'
        && std::tolower(x[1]) == 'a'
        && std::tolower(x[2]) == 'l'
        && std::tolower(x[3]) == 's'
        && std::tolower(x[4]) == 'e') {
        return false;
    }

    throw std::runtime_error("invalid string representation for a boolean");
    return true;
}

}

#endif
