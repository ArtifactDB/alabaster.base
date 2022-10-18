#ifndef UZUKI2_PARSE_JSON_HPP
#define UZUKI2_PARSE_JSON_HPP

#include <memory>
#include <vector>
#include <cctype>
#include <string>
#include <stdexcept>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

#include "byteme/SomeFileReader.hpp"
#include "byteme/SomeBufferReader.hpp"

#include "interfaces.hpp"
#include "Dummy.hpp"
#include "utils.hpp"

/**
 * @file parse_json.hpp
 * @brief Parsing methods for JSON files.
 */

namespace uzuki2 {

/**
 * @cond
 */
namespace json {

namespace raw {

enum JsonType {
    NUMBER,
    STRING,
    BOOLEAN,
    NOTHING,
    ARRAY,
    OBJECT
};

struct Base {
    virtual JsonType type() const = 0;
    virtual ~Base() {}
};

struct Number : public Base {
    Number(double v) : value(v) {}
    double value;
    JsonType type() const { return NUMBER; }
};

struct String : public Base {
    String(std::string s) : value(std::move(s)) {}
    std::string value;
    JsonType type() const { return STRING; }
};

struct Boolean : public Base {
    Boolean(bool v) : value(v) {}
    bool value;
    JsonType type() const { return BOOLEAN; }
};

struct Nothing : public Base {
    JsonType type() const { return NOTHING; }
};

struct Array : public Base {
    std::vector<std::shared_ptr<Base> > values;
    JsonType type() const { return ARRAY; }
};

struct Object : public Base {
    std::unordered_map<std::string, std::shared_ptr<Base> > values;
    JsonType type() const { return OBJECT; }
};

struct CurrentBuffer {
private:
    const char* ptr = nullptr;
    size_t available = 0;
    size_t current = 0;
    bool remaining = false;
    size_t overall = 0;

    byteme::Reader* reader = nullptr;

    void refill() {
        remaining = (*reader)();
        ptr = reinterpret_cast<const char*>(reader->buffer());
        available = reader->available();
        current = 0;
    }
public:
    template<class Reader>
    CurrentBuffer(Reader& r) : reader(&r) {
        refill();
    }

    bool valid() const {
        return current < available;
    }

    void advance() {
        ++current;
        if (current == available) {
            overall += available;
            if (remaining) {
                refill();
            }
        }
    }

    char get() const {
        return ptr[current];
    }

    size_t position() const {
        return overall + current;
    }
};

inline void chomp(CurrentBuffer& buffer) {
    while (buffer.valid() && std::isspace(buffer.get())) {
        buffer.advance();
    }
    return;
}

inline bool is_expected_string(CurrentBuffer& buffer, const std::string& expected) {
    for (auto x : expected) {
        if (!buffer.valid()) {
            return false;
        }
        if (buffer.get() != x) {
            return false;
        }
        buffer.advance();
    }
    return true;
}

inline std::string extract_string(CurrentBuffer& buffer) {
    size_t start = buffer.position() + 1;
    buffer.advance(); // get past the opening quote.
    std::string output;

    while (1) {
        char next = buffer.get();
        switch (next) {
            case '"':
                buffer.advance(); // get past the closing quote.
                return output;
            case '\\':
                buffer.advance();
                if (!buffer.valid()) {
                    throw std::runtime_error("unterminated string at position " + std::to_string(start));
                } else {
                    char next2 = buffer.get();
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
                        case 'b':
                            output += '\b';
                            break;
                        case 'f':
                            output += '\f';
                            break;
                        case 't':
                            output += '\t';
                            break;
                        // TODO: \u unicode?
                        default:
                            throw std::runtime_error("unrecognized escape '\\" + std::string(1, next2) + "'");
                    }
                }
                break;
            default:
                output += next;
                break;
        }

        buffer.advance();
        if (!buffer.valid()) {
            throw std::runtime_error("unterminated string at position " + std::to_string(start));
        }
    }

