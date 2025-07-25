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

    path <- absolutizePath(".")
    expect_false(path == ".")
    expect_identical(normalizePath(path), normalizePath(dirname(tmp)))

    write(file=tmp, letters)
    path <- absolutizePath(basename(tmp))
    expect_false(path == basename(tmp))
    expect_identical(readLines(path), letters)
    if (.Platform$OS.type == "unix") {
        expect_true(startsWith(path, "/"))
    }

    # Works with leading '..' references.
    tmp2 <- tempfile(tmpdir=dirname(tmp))
    dir.create(basename(tmp2), showWarnings=FALSE)
    setwd(basename(tmp2))
    path2 <- absolutizePath(file.path("..", basename(tmp)))
    expect_false(startsWith(path2, ".."))
    expect_identical(path, path2)
})
