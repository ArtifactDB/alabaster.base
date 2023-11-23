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
#include <type_traits>

#include "byteme/PerByte.hpp"
#include "byteme/SomeFileReader.hpp"
#include "byteme/SomeBufferReader.hpp"
#include "millijson/millijson.hpp"

#include "interfaces.hpp"
#include "Dummy.hpp"
#include "ExternalTracker.hpp"
#include "ParsedList.hpp"

/**
 * @file parse_json.hpp
 * @brief Parsing methods for JSON files.
 */

namespace uzuki2 {

/**
 * @namespace uzuki2::json
 * @brief Parse an R list from a JSON file.
 *
 * JSON provides an alternative to the HDF5 format handled by `hdf5::parse()` and friends.
 * JSON is simpler to parse and has less formatting-related overhead.
 * However, it does not support random access and discards some precision for floating-point numbers.
 */
namespace json {

/**
 * @cond
 */
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

inline const millijson::Array* has_names(const std::unordered_map<std::string, std::shared_ptr<millijson::Base> >& properties, const std::string& path) {
    auto nIt = properties.find("names");
    if (nIt == properties.end()) {
        return NULL;
    }

    const auto name_ptr = nIt->second;
    if (name_ptr->type() != millijson::ARRAY) {
        throw std::runtime_error("expected an array in '" + path + ".names'"); 
    }
    return static_cast<const millijson::Array*>(name_ptr.get());
}

template<class Destination>
void fill_names(const millijson::Array* names_ptr, Destination* dest, const std::string& path) {
    const auto& names = names_ptr->values;
    if (names.size() != dest->size()) {
        throw std::runtime_error("length of 'names' and 'values' should be the same in '" + path + "'"); 
    }

    for (size_t i = 0; i < names.size(); ++i) {
        if (names[i]->type() != millijson::STRING) {
            throw std::runtime_error("expected a string at '" + path + ".names[" + std::to_string(i) + "]'");
        }
        dest->set_name(i, static_cast<const millijson::String*>(names[i].get())->value);
    }
}

template<class Function>
auto process_array_or_scalar_values(
    const std::unordered_map<std::string, std::shared_ptr<millijson::Base> >& properties, 
    const std::string& path,
    Function fun)
{
    auto vIt = properties.find("values");
    if (vIt == properties.end()) {
        throw std::runtime_error("expected 'values' property for object at '" + path + "'");
    }

    auto names_ptr = has_names(properties, path);
    bool has_names = names_ptr != NULL;

    typename std::invoke_result<Function,std::vector<std::shared_ptr<millijson::Base> >,bool,bool>::type out_ptr;

    const auto& values_ptr = vIt->second;
    if (values_ptr->type() == millijson::ARRAY) {
        out_ptr = fun(static_cast<const millijson::Array*>(values_ptr.get())->values, has_names, false);
    } else {
        std::vector<std::shared_ptr<millijson::Base> > temp { values_ptr };
        out_ptr = fun(temp, has_names, true);
    }

    if (has_names) {
        fill_names(names_ptr, out_ptr, path);
    }
    return out_ptr;
}

template<class Destination, class Function>
void extract_integers(const std::vector<std::shared_ptr<millijson::Base> >& values, Destination* dest, Function check, const std::string& path, const Version& version) {
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
        if (version.equals(1, 0) && val == -2147483648) {
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

template<class Provisioner, class Externals>
std::shared_ptr<Base> parse_object(const millijson::Base* contents, Externals& ext, const std::string& path, const Version& version) {
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
        process_array_or_scalar_values(map, path, [&](const auto& vals, bool named, bool scalar) -> auto {
            auto ptr = Provisioner::new_Integer(vals.size(), named, scalar);
            output.reset(ptr);
            extract_integers(vals, ptr, [](int32_t) -> void {}, path, version);
            return ptr;
        });

    } else if (type == "factor" || (version.equals(1, 0) && type == "ordered")) {
        bool ordered = false;
        if (type == "ordered") {
            ordered = true;
        } else {
            auto oIt = map.find("ordered");
            if (oIt != map.end()) {
                if (oIt->second->type() != millijson::BOOLEAN) {
                    throw std::runtime_error("expected a boolean at '" + path + ".ordered'");
                }
                auto optr = static_cast<const millijson::Boolean*>((oIt->second).get());
                ordered = optr->value;
            }
        }

        const auto& lvals = extract_array(map, "levels", path);
        int32_t nlevels = lvals.size();
        auto fptr = process_array_or_scalar_values(map, path, [&](const auto& vals, bool named, bool scalar) -> auto {
            auto ptr = Provisioner::new_Factor(vals.size(), named, scalar, nlevels, ordered);
            output.reset(ptr);
            extract_integers(vals, ptr, [&](int32_t x) -> void {
                if (x < 0 || x >= nlevels) {
                    throw std::runtime_error("factor indices of out of range of levels in '" + path + "'");
                }
            }, path, version);
            return ptr;
        });

        std::unordered_set<std::string> existing;
        for (size_t l = 0; l < lvals.size(); ++l) {
            if (lvals[l]->type() != millijson::STRING) {
                throw std::runtime_error("expected strings at '" + path + ".levels[" + std::to_string(l) + "]'");
            }

            const auto& level = static_cast<const millijson::String*>(lvals[l].get())->value;
            if (existing.find(level) != existing.end()) {
                throw std::runtime_error("detected duplicate string at '" + path + ".levels[" + std::to_string(l) + "]'");
            }
            fptr->set_level(l, level);
            existing.insert(level);
        }

    } else if (type == "boolean") {
        process_array_or_scalar_values(map, path, [&](const auto& vals, bool named, bool scalar) -> auto {
            auto ptr = Provisioner::new_Boolean(vals.size(), named, scalar);
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

            return ptr;
        });

    } else if (type == "number") {
        process_array_or_scalar_values(map, path, [&](const auto& vals, bool named, bool scalar) -> auto {
            auto ptr = Provisioner::new_Number(vals.size(), named, scalar);
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

            return ptr;
        });

    } else if (type == "string" || (version.equals(1, 0) && (type == "date" || type == "date-time"))) {
        StringVector::Format format = StringVector::NONE;
        if (version.equals(1, 0)) {
            if (type == "date") {
                format = StringVector::DATE;
            } else if (type == "date-time") {
                format = StringVector::DATETIME;
            }
        } else {
            auto fIt = map.find("format");
            if (fIt != map.end()) {
                if (fIt->second->type() != millijson::STRING) {
                    throw std::runtime_error("expected a string at '" + path + ".format'");
                }
                auto fptr = static_cast<const millijson::String*>(fIt->second.get());
                if (fptr->value == "date") {
                    format = StringVector::DATE;
                } else if (fptr->value == "date-time") {
                    format = StringVector::DATETIME;
                } else {
                    throw std::runtime_error("unsupported format '" + fptr->value + "' at '" + path + ".format'");
                }
            }
        }

        process_array_or_scalar_values(map, path, [&](const auto& vals, bool named, bool scalar) -> auto {
            auto ptr = Provisioner::new_String(vals.size(), named, scalar, format);
            output.reset(ptr);

            if (format == StringVector::NONE) {
                extract_strings(vals, ptr, [](const std::string&) -> void {}, path);
            } else if (format == StringVector::DATE) {
                extract_strings(vals, ptr, [&](const std::string& x) -> void {
                    if (!ritsuko::is_date(x.c_str(), x.size())) {
                         throw std::runtime_error("dates should follow YYYY-MM-DD formatting in '" + path + ".values'");
                    }
                }, path);
            } else if (format == StringVector::DATETIME) {
                extract_strings(vals, ptr, [&](const std::string& x) -> void {
                    if (!ritsuko::is_rfc3339(x.c_str(), x.size())) {
                         throw std::runtime_error("date-times should follow the Internet Date/Time format in '" + path + ".values'");
                    }
                }, path);
            }

            return ptr;
        });

    } else if (type == "list") {
        auto names_ptr = has_names(map, path);
        bool has_names = names_ptr != NULL;

        const auto& vals = extract_array(map, "values", path);
        auto ptr = Provisioner::new_List(vals.size(), has_names);
        output.reset(ptr);

        for (size_t i = 0; i < vals.size(); ++i) {
            ptr->set(i, parse_object<Provisioner>(vals[i].get(), ext, path + ".values[" + std::to_string(i) + "]", version));
        }

        if (has_names) {
            fill_names(names_ptr, ptr, path);
        }

    } else {
        throw std::runtime_error("unknown object type '" + type + "' at '" + path + ".type'");
    }

    return output;
}
/**
 * @endcond
 */

/**
 * @brief Options for JSON file parsing.
 */
struct Options {
    /**
     * Whether parsing should be done in parallel to file I/O.
     * If true, an extra thread is used to avoid blocking I/O operations.
     */
    bool parallel = false;

    /**
     * Whether to throw an error if the top-level R object is not an R list.
     */
    bool strict_list = true;
};

/**
 * Parse JSON file contents using the **uzuki2** specification, given an arbitrary input source of bytes.
 *
 * @tparam Provisioner A class namespace defining static methods for creating new `Base` objects.
 * See `hdf5::parse()` for more details. 
 * @tparam Externals Class describing how to resolve external references for type `EXTERNAL`.
 * See `hdf5::parse()` for more details. 
 *
 * @param reader Instance of a `byteme::Reader` providing the contents of the JSON file.
 * @param ext Instance of an external reference resolver class.
 * @param options Options for parsing.
 *
 * @return A `ParsedList` containing a pointer to the root `Base` object.
 * Depending on `Provisioner`, this may contain references to all nested objects.
 *
 * Any invalid representations in `reader` will cause an error to be thrown.
 */
template<class Provisioner, class Externals>
ParsedList parse(byteme::Reader& reader, Externals ext, Options options = Options()) {
    std::shared_ptr<millijson::Base> contents;
    if (options.parallel) {
        byteme::PerByte bytestream(&reader);
        contents = millijson::parse(bytestream);
    } else {
        byteme::PerByteParallel bytestream(&reader);
        contents = millijson::parse(bytestream);
    }

    Version version;
    if (contents->type() == millijson::OBJECT) {
        auto optr = static_cast<const millijson::Object*>(contents.get());
        const auto& map = optr->values;
        auto vIt = map.find("version");
        if (vIt != map.end()) {
            if (vIt->second->type() != millijson::STRING) {
                throw std::runtime_error("expected a string in 'version'");
            }
            const auto& vstr = static_cast<const millijson::String*>(vIt->second.get())->value;
            auto vraw = ritsuko::parse_version_string(vstr.c_str(), vstr.size(), /* skip_patch = */ true);
            version.major = vraw.major;
            version.minor = vraw.minor;
        }
    }

    ExternalTracker etrack(std::move(ext));
    auto output = parse_object<Provisioner>(contents.get(), etrack, "", version);

    if (options.strict_list && output->type() != LIST) {
        throw std::runtime_error("top-level object should represent an R list");
    }
    etrack.validate();

    return ParsedList(std::move(output), std::move(version));
}

/**
 * Overload of `json::parse()` assuming that there are no external references.
 *
 * @tparam Provisioner A class namespace defining static methods for creating new `Base` objects.
 * See `hdf5::parse()` for more details. 
 *
 * @param reader Instance of a `byteme::Reader` providing the contents of the JSON file.
 * @param options Options for parsing.
 *
 * @return A `ParsedList` containing a pointer to the root `Base` object.
 * Depending on `Provisioner`, this may contain references to all nested objects.
 *
 * Any invalid representations in `reader` will cause an error to be thrown.
 */
template<class Provisioner>
ParsedList parse(byteme::Reader& reader, Options options = Options()) {
    DummyExternals ext(0);
    return parse<Provisioner>(reader, std::move(ext), std::move(options));
}

/**
 * Parse JSON file contents using the **uzuki2** specification, given the file path.
 *
 * @tparam Provisioner A class namespace defining static methods for creating new `Base` objects.
 * See `hdf5::parse()` for more details. 
 * @tparam Externals Class describing how to resolve external references for type `EXTERNAL`.
 * See `hdf5::parse()` for more details. 
 *
 * @param file Path to a (possibly Gzip-compressed) JSON file.
 * @param ext Instance of an external reference resolver class.
 * @param options Options for parsing.
 *
 * @return A `ParsedList` containing a pointer to the root `Base` object.
 * Depending on `Provisioner`, this may contain references to all nested objects.
 *
 * Any invalid representations in `reader` will cause an error to be thrown.
 */
template<class Provisioner, class Externals>
ParsedList parse_file(const std::string& file, Externals ext, Options options = Options()) {
    byteme::SomeFileReader reader(file.c_str());
    return parse<Provisioner>(reader, std::move(ext), std::move(options));
}

/**
 * Overload of `json::parse_file()` assuming that there are no external references.
 *
 * @tparam Provisioner A class namespace defining static methods for creating new `Base` objects.
 * See `hdf5::parse()` for more details. 
 *
 * @param file Path to a (possibly Gzip-compressed) JSON file.
 * @param options Options for parsing.
 *
 * @return A `ParsedList` containing a pointer to the root `Base` object.
 * Depending on `Provisioner`, this may contain references to all nested objects.
 *
 * Any invalid representations in `reader` will cause an error to be thrown.
 */
template<class Provisioner>
ParsedList parse_file(const std::string& file, Options options = Options()) {
    DummyExternals ext(0);
    return parse_file<Provisioner>(file, std::move(ext), std::move(options));
}

/**
 * Parse a buffer containing JSON file contents using the **uzuki2** specification. 
 *
 * @tparam Provisioner A class namespace defining static methods for creating new `Base` objects.
 * See `hdf5::parse()` for more details. 
 * @tparam Externals Class describing how to resolve external references for type `EXTERNAL`.
 * See `hdf5::parse()` for more details. 
 *
 * @param[in] buffer Pointer to an array containing the JSON file contents (possibly Gzip/Zlib-compressed).
 * @param len Length of the buffer in bytes.
 * @param ext Instance of an external reference resolver class.
 * @param options Options for parsing.
 *
 * @return A `ParsedList` containing a pointer to the root `Base` object.
 * Depending on `Provisioner`, this may contain references to all nested objects.
 *
 * Any invalid representations in `reader` will cause an error to be thrown.
 */
template<class Provisioner, class Externals>
ParsedList parse_buffer(const unsigned char* buffer, size_t len, Externals ext, Options options = Options()) {
    byteme::SomeBufferReader reader(buffer, len);
    return parse<Provisioner>(reader, std::move(ext), std::move(options));
}

/**
 * Overload of `json::parse_buffer()` assuming that there are no external references.
 *
 * @tparam Provisioner A class namespace defining static methods for creating new `Base` objects.
 * See `hdf5::parse()` for more details. 
 *
 * @param[in] buffer Pointer to an array containing the JSON file contents (possibly Gzip/Zlib-compressed).
 * @param len Length of the buffer in bytes.
 * @param options Options for parsing.
 *
 * @return A `ParsedList` containing a pointer to the root `Base` object.
 * Depending on `Provisioner`, this may contain references to all nested objects.
 *
 * Any invalid representations in `reader` will cause an error to be thrown.
 */
template<class Provisioner>
ParsedList parse_buffer(const unsigned char* buffer, size_t len, Options options = Options()) {
    DummyExternals ext(0);
    return parse_buffer<Provisioner>(buffer, len, std::move(ext), std::move(options));
}

/**
 * Validate JSON file contents against the **uzuki2** specification, given a source of bytes.
 * Any invalid representations will cause an error to be thrown.
 *
 * @param reader Instance of a `byteme::Reader` providing the contents of the JSON file.
 * @param num_external Expected number of external references. 
 * @param options Options for parsing.
 */
inline void validate(byteme::Reader& reader, int num_external = 0, Options options = Options()) {
    DummyExternals ext(num_external);
    parse<DummyProvisioner>(reader, std::move(ext), std::move(options));
    return;
}

/**
 * Validate JSON file contents against the **uzuki2** specification, given a path to the file.
 * Any invalid representations will cause an error to be thrown.
 *
 * @param file Path to a (possible Gzip-compressed) JSON file.
 * @param num_external Expected number of external references. 
 * @param options Options for parsing.
 */
inline void validate_file(const std::string& file, int num_external = 0, Options options = Options()) {
    DummyExternals ext(num_external);
    parse_file<DummyProvisioner>(file, std::move(ext), std::move(options));
    return;
}

/**
 * Validate JSON file contents against the **uzuki2** specification, given a buffer containing the file contents.
 * Any invalid representations will cause an error to be thrown.
 *
 * @param[in] buffer Pointer to an array containing the JSON file contents (possibly Gzip/Zlib-compressed).
 * @param len Length of the buffer in bytes.
 * @param num_external Expected number of external references. 
 * @param options Options for parsing.
 */
inline void validate_buffer(const unsigned char* buffer, size_t len, int num_external = 0, Options options = Options()) {
    DummyExternals ext(num_external);
    parse_buffer<DummyProvisioner>(buffer, len, std::move(ext), std::move(options));
    return;
}

}

}

#endif
