#ifndef MILLIJSON_MILLIJSON_HPP
#define MILLIJSON_MILLIJSON_HPP

#include <memory>
#include <vector>
#include <cctype>
#include <cstdlib>
#include <string>
#include <stdexcept>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <cstdio>

/**
 * @file millijson.hpp
 * @brief Header-only library for JSON parsing.
 */

/**
 * @namespace millijson
 * @brief A lightweight header-only JSON parser.
 */

namespace millijson {

/**
 * All known JSON types.
 */
enum Type {
    NUMBER,
    STRING,
    BOOLEAN,
    NOTHING,
    ARRAY,
    OBJECT
};

/**
 * @brief Virtual base class for all JSON types.
 */
struct Base {
    /**
     * @return Type of the JSON value.
     */
    virtual Type type() const = 0;

    /**
     * @cond
     */
    virtual ~Base() {}
    /**
     * @endcond
     */

    /**
     * @return The number, if `this` points to a `Number` class.
     */
    double get_number() const;

    /**
     * @return The string, if `this` points to a `String` class.
     */ 
    const std::string& get_string() const;

    /**
     * @return The boolean, if `this` points to a `Boolean` class.
     */ 
    bool get_boolean() const;

    /**
     * @return An unordered map of key-value pairs, if `this` points to an `Object` class.
     */ 
    const std::unordered_map<std::string, std::shared_ptr<Base> >& get_object() const;

    /**
     * @return A vector of `Base` objects, if `this` points to an `Array` class.
     */ 
    const std::vector<std::shared_ptr<Base> >& get_array() const;
};

/**
 * @brief JSON number.
 */
struct Number : public Base {
    /**
     * @cond
     */
    Number(double v) : value(v) {}
    /**
     * @endcond
     */

    Type type() const { return NUMBER; }

    /**
     * Value of the number.
     */
    double value;
};

/**
 * @brief JSON string.
 */
struct String : public Base {
    /**
     * @cond
     */
    String(std::string s) : value(std::move(s)) {}
    /**
     * @endcond
     */

    Type type() const { return STRING; }

    /**
     * Value of the string.
     */
    std::string value;
};

/**
 * @brief JSON boolean.
 */
struct Boolean : public Base {
    /**
     * @cond
     */
    Boolean(bool v) : value(v) {}
    /**
     * @endcond
     */

    Type type() const { return BOOLEAN; }

    /**
     * Value of the boolean.
     */
    bool value;
};

/**
 * @brief JSON null.
 */
struct Nothing : public Base {
    Type type() const { return NOTHING; }
};

/**
 * @brief JSON array.
 */
struct Array : public Base {
    Type type() const { return ARRAY; }

    /**
     * Contents of the array.
     */
    std::vector<std::shared_ptr<Base> > values;

    /**
     * @param value Value to append to the array.
     */
    void add(std::shared_ptr<Base> value) {
        values.push_back(std::move(value));
        return;
    }
};

/**
 * @brief JSON object.
 */
struct Object : public Base {
    Type type() const { return OBJECT; }

    /**
     * Key-value pairs of the object.
     */
    std::unordered_map<std::string, std::shared_ptr<Base> > values;

    /**
     * @param key String containing the key.
     * @return Whether `key` already exists in the object.
     */
    bool has(const std::string& key) const {
        return values.find(key) != values.end();
    }