    return output; // Technically unreachable, but whatever.
}

inline double extract_number(CurrentBuffer& buffer) {
    size_t start = buffer.position() + 1;
    double value = 0;
    double fractional = 10;
    double exponent = 0; 
    bool negative_exponent = false;

    auto is_terminator = [](char v) -> bool {
        return v == ',' || v == ']' || v == '}' || std::isspace(v);
    };

    auto finalizer = [&]() -> double {
        if (exponent) {
            if (negative_exponent) {
                exponent *= -1;
            }
            return value * std::pow(10.0, exponent);
        } else {
            return value;
        }
    };

    bool in_fraction = false;
    bool in_exponent = false;

    // We assume we're starting from the absolute value, after removing any preceding negative sign. 
    char lead = buffer.get();
    if (lead == '0') {
        buffer.advance();
        if (!buffer.valid()) {
            return 0;
        }

        char val = buffer.get();
        if (val == '.') {
            in_fraction = true;
        } else if (val == 'e' || val == 'E') {
            in_exponent = true;
        } else if (is_terminator(val)) {
            return finalizer();
        } else {
            throw std::runtime_error("invalid number starting with 0 at position " + std::to_string(start));
        }

    } else if (std::isdigit(lead)) {
        value += lead - '0';
        buffer.advance();

        while (buffer.valid()) {
            char val = buffer.get();
            if (val == '.') {
                in_fraction = true;
                break;
            } else if (val == 'e' || val == 'E') {
                in_exponent = true;
                break;
            } else if (is_terminator(val)) {
                return finalizer();
            } else if (!std::isdigit(val)) {
                throw std::runtime_error("invalid number containing '" + std::string(1, val) + "' at position " + std::to_string(start));
            }
            value *= 10;
            value += val - '0';
            buffer.advance();
        }

    } else {
        // this should never happen, as extract_number is only called when the lead is a digit (or '-').
    }

    if (in_fraction) {
        buffer.advance();
        if (!buffer.valid()) {
            throw std::runtime_error("invalid number with trailing '.' at position " + std::to_string(start));
        }

        char val = buffer.get();
        if (!std::isdigit(val)) {
            throw std::runtime_error("'.' must be followed by at least one digit at position " + std::to_string(start));
        }
        value += (val - '0') / fractional;

        buffer.advance();
        while (buffer.valid()) {
            char val = buffer.get();
            if (val == 'e' || val == 'E') {
                in_exponent = true;
                break;
            } else if (is_terminator(val)) {
                return finalizer();
            } else if (!std::isdigit(val)) {
                throw std::runtime_error("invalid number containing '" + std::string(1, val) + "' at position " + std::to_string(start));
            }
            fractional *= 10;
            value += (val - '0') / fractional;
            buffer.advance();
        } 
    }

    if (in_exponent) {
        buffer.advance();
        if (!buffer.valid()) {
            throw std::runtime_error("invalid number with trailing 'e/E' at position " + std::to_string(start));
        }

        char val = buffer.get();
        if (val == '+') {
            ;
        } else if (val == '-') {
            negative_exponent = true;
        } else {
            throw std::runtime_error("'e/E' must be followed by a sign in number at position " + std::to_string(start));
        }

        buffer.advance();
        if (!buffer.valid()) {
            throw std::runtime_error("invalid number with trailing exponent sign in number at position " + std::to_string(start));
        }
        val = buffer.get();
        if (!std::isdigit(val)) {
            throw std::runtime_error("exponent sign must be followed by at least one digit in number at position " + std::to_string(start));
        }
        exponent += (val - '0');

        buffer.advance();
        while (buffer.valid()) {
            char val = buffer.get();
            if (is_terminator(val)) {
                return finalizer();
            } else if (!std::isdigit(val)) {
                throw std::runtime_error("invalid number containing '" + std::string(1, val) + "' at position " + std::to_string(start));
            }
            exponent *= 10;
            exponent += (val - '0');
            buffer.advance();
        } 
    }

    return finalizer();
}

inline std::shared_ptr<Base> parse_thing(CurrentBuffer& buffer) {
    std::shared_ptr<Base> output;

    size_t start = buffer.position() + 1;
    const char current = buffer.get();

    if (current == 't') {
        if (!is_expected_string(buffer, "true")) {
            throw std::runtime_error("expected a 'true' string at position " + std::to_string(start));
        }
        output.reset(new Boolean(true));

    } else if (current == 'f') {
        if (!is_expected_string(buffer, "false")) {
            throw std::runtime_error("expected a 'false' string at position " + std::to_string(start));
        }
        output.reset(new Boolean(false));

    } else if (current == 'n') {
        if (!is_expected_string(buffer, "null")) {
            throw std::runtime_error("expected a 'null' string at position " + std::to_string(start));
        }
        output.reset(new Nothing);

    } else if (current == '"') {
        output.reset(new String(extract_string(buffer)));

    } else if (current == '[') {
        auto ptr = new Array;
        output.reset(ptr);

        buffer.advance();
        chomp(buffer);
        if (!buffer.valid()) {
            throw std::runtime_error("unterminated array starting at position " + std::to_string(start));
        }

        if (buffer.get() != ']') {
            while (1) {
                ptr->values.push_back(parse_thing(buffer));

                chomp(buffer);
                if (!buffer.valid()) {
                    throw std::runtime_error("unterminated array starting at position " + std::to_string(start));
                }

                char next = buffer.get();
                if (next == ']') {
                    break;
                } else if (next != ',') {
                    throw std::runtime_error("unknown character '" + std::string(1, next) + "' in array at position " + std::to_string(buffer.position() + 1));
                }

                buffer.advance(); 
                chomp(buffer);
                if (!buffer.valid()) {
                    throw std::runtime_error("unterminated array starting at position " + std::to_string(start));
                }
            }
        }

        buffer.advance(); // skip the closing bracket.

    } else if (current == '{') {
        auto ptr = new Object;
        output.reset(ptr);
        auto& map = ptr->values;

        buffer.advance();
        chomp(buffer);
        if (!buffer.valid()) {
            throw std::runtime_error("unterminated object starting at position " + std::to_string(start));
        }

        if (buffer.get() != '}') {
            while (1) {
                char next = buffer.get();
                if (next != '"') {
                    throw std::runtime_error("expected a string as the object key at position " + std::to_string(buffer.position() + 1));
                }
                auto key = extract_string(buffer);
                if (map.find(key) != map.end()) {
                    throw std::runtime_error("detected duplicate keys in the object at position " + std::to_string(buffer.position() + 1));
                }

                chomp(buffer);
                if (!buffer.valid()) {
                    throw std::runtime_error("unterminated object starting at position " + std::to_string(start));
                }
                if (buffer.get() != ':') {
                    throw std::runtime_error("expected ':' to separate keys and values at position " + std::to_string(buffer.position() + 1));
                }

                buffer.advance();
                chomp(buffer);
                if (!buffer.valid()) {
                    throw std::runtime_error("unterminated object starting at position " + std::to_string(start));
                }
                map[key] = parse_thing(buffer);

                chomp(buffer);
                if (!buffer.valid()) {
                    throw std::runtime_error("unterminated object starting at position " + std::to_string(start));
                }

                next = buffer.get();
                if (next == '}') {
                    break;
                } else if (next != ',') {
                    throw std::runtime_error("unknown character '" + std::string(1, next) + "' in array at position " + std::to_string(buffer.position() + 1));
                }

                buffer.advance(); 
                chomp(buffer);
                if (!buffer.valid()) {
                    throw std::runtime_error("unterminated object starting at position " + std::to_string(start));
                }
            }
        }

        buffer.advance(); // skip the closing brace.

    } else if (current == '-') {
        buffer.advance(); 
        if (!buffer.valid()) {
            throw std::runtime_error("incomplete number starting at position " + std::to_string(start));
        }
        output.reset(new Number(-extract_number(buffer)));

    } else if (std::isdigit(current)) {
        output.reset(new Number(extract_number(buffer)));

    } else {
        throw std::runtime_error(std::string("unknown type starting with '") + std::string(1, current) + "' at position " + std::to_string(start));
    }

    return output;
}

inline std::shared_ptr<Base> parse_json(byteme::Reader& reader) {
    CurrentBuffer buffer(reader);
    chomp(buffer);
    auto output = parse_thing(buffer);
    chomp(buffer);
    if (buffer.valid()) {
        throw std::runtime_error("invalid json with trailing non-space characters at position " + std::to_string(buffer.position() + 1));
    }
    return output;
}

}

inline const std::vector<std::shared_ptr<raw::Base> >& extract_array(const std::unordered_map<std::string, std::shared_ptr<raw::Base> >& properties, const std::string& name, const std::string& path) {
    auto vIt = properties.find(name);
    if (vIt == properties.end()) {
        throw std::runtime_error("expected '" + name + "' property for object at '" + path + "'");
    }

    const auto& values_ptr = vIt->second;
    if (values_ptr->type() != raw::ARRAY) {
        throw std::runtime_error("expected an array in '" + path + "." + name + "'"); 
    }

    return static_cast<const raw::Array*>(values_ptr.get())->values;
}

template<class Destination, class Function>
void extract_integers(const std::vector<std::shared_ptr<raw::Base> >& values, Destination* dest, Function check, const std::string& path) {
    for (size_t i = 0; i < values.size(); ++i) {
        if (values[i]->type() == raw::NOTHING) {
            dest->set_missing(i);
            continue;
        }

        if (values[i]->type() != raw::NUMBER) {
            throw std::runtime_error("expected a number at '" + path + ".values[" + std::to_string(i) + "]'");
        }

        auto val = static_cast<const raw::Number*>(values[i].get())->value;
        if (val != std::floor(val)) {
            throw std::runtime_error("expected an integer at '" + path + ".values[" + std::to_string(i) + "]'");
        }

        constexpr double upper = std::numeric_limits<int32_t>::max();
        constexpr double lower = std::numeric_limits<int32_t>::min();
        if (val < lower || val > upper) {
            throw std::runtime_error("value at '" + path + ".values[" + std::to_string(i) + "]' cannot be represented by a 32-bit signed integer");
        }

        int32_t ival = val;
        check(ival);
        dest->set(i, ival);
    }
}

template<class Destination, class Function>
void extract_strings(const std::vector<std::shared_ptr<raw::Base> >& values, Destination* dest, Function check, const std::string& path) {
    for (size_t i = 0; i < values.size(); ++i) {
        if (values[i]->type() == raw::NOTHING) {
            dest->set_missing(i);
            continue;
        }

        if (values[i]->type() != raw::STRING) {
            throw std::runtime_error("expected a string at '" + path + ".values[" + std::to_string(i) + "]'");
        }

        const auto& str = static_cast<const raw::String*>(values[i].get())->value;
        check(str);
        dest->set(i, str);
    }
}

template<class Destination>
void extract_names(const std::unordered_map<std::string, std::shared_ptr<raw::Base> >& properties, Destination* dest, const std::string& path) {
    auto nIt = properties.find("names");
    if (nIt == properties.end()) {
        return;
    }

    const auto name_ptr = nIt->second;
    if (name_ptr->type() != raw::ARRAY) {
        throw std::runtime_error("expected an array in '" + path + ".names'"); 
    }

    const auto& names = static_cast<const raw::Array*>(name_ptr.get())->values;
    if (names.size() != dest->size()) {
        throw std::runtime_error("length of 'names' and 'values' should be the same in '" + path + "'"); 
    }
    dest->use_names();

    for (size_t i = 0; i < names.size(); ++i) {
        if (names[i]->type() != raw::STRING) {
            throw std::runtime_error("expected a string at '" + path + ".names[" + std::to_string(i) + "]'");
        }
        dest->set_name(i, static_cast<const raw::String*>(names[i].get())->value);
    }
}

template<class Provisioner, class Externals>
std::shared_ptr<Base> parse_object(const raw::Base* contents, Externals& ext, const std::string& path) {
    if (contents->type() != raw::OBJECT) {
        throw std::runtime_error("each R object should be represented by a JSON object at '" + path + "'");
    }

    auto optr = static_cast<const raw::Object*>(contents);
    const auto& map = optr->values;

    auto tIt = map.find("type");
    if (tIt == map.end()) {
        throw std::runtime_error("missing 'type' property for JSON object at '" + path + "'");
    }
    const auto& type_ptr = tIt->second;
    if (type_ptr->type() != raw::STRING) {
        throw std::runtime_error("expected a string at '" + path + ".type'");
    }
    const auto& type = static_cast<const raw::String*>(type_ptr.get())->value;

    std::shared_ptr<Base> output;
    if (type == "nothing") {
        output.reset(Provisioner::new_Nothing());

    } else if (type == "external") {
        auto iIt = map.find("index");
        if (iIt == map.end()) {
            throw std::runtime_error("expected 'index' property for 'external' type at '" + path + "'");
        }
        const auto& index_ptr = iIt->second;
        if (index_ptr->type() != raw::NUMBER) {
            throw std::runtime_error("expected a number at '" + path + ".index'");
        }
        auto index = static_cast<const raw::Number*>(index_ptr.get())->value;

        if (index != std::floor(index)) {
            throw std::runtime_error("expected an integer at '" + path + ".index'");
        } else if (index < 0 || index >= static_cast<double>(ext.size())) {
            throw std::runtime_error("external index out of range at '" + path + ".index'");
        }
        output.reset(Provisioner::new_External(ext.get(index)));

    } else if (type == "integer") {
        const auto& vals = extract_array(map, "values", path);
        auto ptr = Provisioner::new_Integer(vals.size());
        output.reset(ptr);
        extract_integers(vals, ptr, [](int32_t) -> void {}, path);
        extract_names(map, ptr, path);

    } else if (type == "factor" || type == "ordered") {
        const auto& vals = extract_array(map, "values", path);
        const auto& lvals = extract_array(map, "levels", path);

        auto ptr = Provisioner::new_Factor(vals.size(), lvals.size());
        output.reset(ptr);
        if (type == "ordered") {
            ptr->is_ordered();
        }

        int32_t nlevels = lvals.size();
        extract_integers(vals, ptr, [&](int32_t x) -> void {
            if (x < 0 || x >= nlevels) {
                throw std::runtime_error("factor indices of out of range of levels in '" + path + "'");
            }
        }, path);

        std::unordered_set<std::string> existing;
        for (size_t l = 0; l < lvals.size(); ++l) {
            if (lvals[l]->type() != raw::STRING) {
                throw std::runtime_error("expected strings at '" + path + ".levels[" + std::to_string(l) + "]'");
            }

            const auto& level = static_cast<const raw::String*>(lvals[l].get())->value;
            if (existing.find(level) != existing.end()) {
                throw std::runtime_error("detected duplicate string at '" + path + ".levels[" + std::to_string(l) + "]'");
            }
            ptr->set_level(l, level);
            existing.insert(level);
        }

        extract_names(map, ptr, path);

    } else if (type == "boolean") {
        const auto& vals = extract_array(map, "values", path);
        auto ptr = Provisioner::new_Boolean(vals.size());
        output.reset(ptr);

        for (size_t i = 0; i < vals.size(); ++i) {
            if (vals[i]->type() == raw::NOTHING) {
                ptr->set_missing(i);
                continue;
            }

            if (vals[i]->type() != raw::BOOLEAN) {
                throw std::runtime_error("expected a boolean at '" + path + ".values[" + std::to_string(i) + "]'");
            }
            ptr->set(i, static_cast<const raw::Boolean*>(vals[i].get())->value);
        }

        extract_names(map, ptr, path);

    } else if (type == "number") {
        const auto& vals = extract_array(map, "values", path);
        auto ptr = Provisioner::new_Number(vals.size());
        output.reset(ptr);

        for (size_t i = 0; i < vals.size(); ++i) {
            if (vals[i]->type() == raw::NOTHING) {
                ptr->set_missing(i);
                continue;
            }

            if (vals[i]->type() == raw::NUMBER) {
                ptr->set(i, static_cast<const raw::Number*>(vals[i].get())->value);
            } else if (vals[i]->type() == raw::STRING) {
                auto str = static_cast<const raw::String*>(vals[i].get())->value;
                if (str == "NaN") {
                    ptr->set(i, std::numeric_limits<double>::quiet_NaN());
                } else if (str == "Inf") {
                    ptr->set(i, std::numeric_limits<double>::infinity());
                } else if (str == "-Inf") {
                    ptr->set(i, -std::numeric_limits<double>::infinity());
                } else {
                    throw std::runtime_error("unsupported string '" + str + "' at '" + path + ".values[" + std::to_string(i) + "]'");
                }
            } else {
                throw std::runtime_error("expected a number at '" + path + ".values[" + std::to_string(i) + "]'");
            }
        }

        extract_names(map, ptr, path);

    } else if (type == "string") {
        const auto& vals = extract_array(map, "values", path);
        auto ptr = Provisioner::new_String(vals.size());
        output.reset(ptr);
        extract_strings(vals, ptr, [](const std::string&) -> void {}, path);
        extract_names(map, ptr, path);

    } else if (type == "date") {
        const auto& vals = extract_array(map, "values", path);
        auto ptr = Provisioner::new_Date(vals.size());
        output.reset(ptr);
        extract_strings(vals, ptr, [&](const std::string& x) -> void {
            if (!is_date(x)) {
                 throw std::runtime_error("dates should follow YYYY-MM-DD formatting in '" + path + ".values'");
            }
        }, path);
        extract_names(map, ptr, path);

    } else if (type == "list") {
        const auto& vals = extract_array(map, "values", path);
        auto ptr = Provisioner::new_List(vals.size());
        output.reset(ptr);
        for (size_t i = 0; i < vals.size(); ++i) {
            ptr->set(i, parse_object<Provisioner>(vals[i].get(), ext, path + ".values[" + std::to_string(i) + "]"));
        }
        extract_names(map, ptr, path);

    } else {
        throw std::runtime_error("unknown object type '" + type + "' at '" + path + ".type'");
    }

    return output;
}

}
/**
 * @endcond
 */

