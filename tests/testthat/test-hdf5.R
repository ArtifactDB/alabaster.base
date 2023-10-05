# This tests the exported HDF5-related utilities.
# library(testthat); library(alabaster.base); source("test-hdf5.R")

test_that("transformations work as expected", {
    # Logical.
    out <- transformVectorForHdf5(c(TRUE, FALSE, FALSE))
    expect_identical(out$transformed, c(1L, 0L, 0L))
    expect_null(out$placeholder)

    out <- transformVectorForHdf5(c(TRUE, NA, FALSE))
    expect_identical(out$transformed, c(1L, -1L, 0L))
    expect_identical(out$placeholder, -1L)
    expect_identical(chooseMissingPlaceholderForHdf5(TRUE), -1L)

    # Integer.
    input <- c(1L, NA, 2L)
    out <- transformVectorForHdf5(input)
    expect_identical(out$transformed, input)
    expect_identical(out$placeholder, NA_integer_)
    expect_identical(chooseMissingPlaceholderForHdf5(1L), NA_integer_)

    input <- 1:3
    out <- transformVectorForHdf5(input)
    expect_identical(out$transformed, input)
    expect_null(out$placeholder)

    # Double.
    input <- c(1, NaN, 2)
    out <- transformVectorForHdf5(input)
    expect_identical(out$transformed, input)
    expect_null(out$placeholder)

    input <- c(1, NA, 2)
    out <- transformVectorForHdf5(input)
    expect_identical(out$transformed, input)
    expect_identical(out$placeholder, NA_real_)
    expect_identical(chooseMissingPlaceholderForHdf5(1), NA_real_)

    input <- c(1, NA, NaN)
    out <- transformVectorForHdf5(input)
    expect_identical(out$transformed, input)
    expect_identical(out$placeholder, NA_real_)

    # String.
    out <- transformVectorForHdf5(c("FOO", "NA", "BAR"))
    expect_identical(out$transformed, c("FOO", "NA", "BAR"))
    expect_null(out$placeholder)

    out <- transformVectorForHdf5(c("FOO", NA, "BAR"))
    expect_identical(out$transformed, c("FOO", "NA", "BAR"))
    expect_identical(out$placeholder, "NA")
    expect_identical(chooseMissingPlaceholderForHdf5("foobar"), "NA")

    out <- transformVectorForHdf5(c("FOO", NA, "NA"))
    expect_identical(out$transformed, c("FOO", "_NA", "NA"))
    expect_identical(out$placeholder, "_NA")
    expect_identical(chooseMissingPlaceholderForHdf5(c("NA", "foobar")), "_NA")
})
