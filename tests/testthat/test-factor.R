# Test stageObject on factors.
# library(testthat); library(alabaster.base); source("test-factor.R")

test_that("factors work correctly without names", {
    tmp <- tempfile()
    dir.create(tmp, recursive=TRUE)

    input <- factor(LETTERS)
    meta <- stageObject(input, tmp, path="foo")
    writeMetadata(meta, tmp)
    expect_identical(loadBaseFactor(meta, tmp), input)

    vals <- factor(LETTERS[5:20], LETTERS)
    meta <- stageObject(vals, tmp, path="bar")
    writeMetadata(meta, tmp)
    expect_identical(loadBaseFactor(meta, tmp), vals)

    vals <- factor(LETTERS, LETTERS[2:24])
    meta <- stageObject(vals, tmp, path="stuff")
    writeMetadata(meta, tmp)
    expect_identical(loadBaseFactor(meta, tmp), vals)

    vals <- factor(LETTERS, rev(LETTERS), ordered=TRUE)
    meta <- stageObject(vals, tmp, path="whee")
    writeMetadata(meta, tmp)
    expect_identical(loadBaseFactor(meta, tmp), vals)
})

test_that("factors work correctly with names", {
    tmp <- tempfile()
    dir.create(tmp, recursive=TRUE)

    vals <- setNames(factor(LETTERS, rev(LETTERS)), letters)
    meta <- stageObject(vals, tmp, path="bar")
    writeMetadata(meta, tmp)
    expect_identical(loadBaseFactor(meta, tmp), vals)
})