/**
 * Parse JSON file contents using the **uzuki2** specification.
 *
 * @tparam Provisioner A class namespace defining static methods for creating new `Base` objects.
 * See `parse_hdf5()` for more details. 
 * @tparam Externals Class describing how to resolve external references for type `EXTERNAL`.
 * See `parse_hdf5()` for more details. 
 *
 * @param reader Instance of a `byteme::Reader` providing the contents of the JSON file.
 * @param ext Instance of an external reference resolver class.
 *
 * @return Pointer to the root `Base` object.
 * Depending on `Provisioner`, this may contain references to all nested objects.
 *
 * Any invalid representations in `reader` will cause an error to be thrown.
 */
template<class Provisioner, class Externals>
std::shared_ptr<Base> parse_json(byteme::Reader& reader, Externals ext) {
    auto contents = json::raw::parse_json(reader);
    ExternalTracker etrack(std::move(ext));
    auto output = json::parse_object<Provisioner>(contents.get(), etrack, "");
    etrack.validate();
    return output;
}

/**
 * Parse JSON file contents using the **uzuki2** specification, assuming that there are no external references.
 *
 * @tparam Provisioner A class namespace defining static methods for creating new `Base` objects.
 * See `parse_hdf5()` for more details. 
 *
 * @param reader Instance of a `byteme::Reader` providing the contents of the JSON file.
 *
 * @return Pointer to the root `Base` object.
 * Depending on `Provisioner`, this may contain references to all nested objects.
 *
 * Any invalid representations in `reader` will cause an error to be thrown.
 */
