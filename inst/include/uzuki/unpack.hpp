#ifndef UZUKI_UNPACK_HPP
#define UZUKI_UNPACK_HPP

#include "interfaces.hpp"

#include <string>
#include <unordered_map>
#include <stdexcept>

namespace uzuki {

template<typename T>
bool is_integer(T val) {
    return std::floor(val) == val;
}

inline bool is_date(const std::string& val) {
    if (val.size() != 10) {
        return false;
    } 
    
    for (size_t p = 0; p < val.size(); ++p) {
        if (p == 4 || p == 7) {
            if (val[p] != '-') {
                return false;
            }
        } else {
            if (!std::isdigit(val[p])) {
                return false;
            }
        }
    }

    if (val[5] == '1') {
        if (val[6] > '2') {
            return false;
        }
    } else if (val[5] != '0') {
        return false;
    }

    return true;
}

template<class Json, class Thing>
void check_names(const Json& j, size_t n, Thing* vec, const std::string& sofar) {
    if (!j.is_array() || j.size() != n) {
        throw std::runtime_error("\"" + sofar + "\" should be an array of length " + std::to_string(n));
    }

    for (size_t i = 0; i < n; ++i) {
        if (!j[i].is_string()) {
            throw std::runtime_error("\"" + sofar + "[" + std::to_string(i) + "]\" should be a string");
        }
        vec->set_name(i, j[i].template get<std::string>());
    }
}

template<class Provisioner, class Json, typename... Ts>
std::shared_ptr<Base> check_factors(const Json& j, const Json& values, const std::string& sofar, bool ordered, Ts... args) {
    auto lIt = j.find("levels");
    if (lIt == j.end() || !lIt->is_array()) {
        throw std::runtime_error("\"" + sofar + ".levels\" should be an array"); 
    }
    const auto& levels = *lIt;

    auto fptr = Provisioner::new_Factor(args..., levels.size());
    std::shared_ptr<Base> output(fptr);

    std::unordered_map<std::string, size_t> levs;
    for (size_t i = 0; i < levels.size(); ++i) {
        const auto& l = levels[i];
        if (!l.is_string()) {
            throw std::runtime_error("\"" + sofar + ".levels[" + std::to_string(i) + "]\" should be a string");
        }

        auto curlev = l.template get<std::string>();
        if (levs.find(curlev) != levs.end()) {
            throw std::runtime_error("\"" + sofar + ".levels[" + std::to_string(i) + "]\" is duplicated (" + curlev + ")");
        }

        levs[curlev] = i;
        fptr->set_level(i, curlev); 
    }

    if (ordered) {
        fptr->is_ordered();
    }

    for (size_t i = 0; i < values.size(); ++i) {
        const auto& x = values[i];
        if (x.is_null()) {
            fptr->set_missing(i);
        } else if (x.is_string()) {
            std::string val = x.template get<std::string>();
            auto levIt = levs.find(val);
            if (levIt == levs.end()) {
                throw std::runtime_error("\"" + sofar + ".values[" + std::to_string(i) + "]\" should be present in \"" + sofar + ".levels\"");
            }
            fptr->set(i, levIt->second);
        } else {
            throw std::runtime_error("\"" + sofar + ".values[" + std::to_string(i) + "]\" should be a string");
        }
    }

    return output;
}

template<class Provisioner, class Json, typename... Ts>
std::shared_ptr<Base> check_values(const std::string& type, const Json& values, const Json& j, const std::string& sofar, Ts... args) {
    std::shared_ptr<Base> output;

    // Checking values.
    if (type == "string") {
        auto ptr = Provisioner::new_String(args...);
        output.reset(ptr);
        for (size_t i = 0; i < values.size(); ++i) {
            const auto& x = values[i];
            if (x.is_null()) {
                ptr->set_missing(i);
            } else if (x.is_string()) {
                ptr->set(i, x.template get<std::string>());
            } else {
                throw std::runtime_error("\"" + sofar + ".values[" + std::to_string(i) + "]\" should be a string");
            }
        }

    } else if (type == "date") {
        auto ptr = Provisioner::new_Date(args...);
        output.reset(ptr);
        for (size_t i = 0; i < values.size(); ++i) {
            const auto& x = values[i];
            if (x.is_null()) {
                ptr->set_missing(i);
            } else if (x.is_string()) {
                std::string val = x.template get<std::string>();
                if (!is_date(val)) {
                    throw std::runtime_error("\"" + sofar + ".values[" + std::to_string(i) + "]\" should use a YYYY-MM-DD format");
                }
                ptr->set(i, val);
            } else {
                throw std::runtime_error("\"" + sofar + ".values[" + std::to_string(i) + "]\" should be a string");
            }
        }

    } else if (type == "factor" || type == "ordered") {
        output = check_factors<Provisioner>(j, values, sofar, (type == "ordered"), args...);

    } else if (type == "integer") {
        auto ptr = Provisioner::new_Integer(args...);
        output.reset(ptr);
        for (size_t i = 0; i < values.size(); ++i) {
            const auto& x = values[i];
            if (x.is_null()) {
                ptr->set_missing(i);
            } else if (x.is_number()) {
                double val = x.template get<double>();

                constexpr double upper_limit = std::numeric_limits<int32_t>::max();
                constexpr double lower_limit = std::numeric_limits<int32_t>::min();
                if (val < lower_limit || val > upper_limit) {
                    throw std::runtime_error("\"" + sofar + ".values[" + std::to_string(i) + "]\" is out of 32-bit integer range");
                }

                if (!is_integer(val)) {
                    throw std::runtime_error("\"" + sofar + ".values[" + std::to_string(i) + "]\" should be an integer");
                }
                ptr->set(i, val);
            } else {
                throw std::runtime_error("\"" + sofar + ".values[" + std::to_string(i) + "]\" should be an integer");
            }
        }

    } else if (type == "number") {
        auto ptr = Provisioner::new_Number(args...);
        output.reset(ptr);
        for (size_t i = 0; i < values.size(); ++i) {
            const auto& x = values[i];
            if (x.is_null()) {
                ptr->set_missing(i);
            } else if (x.is_number()) {
                ptr->set(i, x.template get<double>());
            } else {
                throw std::runtime_error("\"" + sofar + ".values[" + std::to_string(i) + "]\" should be a number");
            }
        }

    } else if (type == "boolean") {
        auto ptr = Provisioner::new_Boolean(args...);
        output.reset(ptr);
        for (size_t i = 0; i < values.size(); ++i) {
            const auto& x = values[i];
            if (x.is_null()) {
                ptr->set_missing(i);
            } else if (x.is_boolean()) {
                ptr->set(i, x.template get<bool>());
            } else {
                throw std::runtime_error("\"" + sofar + ".values[" + std::to_string(i) + "]\" should be a boolean");
            }
        }

    } else {
        throw std::runtime_error("unrecognized \"" + sofar + ".type\" of \"" + type + "\"");
    }

    return output;
}

template<class Provisioner, class Json>
inline std::shared_ptr<Base> check_simple_object(const std::string& type, const Json& j, const std::string& sofar) {
    auto vIt = j.find("values");
    if (vIt == j.end() || !vIt->is_array()) {
        throw std::runtime_error("\"" + sofar + ".values\" should be an array");
    }
    const auto& values = *vIt;
    size_t len = values.size();

    // Checking if we're dealing with an array.
    auto dimIt = j.find("dimensions");
    if (dimIt == j.end()) {
        auto ptr = check_values<Provisioner>(type, values, j, sofar, len);
        Vector* vptr = static_cast<Vector*>(ptr.get());

        auto namIt = j.find("names");
        if (namIt != j.end()) {
            vptr->use_names();
            check_names(*namIt, values.size(), vptr, sofar + ".names");
        }

        return ptr;
    }

    // Storing the dimensions.
    if (!dimIt->is_array() || dimIt->size() == 0) {
        throw std::runtime_error("\"" + sofar + ".dimensions\" should be an non-empty array");
    }
    const auto& dimensions = *dimIt;

    size_t prod = 1;
    std::vector<size_t> dims(dimensions.size());
    for (size_t d = 0; d < dimensions.size(); ++d) {
        const auto& current = dimensions[d];
        bool fail = true;

        if (current.is_number()) {
            double val = current.template get<double>();
            if (is_integer(val) && val >= 0) {
                dims[d] = val;
                prod *= val;
                fail = false;
            }
        }
        if (fail) {
            throw std::runtime_error("\"" + sofar + ".dimensions[" + std::to_string(d) + "]\" should be a non-negative integer");
        }
    }
    if (prod != len) {
        throw std::runtime_error("product of \"" + sofar + ".dimensions\" should be equal to length of \"" + sofar + ".values\"");
    }

    auto ptr = check_values<Provisioner>(type, values, j, sofar, dims);
    Array* aptr = static_cast<Array*>(ptr.get());

    // Checking if we need to check the names.
    auto namIt = j.find("names");
    if (namIt != j.end()) {
        if (!namIt->is_array() || namIt->size() != dims.size()) {
            throw std::runtime_error("\"" + sofar + ".names\" should be an array of length equal to \"" + sofar + ".dimensions\"");
        }
        const auto& names = *namIt;

        for (size_t d = 0; d < dimensions.size(); ++d) {
            const auto& dimname = names[d];
            if (!dimname.is_null()) {
                aptr->use_names(d);
                if (!dimname.is_array() || dimname.size() != dims[d]) {
                    auto xpath = sofar + ".names[" + std::to_string(d) + "]";
                    throw std::runtime_error("\"" + xpath + "\" should be an array of length " + std::to_string(dims[d]));
                }

                for (size_t i = 0; i < dimname.size(); ++i) {
                    const auto& x = dimname[i];
                    if (!x.is_string()) {
                        auto xpath = sofar + ".names[" + std::to_string(d) + "]";
                        throw std::runtime_error("\"" + xpath + "[" + std::to_string(i) + "]\" should be a string");
                    }
                    aptr->set_name(d, i, x.template get<std::string>());
                }
            }
        }
    }

    return ptr;
}

template<class Provisioner, class Json, class Externals>
inline std::shared_ptr<Base> terminal_validator(const Json& j, std::string sofar, Externals& others) {
    std::shared_ptr<Base> output;

    auto tIt = j.find("type");
    if (tIt == j.end() || !tIt->is_string()) {
        throw std::runtime_error("\"" + sofar + ".type\" should be a string field");
    }

    std::string type = tIt->template get<std::string>();
    if (type == "other") {
        auto iIt = j.find("index");
        if (iIt == j.end() || !iIt->is_number()) {
            throw std::runtime_error("\"" + sofar + ".index\" should be a number for type \"other\"");
        }

        double val = iIt->template get<double>();
        if (val < 0 || !is_integer(val)) {
            throw std::runtime_error("\"" + sofar + ".index\" should be a non-negative integer for type \"other\"");
        }

        size_t idx = val;
        if (idx >= others.size()) {
            throw std::runtime_error("\"" + sofar + ".index\" for type \"other\" is out of range (" + std::to_string(others.size()) + " objects available)");
        }
        output.reset(Provisioner::new_Other(others(idx)));

    } else if (type == "data.frame") {
        auto rIt = j.find("rows");
        if (rIt == j.end() || !rIt->is_number() || !is_integer(rIt->template get<double>())) {
            throw std::runtime_error("\"" + sofar + ".rows\" should be an integer for type \"data.frame\"");
        }
        size_t nr = rIt->template get<double>();

        auto cIt = j.find("columns");
        if (cIt == j.end() || !cIt->is_object()) {
            throw std::runtime_error("\"" + sofar + ".columns\" should be an object for type \"data.frame\"");
        }
        size_t nc = cIt->size();

        auto dptr = Provisioner::new_DataFrame(nr, nc);
        output.reset(dptr);

        size_t i = 0;
        for (const auto& x : cIt->items()) {
            auto curpath = sofar + ".columns." + x.key();
            const auto& curobj = x.value();

            auto tIt = curobj.find("type");
            if (tIt == curobj.end() || !tIt->is_string()) {
                throw std::runtime_error("\"" + curpath + ".type\" should be a string");
            }

            auto ptr = check_simple_object<Provisioner>(tIt->template get<std::string>(), curobj, curpath);
            if (is_vector(ptr->type())) {
                auto vptr = static_cast<Vector*>(ptr.get());
                if (vptr->size() != nr) {
                    throw std::runtime_error("size of \"" + curpath + "\" is not consistent with \"" + sofar + ".rows\"");
                }
            } else if (is_array(ptr->type())) {
                auto aptr = static_cast<Array*>(ptr.get());
                if (aptr->first_dim() != nr) {
                    throw std::runtime_error("first dimension of \"" + curpath + "\" is not consistent with \"" + sofar + ".rows\"");
                }
            } else {
                throw std::runtime_error("unsupported type"); // this should really be handled by check_simple_object.
            }

            dptr->set(i, x.key(), ptr);
            ++i;
        }

        auto namIt = j.find("names");
        if (namIt != j.end()) {
            dptr->use_names();
            check_names(*namIt, nr, dptr, sofar + ".names");
        }

    } else if (type == "nothing") {
        output.reset(Provisioner::new_Nothing());

    } else {
        output = check_simple_object<Provisioner>(type, j, sofar);
    }

    return output;
}

template<class Provisioner, class Json, class Externals>
inline std::shared_ptr<Base> recursive_validator(const Json& j, std::string sofar, Externals& others) {
    std::shared_ptr<Base> output;

    if (j.is_array()) {
        auto lptr = Provisioner::new_List(j.size());
        output.reset(lptr);
        for (size_t i = 0; i < j.size(); ++i) {
            lptr->set(i, recursive_validator<Provisioner>(j[i], sofar + "[" + std::to_string(i) + "]", others));
        }
        
    } else if (j.is_object()) {
        auto tIt = j.find("type");
        bool terminated = false;

        if (tIt != j.end()) {
            if (tIt->is_string()) {
                if (sofar == "") {
                    throw std::runtime_error("top-level \".type\" should be an object or array");
                }
                terminated = true;
                output = terminal_validator<Provisioner>(j, sofar, others);
            } else if (!tIt->is_object() && !tIt->is_array()) {
                throw std::runtime_error("\"" + sofar + ".type\" should be an object, array or string");
            }
        }

        if (!terminated) {
            auto lptr = Provisioner::new_List(j.size());
            output.reset(lptr);
            lptr->use_names();

            size_t i = 0;
            for (const auto& x : j.items()) {
                lptr->set(i, recursive_validator<Provisioner>(x.value(), sofar + "." + x.key(), others));
                lptr->set_name(i, x.key());
                ++i;
            }
        }
    } else {
        throw std::runtime_error("structural elements should JSON arrays or objects");
    }

    return output;
}

template<class Provisioner, class Json, class Externals>
std::shared_ptr<Base> unpack(const Json& j, Externals& others) {
    return recursive_validator<Provisioner>(j, "", others);
}

}

#endif
