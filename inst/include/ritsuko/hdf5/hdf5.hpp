#ifndef RITSUKO_HDF5_HPP
#define RITSUKO_HDF5_HPP

#include "Stream1dNumericDataset.hpp"
#include "Stream1dStringDataset.hpp"
#include "as_numeric_datatype.hpp"
#include "exceeds_limit.hpp"
#include "get_1d_length.hpp"
#include "get_dimensions.hpp"
#include "get_name.hpp"
#include "is_utf8_string.hpp"
#include "load_attribute.hpp"
#include "load_dataset.hpp"
#include "missing_placeholder.hpp"
#include "miscellaneous.hpp"
#include "open.hpp"
#include "pick_1d_block_size.hpp"
#include "pick_nd_block_dimensions.hpp"
#include "IterateNdDataset.hpp"
#include "validate_string.hpp"

/**
 * @file hdf5.hpp
 * @brief Umbrella header for **ritsuko**'s HDF5 utilities.
 */

namespace ritsuko {

/**
 * @namespace ritsuko::hdf5
 * @brief Assorted helper functions for HDF5 parsing. 
 */
namespace hdf5 {}

}

#endif
