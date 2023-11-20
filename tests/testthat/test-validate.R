# This checks that the registration functions for validate() work correctly.
# library(alabaster.base); library(testthat); source("test-validate.R")

test_that("validation registration works as expected", {
    tmp <- tempfile()
    dir.create(tmp)
    write("foobar", file.path(tmp, "OBJECT"))
    expect_error(validateObject(tmp), "foobar");

    registerValidateObjectFunction("foobar", function(path) {})
    expect_error(validateObject(tmp), NA);

    registerValidateObjectFunction("foobar", NULL)
    expect_error(validateObject(tmp), "foobar");
})

test_that("height registration works as expected", {
    library(S4Vectors)
    X <- DataFrame(X=S4Vectors::I(DataFrame(Y=1:10)))
    tmp <- tempfile()
    saveObject(X, tmp)

    write("foobar", file.path(tmp, "other_columns", "0", "OBJECT"))
    registerValidateObjectFunction("foobar", function(path) {})
    on.exit(registerValidateObjectFunction("foobar", NULL))

    expect_error(validateObject(tmp), "no registered 'height'");

    registerValidateObjectHeightFunction("foobar", function(path) { nrow(X) })
    expect_error(validateObject(tmp), NA);

    registerValidateObjectHeightFunction("foobar", NULL)
    expect_error(validateObject(tmp), "no registered 'height'");
})

