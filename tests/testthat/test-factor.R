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

test_that("factors work correctly without names in the new world", {
    tmp <- tempfile()
    dir.create(tmp, recursive=TRUE)

    input <- factor(LETTERS)
    saveObject(input, file.path(tmp, "foo"))
    expect_identical(readBaseFactor(file.path(tmp, "foo")), input)

    vals <- factor(LETTERS[5:20], LETTERS)
    saveObject(vals, file.path(tmp, "bar"))
    expect_identical(readBaseFactor(file.path(tmp, "bar")), vals)

    vals <- factor(LETTERS, LETTERS[2:24])
    saveObject(vals, file.path(tmp, "stuff"))
    expect_identical(readBaseFactor(file.path(tmp, "stuff")), vals)

    vals <- factor(LETTERS, rev(LETTERS), ordered=TRUE)
    saveObject(vals, file.path(tmp, "whee"))
    expect_identical(readBaseFactor(file.path(tmp, "whee")), vals)
})

test_that("factors work correctly with names", {
    tmp <- tempfile()
    dir.create(tmp, recursive=TRUE)

    vals <- setNames(factor(LETTERS, rev(LETTERS)), letters)
    meta <- stageObject(vals, tmp, path="bar")
    writeMetadata(meta, tmp)
    expect_identical(loadBaseFactor(meta, tmp), vals)

    saveObject(vals, file.path(tmp, "whee"))
    expect_identical(readBaseFactor(file.path(tmp, "whee")), vals)
})
