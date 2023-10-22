#ifndef COMSERVATORY_READ_CSV_HPP
#define COMSERVATORY_READ_CSV_HPP

#include <vector>
#include <string>

#include "Field.hpp"
#include "Parser.hpp"
#include "read.hpp"

#include "byteme/byteme.hpp"

/**
 * @file ReadCsv.hpp
 *
 * @brief Backwards compatibility only, use `read()` instead.
 */

namespace comservatory {

/**
 * @cond
 */
class ReadCsv {
public:
    bool parallel = false;

    bool validate_only = false;

    const FieldCreator* creator = nullptr; 

    bool keep_subset = false;

    std::vector<std::string> keep_subset_names;

    std::vector<int> keep_subset_indices;

private:
    ReadOptions configure_options() const {
        ReadOptions opt;
        opt.parallel = parallel;
        opt.validate_only = validate_only;
        opt.creator = creator;
        opt.keep_subset = keep_subset;
        opt.keep_subset_names = keep_subset_names;
        opt.keep_subset_indices = keep_subset_indices;
        return opt;
    }

public:
    template<class Reader>
    Contents read(Reader& reader) const {
        return comservatory::read(reader, configure_options());
    }

    Contents read(const char* path) const {
        return read_file(path, configure_options());
    } 

    Contents read(std::string path) const {
        return read_file(path, configure_options());
    } 
};
/**
 * @endcond
 */

}

#endif