    /**
     * @param key String containing the key.
     * @param value Value to add to the array.
     */
    void add(std::string key, std::shared_ptr<Base> value) {
        values[std::move(key)] = std::move(value);
        return;
    }
};

/**
 * @cond
 */
inline double Base::get_number() const {
    return static_cast<const Number*>(this)->value;
}

inline const std::string& Base::get_string() const {
    return static_cast<const String*>(this)->value;
}

inline bool Base::get_boolean() const {
    return static_cast<const Boolean*>(this)->value;
}

inline const std::unordered_map<std::string, std::shared_ptr<Base> >& Base::get_object() const {
    return static_cast<const Object*>(this)->values;
}

inline const std::vector<std::shared_ptr<Base> >& Base::get_array() const {
    return static_cast<const Array*>(this)->values;
}

inline bool isspace(char x) {
    // Allowable whitespaces as of https://www.rfc-editor.org/rfc/rfc7159#section-2.
    return x == ' ' || x == '\n' || x == '\r' || x == '\t';
}

template<class Input>
void chomp(Input& input) {
    bool ok = input.valid();
    while (ok && isspace(input.get())) {
        ok = input.advance();
    }
    return;
}

template<class Input>
bool is_expected_string(Input& input, const std::string& expected) {
    for (auto x : expected) {
        if (!input.valid()) {
            return false;
        }
        if (input.get() != x) {
            return false;
        }
        input.advance();
    }
    return true;
}

template<class Input>
std::string extract_string(Input& input) {
    size_t start = input.position() + 1;
    input.advance(); // get past the opening quote.
    std::string output;

    while (1) {
        char next = input.get();
        switch (next) {
            case '"':
                input.advance(); // get past the closing quote.
                return output;
            case '\\':
                if (!input.advance()) {
                    throw std::runtime_error("unterminated string at position " + std::to_string(start));
                } else {
                    char next2 = input.get();
                    switch (next2) {
                        case '"':
                            output += '"';          
                            break;
                        case 'n':
                            output += '\n';
                            break;
                        case 'r':
                            output += '\r';
                            break;
                        case '\\':
                            output += '\\';
                            break;
                        case '/':
                            output += '/';
                            break;
                        case 'b':
                            output += '\b';
                            break;
                        case 'f':
                            output += '\f';
                            break;
                        case 't':
                            output += '\t';
                            break;
                        case 'u':
                            {
                                unsigned short mb = 0;
                                for (size_t i = 0; i < 4; ++i) {
                                    if (!input.advance()){
                                        throw std::runtime_error("unterminated string at position " + std::to_string(start));
                                    }
                                    mb *= 16;
                                    char val = input.get();
                                    switch (val) {
                                        case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
                                            mb += val - '0';
                                            break;
                                        case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': 
                                            mb += (val - 'a') + 10;
                                            break;
                                        case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': 
                                            mb += (val - 'A') + 10;
                                            break;
                                        default:
                                            throw std::runtime_error("invalid unicode escape detected at position " + std::to_string(input.position() + 1));
                                    }
                                }

                                // Converting manually from UTF-16 to UTF-8. We only allow
                                // 3 bytes at most because there's only 4 hex digits in JSON. 
                                if (mb <= 127) {
                                    output += static_cast<char>(mb);
                                } else if (mb <= 2047) {
                                    unsigned char left = (mb >> 6) | 0b11000000;
                                    output += *(reinterpret_cast<char*>(&left));
                                    unsigned char right = (mb & 0b00111111) | 0b10000000;
                                    output += *(reinterpret_cast<char*>(&right));
                                } else {
                                    unsigned char left = (mb >> 12) | 0b11100000;
                                    output += *(reinterpret_cast<char*>(&left));
                                    unsigned char middle = ((mb >> 6) & 0b00111111) | 0b10000000;
                                    output += *(reinterpret_cast<char*>(&middle));
                                    unsigned char right = (mb & 0b00111111) | 0b10000000;
                                    output += *(reinterpret_cast<char*>(&right));
                                }
                            }
                            break;
                        default:
                            throw std::runtime_error("unrecognized escape '\\" + std::string(1, next2) + "'");
                    }
                }
                break;
            case (char) 0: case (char) 1: case (char) 2: case (char) 3: case (char) 4: case (char) 5: case (char) 6: case (char) 7: case (char) 8: case (char) 9:
            case (char)10: case (char)11: case (char)12: case (char)13: case (char)14: case (char)15: case (char)16: case (char)17: case (char)18: case (char)19:
            case (char)20: case (char)21: case (char)22: case (char)23: case (char)24: case (char)25: case (char)26: case (char)27: case (char)28: case (char)29:
            case (char)30: case (char)31:
                throw std::runtime_error("string contains ASCII control character at position " + std::to_string(input.position() + 1));
            default:
                output += next;
                break;
        }

        if (!input.advance()) {
            throw std::runtime_error("unterminated string at position " + std::to_string(start));
        }
    }

    return output; // Technically unreachable, but whatever.
}

template<class Input>
double extract_number(Input& input) {
    size_t start = input.position() + 1;
    double value = 0;
    double fractional = 10;
    double exponent = 0; 
    bool negative_exponent = false;

    auto is_terminator = [](char v) -> bool {
        return v == ',' || v == ']' || v == '}' || isspace(v);
    };

    bool in_fraction = false;
    bool in_exponent = false;

    // We assume we're starting from the absolute value, after removing any preceding negative sign. 
    char lead = input.get();
    if (lead == '0') {
        if (!input.advance()) {
            return 0;
        }

        char val = input.get();
        if (val == '.') {
            in_fraction = true;
        } else if (val == 'e' || val == 'E') {
            in_exponent = true;
        } else if (is_terminator(val)) {
            return value;
        } else {
            throw std::runtime_error("invalid number starting with 0 at position " + std::to_string(start));
        }

    } else if (std::isdigit(lead)) {
        value += lead - '0';

        while (input.advance()) {
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
                throw std::runtime_error("invalid number containing '" + std::string(1, val) + "' at position " + std::to_string(start));
            }
            value *= 10;
            value += val - '0';
        }

    } else {
        // this should never happen, as extract_number is only called when the lead is a digit (or '-').
    }

