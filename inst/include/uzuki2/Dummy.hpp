#ifndef UZUKI2_DUMMY_HPP
#define UZUKI2_DUMMY_HPP

#include <vector>
#include <memory>
#include <string>
#include <cstdint>

#include "interfaces.hpp"

namespace uzuki2 {

/** Defining the simple vectors first. **/

template<typename T, Type tt>
struct DummyTypedVector : public TypedVector<T, tt> {
    DummyTypedVector(size_t s) : length(s) {}

    size_t size() const { return length; }

    void set(size_t, T) {}
    void set_missing(size_t) {}
   
    void use_names() {}
    void set_name(size_t, std::string) {}

    void is_scalar() {}

    size_t length;
};

typedef DummyTypedVector<int32_t, INTEGER> DummyIntegerVector;
typedef DummyTypedVector<double, NUMBER> DummyNumberVector;
typedef DummyTypedVector<std::string, STRING> DummyStringVector;
typedef DummyTypedVector<unsigned char, BOOLEAN> DummyBooleanVector;
typedef DummyTypedVector<std::string, DATE> DummyDateVector;
typedef DummyTypedVector<std::string, DATETIME> DummyDateTimeVector;

struct DummyFactor : public Factor {
    DummyFactor(size_t s, size_t) : length(s) {}

    size_t size() const { return length; }

    void set(size_t, size_t) {}
    void set_missing(size_t) {}
   
    void use_names() {}
    void set_name(size_t, std::string) {}

    void is_ordered() {}
    void set_level(size_t, std::string) {}

    size_t length;
};

/** Defining the structural elements. **/

struct DummyNothing : public Nothing {};

struct DummyExternal : public External {};

struct DummyList : public List {
    DummyList(size_t n) : length(n) {}

    size_t size() const { return length; }
    void set(size_t, std::shared_ptr<Base>) {}

    void use_names() {}
    void set_name(size_t, std::string) {}

    size_t length;
};

/** Dummy provisioner. **/

struct DummyProvisioner {
    static Nothing* new_Nothing() { return (new DummyNothing); }

    static External* new_External(void*) { return (new DummyExternal); }

    static List* new_List(size_t l) { return (new DummyList(l)); }

    static IntegerVector* new_Integer(size_t l) { return (new DummyIntegerVector(l)); }

    static NumberVector* new_Number(size_t l) { return (new DummyNumberVector(l)); }

    static StringVector* new_String(size_t l) { return (new DummyStringVector(l)); }

    static BooleanVector* new_Boolean(size_t l) { return (new DummyBooleanVector(l)); }

    static DateVector* new_Date(size_t l) { return (new DummyDateVector(l)); }

    static DateTimeVector* new_DateTime(size_t l) { return (new DummyDateTimeVector(l)); }

    static Factor* new_Factor(size_t l, size_t ll) { return (new DummyFactor(l, ll)); }
};

struct DummyExternals {
    DummyExternals(size_t n) : number(n) {}

    void* get(size_t) const {
        return nullptr;
    }

    size_t size() const {
        return number;
    }

    size_t number;
};

}

#endif
