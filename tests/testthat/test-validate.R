# This checks that the registration functions for validate() work correctly.
# library(alabaster.base); library(testthat); source("test-validate.R")

test_that("validation registration works as expected", {
    tmp <- tempfile()
    dir.create(tmp)
    saveObjectFile(tmp, "foobar")
    expect_error(validateObject(tmp), "foobar");

    registerValidateObjectFunction("foobar", function(path, metadata) {})
    expect_error(validateObject(tmp), NA);

    registerValidateObjectFunction("foobar", function(path, metadata) { stop("YAY") }, existing="old")
    expect_error(validateObject(tmp), NA);

    registerValidateObjectFunction("foobar", function(path, metadata) { stop("YAY") }, existing="new")
    expect_error(validateObject(tmp), "YAY");

    expect_error(registerValidateObjectFunction("foobar", function(path, metadata) { stop("YAY") }, existing="error"), "already been registered")

    registerValidateObjectFunction("foobar", NULL)
    expect_error(validateObject(tmp), "foobar");
})

test_that("height registration works as expected", {
    library(S4Vectors)
    X <- DataFrame(X=S4Vectors::I(DataFrame(Y=1:10)))
    tmp <- tempfile()
    saveObject(X, tmp)

    saveObjectFile(file.path(tmp, "other_columns", "0"), "foobar", list())
    registerValidateObjectFunction("foobar", function(path, metadata) {})
    on.exit(registerValidateObjectFunction("foobar", NULL))

    expect_error(validateObject(tmp), "no registered 'height'");

    registerValidateObjectHeightFunction("foobar", function(path, metadata) { nrow(X) })
    expect_error(validateObject(tmp), NA);

    registerValidateObjectHeightFunction("foobar", NULL)
    expect_error(validateObject(tmp), "no registered 'height'");
})

test_that("dimensions registration works as expected", {
    # Don't have an object that calls dimensions() at the C++ level, so we just
    # check that the registration works correctly.
    registerValidateObjectDimensionsFunction("foobar", function(path, metadata) c(1L, 2L))
    expect_true(registerValidateObjectDimensionsFunction("foobar", NULL))
})

test_that("conversion from R list to C++ JSON during validation works as expected", {
    ncols <- 123
    df <- DataFrame(
        stuff = rep(LETTERS[1:3], length.out=ncols),
        whee = as.numeric(10 + seq_len(ncols))
    )

    tmp <- tempfile()
    saveObject(df, tmp)
    expect_error(validateObject(tmp), NA)

    meta <- readObjectFile(tmp)
    expect_error(validateObject(tmp, metadata=meta), NA)

    meta$type <- "FOO"
    expect_error(validateObject(tmp, metadata=meta), "no registered 'validate'")
})

test_that("conversion from C++ JSON to R list during validation works as expected", {
    registerValidateObjectFunction("foobar", function(path, metadata) { 
        stopifnot(metadata$type == "foobar")
        if (metadata$foobar==2) {
            stop("ARGGGGH")
        }
    })
    on.exit(registerValidateObjectFunction("foobar", NULL))

    tmp <- tempfile()
    dir.create(tmp)
    saveObjectFile(tmp, "foobar", list(foobar=3))
    expect_error(validateObject(tmp), NA);

    saveObjectFile(tmp, "foobar", list(foobar=2))
    expect_error(validateObject(tmp), "ARGGGGH");
})

test_that("interface registration works as expected", {
    library(S4Vectors)
    X <- DataFrame(X=S4Vectors::I(DataFrame(Y=1:10)))
    tmp <- tempfile()
    saveObject(X, tmp)

    mtmp <- file.path(tmp, "other_annotations")
    dir.create(mtmp)
    saveObjectFile(mtmp, "foobar")

    expect_error(validateObject(tmp), "'SIMPLE_LIST' interface");
    registerValidateObjectSatisfiesInterface("foobar", "SIMPLE_LIST")
    on.exit(registerValidateObjectSatisfiesInterface("foobar", "SIMPLE_LIST", action="remove"), add=TRUE, after=FALSE)

    expect_error(validateObject(tmp), "no registered 'validate'");
    registerValidateObjectFunction("foobar", function(path, metadata) {})
    on.exit(registerValidateObjectFunction("foobar", NULL), add=TRUE, after=FALSE)

    expect_error(validateObject(tmp), NA)

    registerValidateObjectSatisfiesInterface("foobar", "SIMPLE_LIST", action="remove")
    expect_error(validateObject(tmp), "'SIMPLE_LIST' interface");
})