template<class Provisioner>
std::shared_ptr<Base> parse_json(byteme::Reader& reader) {
    DummyExternals ext(0);
    return parse_json<Provisioner>(reader, std::move(ext));
}

/**
 * Parse JSON file contents using the **uzuki2** specification, given the file path.
 *
 * @tparam Provisioner A class namespace defining static methods for creating new `Base` objects.
 * See `parse_hdf5()` for more details. 
 * @tparam Externals Class describing how to resolve external references for type `EXTERNAL`.
 * See `parse_hdf5()` for more details. 
 *
 * @param file Path to a (possibly Gzip-compressed) JSON file.
 * @param ext Instance of an external reference resolver class.
 *
 * @return Pointer to the root `Base` object.
 * Depending on `Provisioner`, this may contain references to all nested objects.
 *
 * Any invalid representations in `reader` will cause an error to be thrown.
 */
template<class Provisioner, class Externals>
std::shared_ptr<Base> parse_json(const std::string& file, Externals ext) {
    byteme::SomeFileReader reader(file.c_str());
    return parse_json<Provisioner>(reader, std::move(ext));
}

/**
 * Parse JSON file contents using the **uzuki2** specification, given the file path.
 * This assumes that there are no external references.
 *
 * @tparam Provisioner A class namespace defining static methods for creating new `Base` objects.
 * See `parse_hdf5()` for more details. 
 *
 * @param file Path to a (possibly Gzip-compressed) JSON file.
 *
 * @return Pointer to the root `Base` object.
 * Depending on `Provisioner`, this may contain references to all nested objects.
 *
 * Any invalid representations in `reader` will cause an error to be thrown.
 */
