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

#include "byteme/PerByte.hpp"
#include "byteme/SomeFileReader.hpp"
#include "byteme/SomeBufferReader.hpp"
#include "millijson/millijson.hpp"

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

inline const std::vector<std::shared_ptr<millijson::Base> >& extract_array(
    const std::unordered_map<std::string, std::shared_ptr<millijson::Base> >& properties, 
    const std::string& name, 
    const std::string& path) 
{
    auto vIt = properties.find(name);
    if (vIt == properties.end()) {
        throw std::runtime_error("expected '" + name + "' property for object at '" + path + "'");
    }

    const auto& values_ptr = vIt->second;
    if (values_ptr->type() != millijson::ARRAY) {
        throw std::runtime_error("expected an array in '" + path + "." + name + "'"); 
    }

    return static_cast<const millijson::Array*>(values_ptr.get())->values;
}

template<class Function>
void process_array_or_scalar_values(
    const std::unordered_map<std::string, std::shared_ptr<millijson::Base> >& properties, 
    const std::string& path,
    Function fun)
{
    auto vIt = properties.find("values");
    if (vIt == properties.end()) {
        throw std::runtime_error("expected 'values' property for object at '" + path + "'");
    }

    const auto& values_ptr = vIt->second;
    if (values_ptr->type() == millijson::ARRAY) {
        fun(static_cast<const millijson::Array*>(values_ptr.get())->values);
    } else {
        std::vector<std::shared_ptr<millijson::Base> > temp { values_ptr };
        auto ptr = fun(temp);
        ptr->is_scalar();
    }
}

template<class Destination, class Function>
void extract_integers(const std::vector<std::shared_ptr<millijson::Base> >& values, Destination* dest, Function check, const std::string& path) {
    for (size_t i = 0; i < values.size(); ++i) {
        if (values[i]->type() == millijson::NOTHING) {
            dest->set_missing(i);
            continue;
        }

        if (values[i]->type() != millijson::NUMBER) {
            throw std::runtime_error("expected a number at '" + path + ".values[" + std::to_string(i) + "]'");
        }

        auto val = static_cast<const millijson::Number*>(values[i].get())->value;
        if (val != std::floor(val)) {
            throw std::runtime_error("expected an integer at '" + path + ".values[" + std::to_string(i) + "]'");
        }

        constexpr double upper = std::numeric_limits<int32_t>::max();
        constexpr double lower = std::numeric_limits<int32_t>::min();
        if (val < lower || val > upper) {
            throw std::runtime_error("value at '" + path + ".values[" + std::to_string(i) + "]' cannot be represented by a 32-bit signed integer");
        }

        int32_t ival = val;
        if (val == -2147483648) {
            dest->set_missing(i);
            continue;
        }

        check(ival);
        dest->set(i, ival);
    }
}

template<class Destination, class Function>
void extract_strings(const std::vector<std::shared_ptr<millijson::Base> >& values, Destination* dest, Function check, const std::string& path) {
    for (size_t i = 0; i < values.size(); ++i) {
        if (values[i]->type() == millijson::NOTHING) {
            dest->set_missing(i);
            continue;
        }

        if (values[i]->type() != millijson::STRING) {
            throw std::runtime_error("expected a string at '" + path + ".values[" + std::to_string(i) + "]'");
        }

        const auto& str = static_cast<const millijson::String*>(values[i].get())->value;
        check(str);
        dest->set(i, str);
    }
}

template<class Destination>
void extract_names(const std::unordered_map<std::string, std::shared_ptr<millijson::Base> >& properties, Destination* dest, const std::string& path) {
    auto nIt = properties.find("names");
    if (nIt == properties.end()) {
        return;
    }

    const auto name_ptr = nIt->second;
    if (name_ptr->type() != millijson::ARRAY) {
        throw std::runtime_error("expected an array in '" + path + ".names'"); 
    }

    const auto& names = static_cast<const millijson::Array*>(name_ptr.get())->values;
    if (names.size() != dest->size()) {
        throw std::runtime_error("length of 'names' and 'values' should be the same in '" + path + "'"); 
    }
    dest->use_names();

    for (size_t i = 0; i < names.size(); ++i) {
        if (names[i]->type() != millijson::STRING) {
            throw std::runtime_error("expected a string at '" + path + ".names[" + std::to_string(i) + "]'");
        }
        dest->set_name(i, static_cast<const millijson::String*>(names[i].get())->value);
    }
}

