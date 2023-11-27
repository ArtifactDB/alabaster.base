# This checks the anyMissing() function.
# library(testthat); library(alabaster.base); source("test-anyMissing.R")

test_that("anyMissing works correctly", {
    expect_true(anyNA(c(NaN)))
    expect_true(anyNA(c(NA)))
    expect_false(anyMissing(c(NaN)))
    expect_true(anyMissing(c(NA)))
})

test_that("is.missing works correctly", {
    expect_identical(is.na(c(NA, NaN)), c(TRUE, TRUE))
    expect_identical(is.missing(c(NA, NaN)), c(TRUE, FALSE))
})