template<class Provisioner>
std::shared_ptr<Base> parse_json(const std::string& file) {
    DummyExternals ext(0);
    return parse_json<Provisioner>(file, std::move(ext));
}

/**
 * Parse JSON file contents using the **uzuki2** specification, given a buffer containing the file contents.
 *
 * @tparam Provisioner A class namespace defining static methods for creating new `Base` objects.
 * See `parse_hdf5()` for more details. 
 * @tparam Externals Class describing how to resolve external references for type `EXTERNAL`.
 * See `parse_hdf5()` for more details. 
 *
 * @param[in] buffer Pointer to an array containing the JSON file contents (possibly Gzip/Zlib-compressed).
 * @param len Length of the buffer in bytes.
 * @param ext Instance of an external reference resolver class.
 *
 * @return Pointer to the root `Base` object.
 * Depending on `Provisioner`, this may contain references to all nested objects.
 *
 * Any invalid representations in `reader` will cause an error to be thrown.
 */
template<class Provisioner, class Externals>
std::shared_ptr<Base> parse_json(const unsigned char* buffer, size_t len, Externals ext) {
    byteme::SomeBufferReader reader(buffer, len);
    return parse_json<Provisioner>(reader, std::move(ext));
}

/**
 * Parse JSON file contents using the **uzuki2** specification, given a buffer containing the file contents.
 * This assumes that there are no external references.
 *
 * @tparam Provisioner A class namespace defining static methods for creating new `Base` objects.
 * See `parse_hdf5()` for more details. 
 *
 * @param[in] buffer Pointer to an array containing the JSON file contents (possibly Gzip/Zlib-compressed).
 * @param len Length of the buffer in bytes.
 *
 * @return Pointer to the root `Base` object.
 * Depending on `Provisioner`, this may contain references to all nested objects.
 *
 * Any invalid representations in `reader` will cause an error to be thrown.
 */
template<class Provisioner>
std::shared_ptr<Base> parse_json(const unsigned char* buffer, size_t len) {
    DummyExternals ext(0);
    return parse_json<Provisioner>(buffer, len, std::move(ext));
}

/**
 * Validate JSON file contents against the **uzuki2** specification, given a path to the file.
 * Any invalid representations will cause an error to be thrown.
 *
 * @param file Path to a (possible Gzip-compressed) JSON file.
 * @param num_external Expected number of external references. 
 */
inline void validate_json(const std::string& file, int num_external = 0) {
    DummyExternals ext(num_external);
    parse_json<DummyProvisioner>(file, std::move(ext));
    return;
}

/**
 * Validate JSON file contents against the **uzuki2** specification, given a buffer containing the file contents.
 * Any invalid representations will cause an error to be thrown.
 *
 * @param[in] buffer Pointer to an array containing the JSON file contents (possibly Gzip/Zlib-compressed).
 * @param len Length of the buffer in bytes.
 * @param num_external Expected number of external references. 
 */
inline void validate_json(const unsigned char* buffer, size_t len, int num_external = 0) {
    DummyExternals ext(num_external);
    parse_json<DummyProvisioner>(buffer, len, std::move(ext));
    return;
}

}

#endif
