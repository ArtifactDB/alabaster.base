# library(alabaster.base); library(testthat); source("test-absolutizePath.R")

test_that("absolutizePath cleans the path", {
    expect_false(grepl("~", absolutizePath("~")))

    tmp <- tempfile()
    write(file=tmp, letters)

    path <- absolutizePath(file.path(dirname(tmp), ".", basename(tmp)))
    expect_false(grepl("/\\./", path))
    expect_identical(readLines(path), letters)
    if (.Platform$OS.type == "unix") {
        expect_true(startsWith(path, "/"))
    }

    path <- absolutizePath(file.path(dirname(tmp), "super", "..", basename(tmp)))
    expect_false(grepl("/\\.\\./", path))
    expect_identical(readLines(path), letters)
    if (.Platform$OS.type == "unix") {
        expect_true(startsWith(path, "/"))
    }
})

test_that("absolutizePath resolves relative paths", {
    pwd <- getwd()
    on.exit(setwd(pwd))
    tmp <- tempfile()
    setwd(dirname(tmp))

    write(file=tmp, letters)
    path <- absolutizePath(basename(tmp))
    expect_identical(readLines(path), letters)
    if (.Platform$OS.type == "unix") {
        expect_true(startsWith(path, "/"))
    }
})
