# This tests the staging and loading of base lists.
# library(testthat); library(alabaster.base); source("test-list.R")

library(S4Vectors)

test_that("lists handle complex types correctly", {
    tmp <- tempfile()
    dir.create(tmp)

    vals <- list(
        A = 1.1,
        B = (1:5) * 0.5, # numeric...
        C = LETTERS[1:5],
        D1 = factor(LETTERS[1:5], LETTERS),
        D2 = factor(LETTERS[1:5], rev(LETTERS), ordered=TRUE),
        E = c(Sys.Date(), Sys.Date() - 1, Sys.Date() + 1)
    )

    info <- stageObject(vals, tmp, path="stuff")
    expect_identical(length(info$basic_list$children), 0L) 

    resource <- .writeMetadata(info, tmp)
    expect_true(file.exists(file.path(tmp, resource$path)))

    contents <- paste(readLines(file.path(tmp, "stuff/list.gz")), collapse='\n')
    expect_match(contents, "factor")

    roundtrip <- loadBaseList(info, tmp)
    expect_identical(roundtrip, vals)

    # Preserve non-alphabetical ordering.
    rvals <- rev(vals)
    info <- stageObject(rvals, tmp, path="stuff2")
    roundtrip <- loadBaseList(info, tmp)
    expect_identical(roundtrip, rvals)
})

test_that("lists can also be staged", {
    tmp <- tempfile()
    dir.create(tmp)

    library(S4Vectors)
    vals <- List(
        A = 1.1,
        B = (1:5) * 0.5, # numeric...
        C = LETTERS[1:5]
    )

    info <- stageObject(vals, tmp, path="stuff")
    roundtrip <- loadBaseList(info, tmp)
    expect_identical(roundtrip, as.list(vals))
})

test_that("names are properly supported", {
    tmp <- tempfile()
    dir.create(tmp)

    vals <- list(
        X = setNames((1:5) * 0.5, LETTERS[1:5]),
        Y = setNames(factor(letters[6:15]), 1:10)
    )

    info <- stageObject(vals, tmp, path="stuff")
    resource <- .writeMetadata(info, tmp)
    expect_true(file.exists(file.path(tmp, resource$path)))

    roundtrip <- loadBaseList(info, tmp)
    expect_identical(roundtrip, vals)
})

test_that("internal arrays are properly supported", {
    tmp <- tempfile()
    dir.create(tmp)

    vals <- list(
        A = array(runif(6), c(1,2,3)),
        B = matrix(runif(20), c(5,4), dimnames=list(letters[1:5], LETTERS[1:4])),
        C = matrix(runif(6), c(2,3), dimnames=list(NULL, LETTERS[1:3])),
        D = matrix(factor(letters[1:6]), c(2,3), dimnames=list(c("X", "Y"), NULL)) # also checking factor arrays.
    )

    info <- stageObject(vals, tmp, path="stuff")
    resource <- .writeMetadata(info, tmp)
    expect_true(file.exists(file.path(tmp, resource$path)))

    roundtrip <- loadBaseList(info, tmp)
    expect_equal(roundtrip, vals)
})

test_that("1-d arrays are properly supported", {
    tmp <- tempfile()
    dir.create(tmp)

    vals <- list(A = array(1, dimnames=list("X")))
    info <- stageObject(vals, tmp, path="stuff")
    resource <- .writeMetadata(info, tmp)
    expect_true(file.exists(file.path(tmp, resource$path)))

    roundtrip <- loadBaseList(info, tmp)
    expect_equal(roundtrip, vals)
})

test_that("data.frames are properly supported", {
    tmp <- tempfile()
    dir.create(tmp)

    vals <- list(
        X = data.frame(X1 = runif(2), X2 = rnorm(2), X3 = LETTERS[1:2]),
        Y = list(data.frame(Y1 = runif(5), Y2 = letters[1:5], row.names=as.character(5:1))),
        Z = data.frame(Z1 = runif(5), row.names=c("alpha", "bravo", "charlie", "delta", "echo"))
    )

    info <- stageObject(vals, tmp, path="stuff")
    resource <- .writeMetadata(info, tmp)
    expect_true(file.exists(file.path(tmp, resource$path)))

    roundtrip <- loadBaseList(info, tmp)
    expect_equal(roundtrip, vals)
})

test_that("unnamed lists are properly supported", {
    tmp <- tempfile()
    dir.create(tmp)

    vals <- list("A", 1.5, 2.3, list("C", list(DataFrame(X=1:10)), 3.5), (2:6)*1.5)

    info <- stageObject(vals, tmp, path="stuff")
    expect_identical(length(info$basic_list$children), 1L) 

    resource <- .writeMetadata(info, tmp)
    expect_true(file.exists(file.path(tmp, resource$path)))

    contents <- paste(readLines(file.path(tmp, "stuff/list.gz")), collapse='\n')
    expect_match(contents, "^\\[")

    roundtrip <- loadBaseList(info, tmp)
    expect_identical(roundtrip, vals)
})

test_that("partially named or duplicate named lists fail", {
    tmp <- tempfile()
    dir.create(tmp)

    expect_error(stageObject(list(1, A=2), dir=tmp, path="whee"), "non-empty")
    expect_error(stageObject(list(A=1, A=2), dir=tmp, path="whee2"), "multiple instances of 'A'")
})

test_that("external references work correctly", {
    tmp <- tempfile()
    dir.create(tmp)

    vals <- list(
        A = 1.1,
        B = list(
            C = DataFrame(X = 1:10),
            D = DataFrame(Y = 2:5)
        ),
        E = DataFrame(Z = runif(5))
    )

    info <- stageObject(vals, tmp, path="stuff")
    expect_identical(length(info$basic_list$children), 3L) 

    roundtrip <- loadBaseList(info, tmp)
    expect_equal(roundtrip, vals)
})

test_that("we handle lists with NAs and NULLs", {
    vals <- list(A=NA, B=c(1,2,3,NA), C=c("A", "B", NA))

    tmp <- tempfile()
    dir.create(tmp)
    info <- stageObject(vals, tmp, path="stuff")

    resource <- .writeMetadata(info, tmp)
    expect_true(file.exists(file.path(tmp, resource$path)))

    contents <- paste(readLines(file.path(tmp, "stuff/list.gz")), collapse='\n')
    expect_match(contents, "null")

    roundtrip <- loadBaseList(info, tmp)
    expect_equal(roundtrip, vals)

    # And now with NULLs.
    vals[1] <- list(NULL)
    info <- stageObject(vals, tmp, path="whee")

    resource <- .writeMetadata(info, tmp)
    expect_true(file.exists(file.path(tmp, resource$path)))

    contents <- paste(readLines(file.path(tmp, "stuff/list.gz")), collapse='\n')
    expect_match(contents, "null")

    roundtrip <- loadBaseList(info, tmp)
    expect_identical(vals["A"], list(A=NULL))
    expect_equal(roundtrip, vals)
})