    if (in_fraction) {
        if (!input.advance()) {
            throw std::runtime_error("invalid number with trailing '.' at position " + std::to_string(start));
        }

        char val = input.get();
        if (!std::isdigit(val)) {
            throw std::runtime_error("'.' must be followed by at least one digit at position " + std::to_string(start));
        }
        value += (val - '0') / fractional;

        while (input.advance()) {
            char val = input.get();
            if (val == 'e' || val == 'E') {
                in_exponent = true;
                break;
            } else if (is_terminator(val)) {
                return value;
            } else if (!std::isdigit(val)) {
                throw std::runtime_error("invalid number containing '" + std::string(1, val) + "' at position " + std::to_string(start));
            }
            fractional *= 10;
            value += (val - '0') / fractional;
        } 
    }

    if (in_exponent) {
        if (!input.advance()) {
            throw std::runtime_error("invalid number with trailing 'e/E' at position " + std::to_string(start));
        }

        char val = input.get();
        if (!std::isdigit(val)) {
            if (val == '-') {
                negative_exponent = true;
            } else if (val != '+') {
                throw std::runtime_error("'e/E' should be followed by a sign or digit in number at position " + std::to_string(start));
            }

            if (!input.advance()) {
                throw std::runtime_error("invalid number with trailing exponent sign at position " + std::to_string(start));
            }
            val = input.get();
            if (!std::isdigit(val)) {
                throw std::runtime_error("exponent sign must be followed by at least one digit in number at position " + std::to_string(start));
            }
        }

        exponent += (val - '0');

        while (input.advance()) {
            char val = input.get();
            if (is_terminator(val)) {
                break;
            } else if (!std::isdigit(val)) {
                throw std::runtime_error("invalid number containing '" + std::string(1, val) + "' at position " + std::to_string(start));
            }
            exponent *= 10;
            exponent += (val - '0');
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

struct DefaultProvisioner {
    typedef ::millijson::Base base;

    static Boolean* new_boolean(bool x) {
        return new Boolean(x); 
    }

    static Number* new_number(double x) {
        return new Number(x);
    }

    static String* new_string(std::string x) {
        return new String(std::move(x));
    }

    static Nothing* new_nothing() {
        return new Nothing;
    }

    static Array* new_array() {
        return new Array;
    }

    static Object* new_object() {
        return new Object;
    }
};

struct FakeProvisioner {
    struct FakeBase {
        virtual Type type() const = 0;
        virtual ~FakeBase() {}
    };
    typedef FakeBase base;

    struct FakeBoolean : public FakeBase {
        Type type() const { return BOOLEAN; }
    };
    static FakeBoolean* new_boolean(bool) {
        return new FakeBoolean; 
    }

    struct FakeNumber : public FakeBase {
        Type type() const { return NUMBER; }
    };
    static FakeNumber* new_number(double) {
        return new FakeNumber;
    }

    struct FakeString : public FakeBase {
        Type type() const { return STRING; }
    };
    static FakeString* new_string(std::string) {
        return new FakeString;
    }

    struct FakeNothing : public FakeBase {
        Type type() const { return NOTHING; }
    };
    static FakeNothing* new_nothing() {
        return new FakeNothing;
    }

    struct FakeArray : public FakeBase {
        Type type() const { return ARRAY; }
        void add(std::shared_ptr<FakeBase>) {}
    };
    static FakeArray* new_array() {
        return new FakeArray;
    }

    struct FakeObject : public FakeBase {
        Type type() const { return OBJECT; }
        std::unordered_set<std::string> keys;
        bool has(const std::string& key) const {
            return keys.find(key) != keys.end();
        }
        void add(std::string key, std::shared_ptr<FakeBase>) {
            keys.insert(std::move(key));
        }
    };
    static FakeObject* new_object() {
        return new FakeObject;
    }
};

template<class Provisioner, class Input>
std::shared_ptr<typename Provisioner::base> parse_thing(Input& input) {
    std::shared_ptr<typename Provisioner::base> output;

    size_t start = input.position() + 1;
    const char current = input.get();

    if (current == 't') {
        if (!is_expected_string(input, "true")) {
            throw std::runtime_error("expected a 'true' string at position " + std::to_string(start));
        }
        output.reset(Provisioner::new_boolean(true));

    } else if (current == 'f') {
        if (!is_expected_string(input, "false")) {
            throw std::runtime_error("expected a 'false' string at position " + std::to_string(start));
        }
        output.reset(Provisioner::new_boolean(false));

    } else if (current == 'n') {
        if (!is_expected_string(input, "null")) {
            throw std::runtime_error("expected a 'null' string at position " + std::to_string(start));
        }
        output.reset(Provisioner::new_nothing());

    } else if (current == '"') {
        output.reset(Provisioner::new_string(extract_string(input)));

    } else if (current == '[') {
        auto ptr = Provisioner::new_array();
        output.reset(ptr);

        input.advance();
        chomp(input);
        if (!input.valid()) {
            throw std::runtime_error("unterminated array starting at position " + std::to_string(start));
        }

        if (input.get() != ']') {
            while (1) {
                ptr->add(parse_thing<Provisioner>(input));

                chomp(input);
                if (!input.valid()) {
                    throw std::runtime_error("unterminated array starting at position " + std::to_string(start));
                }

                char next = input.get();
                if (next == ']') {
                    break;
                } else if (next != ',') {
                    throw std::runtime_error("unknown character '" + std::string(1, next) + "' in array at position " + std::to_string(input.position() + 1));
                }

                input.advance(); 
                chomp(input);
                if (!input.valid()) {
                    throw std::runtime_error("unterminated array starting at position " + std::to_string(start));
                }
            }
        }

        input.advance(); // skip the closing bracket.

    } else if (current == '{') {
        auto ptr = Provisioner::new_object();
        output.reset(ptr);

        input.advance();
        chomp(input);
        if (!input.valid()) {
            throw std::runtime_error("unterminated object starting at position " + std::to_string(start));
        }

        if (input.get() != '}') {
            while (1) {
                char next = input.get();
                if (next != '"') {
                    throw std::runtime_error("expected a string as the object key at position " + std::to_string(input.position() + 1));
                }
                auto key = extract_string(input);
                if (ptr->has(key)) {
                    throw std::runtime_error("detected duplicate keys in the object at position " + std::to_string(input.position() + 1));
                }

                chomp(input);
                if (!input.valid()) {
                    throw std::runtime_error("unterminated object starting at position " + std::to_string(start));
                }
                if (input.get() != ':') {
                    throw std::runtime_error("expected ':' to separate keys and values at position " + std::to_string(input.position() + 1));
                }

                input.advance();
                chomp(input);
                if (!input.valid()) {
                    throw std::runtime_error("unterminated object starting at position " + std::to_string(start));
                }
                ptr->add(std::move(key), parse_thing<Provisioner>(input)); // consuming the key here.

                chomp(input);
                if (!input.valid()) {
                    throw std::runtime_error("unterminated object starting at position " + std::to_string(start));
                }

                next = input.get();
                if (next == '}') {
                    break;
                } else if (next != ',') {
                    throw std::runtime_error("unknown character '" + std::string(1, next) + "' in array at position " + std::to_string(input.position() + 1));
                }

                input.advance(); 
                chomp(input);
                if (!input.valid()) {
                    throw std::runtime_error("unterminated object starting at position " + std::to_string(start));
                }
            }
        }

        input.advance(); // skip the closing brace.

    } else if (current == '-') {
        if (!input.advance()) {
            throw std::runtime_error("incomplete number starting at position " + std::to_string(start));
        }
        output.reset(Provisioner::new_number(-extract_number(input)));

    } else if (std::isdigit(current)) {
        output.reset(Provisioner::new_number(extract_number(input)));

    } else {
        throw std::runtime_error(std::string("unknown type starting with '") + std::string(1, current) + "' at position " + std::to_string(start));
    }

    return output;
}

template<class Provisioner, class Input>
std::shared_ptr<typename Provisioner::base> parse_thing_with_chomp(Input& input) {
    chomp(input);
    auto output = parse_thing<Provisioner>(input);
    chomp(input);
    if (input.valid()) {
        throw std::runtime_error("invalid json with trailing non-space characters at position " + std::to_string(input.position() + 1));
    }
    return output;
}
/**
 * @endcond
 */

/**
 * @tparam Input Any class that provides the following methods:
 *
 * - `char get() const `, which extracts a `char` from the input source without advancing the position on the byte stream.
 * - `bool valid() const`, to determine whether an input `char` can be `get()` from the input.
 * - `bool advance()`, to advance the input stream and return `valid()` at the new position.
 * - `size_t position() const`, for the current position relative to the start of the byte stream.
 *
 * @param input An instance of an `Input` class, referring to the bytes from a JSON-formatted file or string.
 * @return A pointer to a JSON value.
 */
template<class Input>
std::shared_ptr<Base> parse(Input& input) {
    return parse_thing_with_chomp<DefaultProvisioner>(input);
}

/**
 * @tparam Input Any class that supplies input characters, see `parse()` for details. 
 *
 * @param input An instance of an `Input` class, referring to the bytes from a JSON-formatted file or string.
 *
 * @return The type of the JSON variable stored in `input`.
 * If the JSON string is invalid, an error is raised.
 */
template<class Input>
Type validate(Input& input) {
    auto ptr = parse_thing_with_chomp<FakeProvisioner>(input);
    return ptr->type();
}

/**
 * @cond
 */
struct RawReader {
    RawReader(const char* p, size_t n) : ptr_(p), len_(n) {}
    size_t pos_ = 0;
    const char * ptr_;
    size_t len_;

    char get() const {
        return ptr_[pos_];
    }

    bool valid() const {
        return pos_ < len_;
    }

    bool advance() {
        ++pos_;
        return valid();
    }

    size_t position() const {
        return pos_;
    }
};
/**
 * @endcond
 */

/**
 * @param[in] ptr Pointer to an array containing a JSON string.
 * @param len Length of the array.
 * @return A pointer to a JSON value.
 */
inline std::shared_ptr<Base> parse_string(const char* ptr, size_t len) {
    RawReader input(ptr, len);
    return parse(input);
}

/**
 * @param[in] ptr Pointer to an array containing a JSON string.
 * @param len Length of the array.
 *
 * @return The type of the JSON variable stored in the string.
 * If the JSON string is invalid, an error is raised.
 */
inline Type validate_string(const char* ptr, size_t len) {
    RawReader input(ptr, len);
    return validate(input);
}

/**
 * @cond
 */
struct FileReader{
    FileReader(const char* p, size_t b) : handle(std::fopen(p, "rb")), buffer(b) {
        if (!handle) {
            throw std::runtime_error("failed to open file at '" + std::string(p) + "'");
        }
        fill();
    }

    ~FileReader() {
        std::fclose(handle);
    }

    FILE* handle;
    std::vector<char> buffer;
    size_t available = 0;
    size_t index = 0;
    size_t overall = 0;
    bool finished = false;

    char get() const {
        return buffer[index];
    }

    bool valid() const {
        return index < available;
    }

    bool advance() {
        ++index;
        if (index < available) {
            return true;
        }

        index = 0;
        overall += available;
        fill();
        return valid();
    }

    void fill() {
        if (finished) {
            available = 0;
            return;
        }

        available = std::fread(buffer.data(), sizeof(char), buffer.size(), handle);
        if (available == buffer.size()) {
            return;
        }

        if (std::feof(handle)) {
            finished = true;
        } else {
            throw std::runtime_error("failed to read file (error " + std::to_string(std::ferror(handle)) + ")");
        }
    }

    size_t position() const {
        return overall + index;
    }
};
/**
 * @endcond
 */

/**
 * @param[in] path Pointer to an array containing a path to a JSON file.
 * @param buffer_size Size of the buffer to use for reading the file.
 * @return A pointer to a JSON value.
 */
inline std::shared_ptr<Base> parse_file(const char* path, size_t buffer_size = 65536) {
    FileReader input(path, buffer_size);
    return parse(input);
}

/**
 * @param[in] path Pointer to an array containing a path to a JSON file.
 * @param buffer_size Size of the buffer to use for reading the file.
 *
 * @return The type of the JSON variable stored in the file.
 * If the JSON file is invalid, an error is raised.
 */
inline Type validate_file(const char* path, size_t buffer_size = 65536) {
    FileReader input(path, buffer_size);
    return validate(input);
}

}

#endif