template<class Provisioner, class Externals>
std::shared_ptr<Base> parse_object(const millijson::Base* contents, Externals& ext, const std::string& path) {
    if (contents->type() != millijson::OBJECT) {
        throw std::runtime_error("each R object should be represented by a JSON object at '" + path + "'");
    }

    auto optr = static_cast<const millijson::Object*>(contents);
    const auto& map = optr->values;

    auto tIt = map.find("type");
    if (tIt == map.end()) {
        throw std::runtime_error("missing 'type' property for JSON object at '" + path + "'");
    }
    const auto& type_ptr = tIt->second;
    if (type_ptr->type() != millijson::STRING) {
        throw std::runtime_error("expected a string at '" + path + ".type'");
    }
    const auto& type = static_cast<const millijson::String*>(type_ptr.get())->value;

    std::shared_ptr<Base> output;
    if (type == "nothing") {
        output.reset(Provisioner::new_Nothing());

    } else if (type == "external") {
        auto iIt = map.find("index");
        if (iIt == map.end()) {
            throw std::runtime_error("expected 'index' property for 'external' type at '" + path + "'");
        }
        const auto& index_ptr = iIt->second;
        if (index_ptr->type() != millijson::NUMBER) {
            throw std::runtime_error("expected a number at '" + path + ".index'");
        }
        auto index = static_cast<const millijson::Number*>(index_ptr.get())->value;

        if (index != std::floor(index)) {
            throw std::runtime_error("expected an integer at '" + path + ".index'");
        } else if (index < 0 || index >= static_cast<double>(ext.size())) {
            throw std::runtime_error("external index out of range at '" + path + ".index'");
        }
        output.reset(Provisioner::new_External(ext.get(index)));

    } else if (type == "integer") {
        process_array_or_scalar_values(map, path, [&](const auto& vals) -> auto {
            auto ptr = Provisioner::new_Integer(vals.size());
            output.reset(ptr);
            extract_integers(vals, ptr, [](int32_t) -> void {}, path);
            extract_names(map, ptr, path);
            return ptr;
        });

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
            if (lvals[l]->type() != millijson::STRING) {
                throw std::runtime_error("expected strings at '" + path + ".levels[" + std::to_string(l) + "]'");
            }

            const auto& level = static_cast<const millijson::String*>(lvals[l].get())->value;
            if (existing.find(level) != existing.end()) {
                throw std::runtime_error("detected duplicate string at '" + path + ".levels[" + std::to_string(l) + "]'");
            }
            ptr->set_level(l, level);
            existing.insert(level);
        }

        extract_names(map, ptr, path);

    } else if (type == "boolean") {
        process_array_or_scalar_values(map, path, [&](const auto& vals) -> auto {
            auto ptr = Provisioner::new_Boolean(vals.size());
            output.reset(ptr);

            for (size_t i = 0; i < vals.size(); ++i) {
                if (vals[i]->type() == millijson::NOTHING) {
                    ptr->set_missing(i);
                    continue;
                }

                if (vals[i]->type() != millijson::BOOLEAN) {
                    throw std::runtime_error("expected a boolean at '" + path + ".values[" + std::to_string(i) + "]'");
                }
                ptr->set(i, static_cast<const millijson::Boolean*>(vals[i].get())->value);
            }

            extract_names(map, ptr, path);
            return ptr;
        });

    } else if (type == "number") {
        process_array_or_scalar_values(map, path, [&](const auto& vals) -> auto {
            auto ptr = Provisioner::new_Number(vals.size());
            output.reset(ptr);

            for (size_t i = 0; i < vals.size(); ++i) {
                if (vals[i]->type() == millijson::NOTHING) {
                    ptr->set_missing(i);
                    continue;
                }

                if (vals[i]->type() == millijson::NUMBER) {
                    ptr->set(i, static_cast<const millijson::Number*>(vals[i].get())->value);
                } else if (vals[i]->type() == millijson::STRING) {
                    auto str = static_cast<const millijson::String*>(vals[i].get())->value;
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
            return ptr;
        });

    } else if (type == "string") {
        process_array_or_scalar_values(map, path, [&](const auto& vals) -> auto {
            auto ptr = Provisioner::new_String(vals.size());
            output.reset(ptr);
            extract_strings(vals, ptr, [](const std::string&) -> void {}, path);
            extract_names(map, ptr, path);
            return ptr;
        });

    } else if (type == "date") {
        process_array_or_scalar_values(map, path, [&](const auto& vals) -> auto {
            auto ptr = Provisioner::new_Date(vals.size());
            output.reset(ptr);
            extract_strings(vals, ptr, [&](const std::string& x) -> void {
                if (!is_date(x)) {
                     throw std::runtime_error("dates should follow YYYY-MM-DD formatting in '" + path + ".values'");
                }
            }, path);
            extract_names(map, ptr, path);
            return ptr;
        });

    } else if (type == "date-time") {
        const auto& vals = extract_array(map, "values", path);
        auto ptr = Provisioner::new_DateTime(vals.size());
        output.reset(ptr);
        extract_strings(vals, ptr, [&](const std::string& x) -> void {
            if (!is_rfc3339(x)) {
                 throw std::runtime_error("date-times should follow the Internet Date/Time format in '" + path + ".values'");
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
 * @brief Parse JSON file contents using the **uzuki2** specification.
 *
 * JSON provides an alternative to the HDF5 format that is handled by `Hdf5Parser`.
 * JSON is simpler to parse and has less formatting-related overhead.
 * However, it does not support random access and discards some precision for floating-point numbers.
 */
class JsonParser {
public:
    /**
     * Whether parsing should be done in parallel to file I/O.
     * If true, an extra thread is used to avoid blocking I/O operations.
     */
    bool parallel = false;

    /**
     * Parse JSON file contents using the **uzuki2** specification, given an arbitrary input source of bytes.
     *
     * @tparam Provisioner A class namespace defining static methods for creating new `Base` objects.
     * See `Hdf5Parser::parse()` for more details. 
     * @tparam Externals Class describing how to resolve external references for type `EXTERNAL`.
     * See `Hdf5Parser::parse()` for more details. 
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
    std::shared_ptr<Base> parse(byteme::Reader& reader, Externals ext) {
        std::shared_ptr<millijson::Base> contents;
        if (parallel) {
            byteme::PerByte bytestream(reader);
            contents = millijson::parse(bytestream);
        } else {
            byteme::PerByteParallel bytestream(reader);
            contents = millijson::parse(bytestream);
        }

        ExternalTracker etrack(std::move(ext));
        auto output = json::parse_object<Provisioner>(contents.get(), etrack, "");
        etrack.validate();
        return output;
    }

    /**
     * Overload of `parse()` assuming that there are no external references.
     *
     * @tparam Provisioner A class namespace defining static methods for creating new `Base` objects.
     * See `Hdf5Parser::parse()` for more details. 
     *
     * @param reader Instance of a `byteme::Reader` providing the contents of the JSON file.
     *
     * @return Pointer to the root `Base` object.
     * Depending on `Provisioner`, this may contain references to all nested objects.
     *
     * Any invalid representations in `reader` will cause an error to be thrown.
     */
    template<class Provisioner>
    std::shared_ptr<Base> parse(byteme::Reader& reader) {
        DummyExternals ext(0);
        return parse<Provisioner>(reader, std::move(ext));
    }

public:
    /**
     * Parse JSON file contents using the **uzuki2** specification, given the file path.
     *
     * @tparam Provisioner A class namespace defining static methods for creating new `Base` objects.
     * See `Hdf5Parser::parse()` for more details. 
     * @tparam Externals Class describing how to resolve external references for type `EXTERNAL`.
     * See `Hdf5Parser::parse()` for more details. 
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
    std::shared_ptr<Base> parse_file(const std::string& file, Externals ext) {
        byteme::SomeFileReader reader(file.c_str());
        return parse<Provisioner>(reader, std::move(ext));
    }

    /**
     * Overload of `parse_file()` assuming that there are no external references.
     *
     * @tparam Provisioner A class namespace defining static methods for creating new `Base` objects.
     * See `Hdf5Parser::parse()` for more details. 
     *
     * @param file Path to a (possibly Gzip-compressed) JSON file.
     *
     * @return Pointer to the root `Base` object.
     * Depending on `Provisioner`, this may contain references to all nested objects.
     *
     * Any invalid representations in `reader` will cause an error to be thrown.
     */
    template<class Provisioner>
    std::shared_ptr<Base> parse_file(const std::string& file) {
        DummyExternals ext(0);
        return parse_file<Provisioner>(file, std::move(ext));
    }

public:
    /**
     * Parse a buffer containing JSON file contents using the **uzuki2** specification. 
     *
     * @tparam Provisioner A class namespace defining static methods for creating new `Base` objects.
     * See `Hdf5Parser::parse()` for more details. 
     * @tparam Externals Class describing how to resolve external references for type `EXTERNAL`.
     * See `Hdf5Parser::parse()` for more details. 
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
    std::shared_ptr<Base> parse_buffer(const unsigned char* buffer, size_t len, Externals ext) {
        byteme::SomeBufferReader reader(buffer, len);
        return parse<Provisioner>(reader, std::move(ext));
    }

    /**
     * Overload of `parse_buffer()` assuming that there are no external references.
     *
     * @tparam Provisioner A class namespace defining static methods for creating new `Base` objects.
     * See `Hdf5Parser::parse()` for more details. 
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
    std::shared_ptr<Base> parse_buffer(const unsigned char* buffer, size_t len) {
        DummyExternals ext(0);
        return parse_buffer<Provisioner>(buffer, len, std::move(ext));
    }

public:
    /**
     * Validate JSON file contents against the **uzuki2** specification, given a source of bytes.
     * Any invalid representations will cause an error to be thrown.
     *
     * @param reader Instance of a `byteme::Reader` providing the contents of the JSON file.
     * @param num_external Expected number of external references. 
     */
    void validate(byteme::Reader& reader, int num_external = 0) {
        DummyExternals ext(num_external);
        parse<DummyProvisioner>(reader, std::move(ext));
        return;
    }

    /**
     * Validate JSON file contents against the **uzuki2** specification, given a path to the file.
     * Any invalid representations will cause an error to be thrown.
     *
     * @param file Path to a (possible Gzip-compressed) JSON file.
     * @param num_external Expected number of external references. 
     */
    void validate_file(const std::string& file, int num_external = 0) {
        DummyExternals ext(num_external);
        parse_file<DummyProvisioner>(file, std::move(ext));
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
    void validate_buffer(const unsigned char* buffer, size_t len, int num_external = 0) {
        DummyExternals ext(num_external);
        parse_buffer<DummyProvisioner>(buffer, len, std::move(ext));
        return;
    }
};

}

#endif
