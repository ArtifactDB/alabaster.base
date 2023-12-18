# This checks the readObject function and its related registration.
# library(testthat); library(alabaster.base); source("test-readObject.R")

test_that("registerReadObjectFunction works as expected", {
    old <- readObjectFunctionRegistry()$data_frame
    expect_false(is.null(old))
    on.exit(registerReadObjectFunction("data_frame", old, existing="new"))

    # No-op.
    registerReadObjectFunction("data_frame", 2)
    expect_identical(readObjectFunctionRegistry()$data_frame, old)

    # Replacement.
    registerReadObjectFunction("data_frame", "FOOBAR", existing="new")
    expect_identical(readObjectFunctionRegistry()$data_frame, "FOOBAR")

    # Error.
    expect_error(registerReadObjectFunction("data_frame", "YAY", existing="error"), "already been registered")
})
