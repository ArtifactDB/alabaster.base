#ifndef UZUKI2_DUMMY_HPP
#define UZUKI2_DUMMY_HPP

#include <vector>
#include <memory>
#include <string>
#include <cstdint>

#include "interfaces.hpp"

namespace uzuki2 {

/** Defining the simple vectors first. **/

struct DummyIntegerVector : public IntegerVector {
    DummyIntegerVector(size_t l, bool, bool) : length(l) {}

    size_t size() const { return length; }
    size_t length;

    void set(size_t, int32_t) {}
    void set_missing(size_t) {}
    void set_name(size_t, std::string) {}
};

struct DummyNumberVector : public NumberVector {
    DummyNumberVector(size_t l, bool, bool) : length(l) {}

    size_t size() const { return length; }
    size_t length;

    void set(size_t, double) {}
    void set_missing(size_t) {}
    void set_name(size_t, std::string) {}
};

struct DummyStringVector : public StringVector {
    DummyStringVector(size_t l, bool, bool, StringVector::Format) : length(l) {}

    size_t size() const { return length; }
    size_t length;

    void set(size_t, std::string) {}
    void set_missing(size_t) {}
    void set_name(size_t, std::string) {}
};

struct DummyBooleanVector : public BooleanVector {
    DummyBooleanVector(size_t l, bool, bool) : length(l) {}

    size_t size() const { return length; }
    size_t length;

    void set(size_t, bool) {}
    void set_missing(size_t) {}
    void set_name(size_t, std::string) {}
};

struct DummyFactor : public Factor {
    DummyFactor(size_t l, bool, bool, size_t, bool) : length(l) {}

    size_t size() const { return length; }
    size_t length;

    void set(size_t, size_t) {}
    void set_missing(size_t) {}
    void set_name(size_t, std::string) {}

    void set_level(size_t, std::string) {}
};

/** Defining the structural elements. **/

struct DummyNothing : public Nothing {};

struct DummyExternal : public External {};

struct DummyList : public List {
    DummyList(size_t n, bool) : length(n) {}

    size_t size() const { return length; }
    size_t length;

    void set(size_t, std::shared_ptr<Base>) {}
    void set_name(size_t, std::string) {}
};

/** Dummy provisioner. **/

struct DummyProvisioner {
    static Nothing* new_Nothing() { return (new DummyNothing); }

    static External* new_External(void*) { return (new DummyExternal); }

    template<class ... Args_>
    static List* new_List(Args_&& ... args) { return (new DummyList(std::forward<Args_>(args)...)); }

    template<class ... Args_>
    static IntegerVector* new_Integer(Args_&& ... args) { return (new DummyIntegerVector(std::forward<Args_>(args)...)); }

    template<class ... Args_>
    static NumberVector* new_Number(Args_&& ... args) { return (new DummyNumberVector(std::forward<Args_>(args)...)); }

    template<class ... Args_>
    static StringVector* new_String(Args_&& ... args) { return (new DummyStringVector(std::forward<Args_>(args)...)); }

    template<class ... Args_>
    static BooleanVector* new_Boolean(Args_&& ... args) { return (new DummyBooleanVector(std::forward<Args_>(args)...)); }

    template<class ... Args_>
    static Factor* new_Factor(Args_&& ... args) { return (new DummyFactor(std::forward<Args_>(args)...)); }
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
