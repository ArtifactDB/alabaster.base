# This tests the functions around the Rfc3339 class.
# library(testthat); library(alabaster.base); source("test-Rfc3339.R")

test_that("casting of datetimes works as expected", {
    out <- alabaster.base:::.cast_datetime("2023-12-04T14:41:19+01:00")
    expect_false(is.na(out))

    # Behaves with fractional seconds.
    out <- alabaster.base:::.cast_datetime("2023-12-04T14:41:19.21323+01:00")
    expect_false(is.na(out))

    # Behaves with 'Z'.
    out <- alabaster.base:::.cast_datetime("2023-12-04T14:41:19Z")
    expect_false(is.na(out))
})

test_that("Coercion to Rfc3339 works as expected", {
    out <- as.Rfc3339(c("2022-01-02T02:03:05.22Z", "blah", "2023-12-04T14:41:19.21323+01:00"))
    expect_identical(is.na(out), c(FALSE, TRUE, FALSE))
    expect_true(is.Rfc3339(out))

    out <- as.Rfc3339(Sys.time() + 1:10)
    expect_identical(length(out), 10L)
    expect_false(anyNA(out))

    out <- as.Rfc3339(as.POSIXlt(Sys.time() + 1:10))
    expect_identical(length(out), 10L)
    expect_false(anyNA(out))

    out2 <- as.Rfc3339(out)
    expect_identical(out, out2)

    out <- as.Rfc3339(1:10)
    expect_identical(sum(is.na(out)), 10L)

    out <- as.Rfc3339(c(NA_character_, "2022-01-02T02:03:05.22Z")) 
    expect_identical(is.na(out), c(TRUE, FALSE))

    # Preserves names.
    out <- as.Rfc3339(c(A="2022-01-02T02:03:05.22Z", B="blah", C="2023-12-04T14:41:19.21323+01:00"))
    expect_identical(names(out), LETTERS[1:3])
})

test_that("Coercion from Rfc339 works as expected", {
    rfc <- as.Rfc3339(Sys.time() + 1:10)
    coerced <- as.character(rfc)
    expect_identical(sum(!is.na(coerced)), 10L)

    coerced <- as.POSIXct(rfc)
    expect_s3_class(coerced, "POSIXct")
    expect_false(anyNA(coerced))

    coerced <- as.POSIXlt(rfc)
    expect_s3_class(coerced, "POSIXlt")
    expect_false(anyNA(coerced))

    # Respects names.
    names(rfc) <- rev(LETTERS[1:10])
    coerced <- as.POSIXlt(rfc)
    expect_identical(names(coerced), rev(LETTERS[1:10]))
})

test_that("Rfc3339 subset methods works as expected", {
    rfc <- as.Rfc3339(Sys.time() + 1:10)
    sub <- rfc[2:6]
    expect_s3_class(sub, "Rfc3339")
    expect_identical(unclass(sub), unclass(rfc)[2:6])

    sub <- rfc[[10]]
    expect_s3_class(sub, "Rfc3339")
    expect_identical(unclass(sub), unclass(rfc)[10])

    copy <- rfc
    val <- as.Rfc3339(Sys.time() + 10000 + 6:10)
    copy[1:5] <- val
    expect_s3_class(sub, "Rfc3339")
    expected <- unclass(rfc)
    expected[1:5] <- unclass(val)
    expect_identical(unclass(expected), unclass(copy))

    copy <- rfc
    val <- as.Rfc3339(Sys.time() + 10000)
    copy[[5]] <- val
    expect_s3_class(sub, "Rfc3339")
    expected <- unclass(rfc)
    expected[[5]] <- unclass(val)
    expect_identical(unclass(expected), unclass(copy))

    # Assignment fails if the things are stupid.
    copy <- rfc
    copy[1:2] <- c("aaron", "bravo")
    copy[[10]] <- "charlie"
    expect_identical(which(is.na(copy)), c(1L, 2L, 10L))

    # Respects names.
    copy <- rfc
    names(copy) <- letters[1:10]
    expect_identical(names(copy), letters[1:10])
    sub <- copy[c("a", "b", "c")]
    expect_s3_class(sub, "Rfc3339")
    expect_identical(unclass(sub), unclass(copy)[1:3])
})

test_that("Rfc3339 combining methods works as expected", {
    rfc <- as.Rfc3339(Sys.time() + 1:10)
    val <- as.Rfc3339(Sys.time() + 10000 + 6:10)
    combined <- c(rfc, val)
    expect_s3_class(combined, "Rfc3339")
    expect_identical(unclass(combined), c(unclass(rfc), unclass(val)))

    combined <- c(rfc, LETTERS, val)
    expect_s3_class(combined, "Rfc3339")
    expect_identical(unclass(combined), c(unclass(rfc), rep(NA, length(LETTERS)), unclass(val)))
})

test_that("Rfc3339 behaves nicely with AsIs", {
    rfc <- I(as.Rfc3339(Sys.time()))
    expect_true(is(rfc, "Rfc3339"))
    expect_true(is(rfc, "AsIs"))

    y <- rfc[1]
    expect_true(is(y, "Rfc3339"))
    y <- c(rfc, rfc)
    expect_true(is(y, "Rfc3339"))

    z <- as.POSIXct(rfc)
    expect_true(is(z, "POSIXct"))
})
