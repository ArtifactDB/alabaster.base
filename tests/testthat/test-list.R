# This tests the staging and loading of base lists.
# library(testthat); library(alabaster.base); source("test-list.R")

library(S4Vectors)

vals <- list(
    A = 1.1,
    B = (1:5) * 0.5, # numeric...
    C = LETTERS[1:5],
    D1 = factor(LETTERS[1:5], LETTERS),
    D2 = factor(LETTERS[1:5], rev(LETTERS), ordered=TRUE),
    E = c(Sys.Date(), Sys.Date() - 1, Sys.Date() + 1)
)

test_that("lists handle complex types correctly", {
    tmp <- tempfile()
    dir.create(tmp)

    info <- stageObject(vals, tmp, path="stuff")
    expect_identical(length(info$simple_list$children), 0L) 
    expect_identical(info$json_simple_list$compression, "gzip")

    resource <- .writeMetadata(info, tmp)
    expect_true(file.exists(file.path(tmp, resource$path)))

    roundtrip <- loadBaseList(info, tmp)
    expect_identical(roundtrip, vals)

    # Preserve non-alphabetical ordering.
    rvals <- rev(vals)
    info <- stageObject(rvals, tmp, path="stuff2")
    roundtrip <- loadBaseList(info, tmp)
    expect_identical(roundtrip, rvals)
})

test_that("lists work in HDF5 mode", {
    old <- .saveBaseListFormat("hdf5")
    on.exit(.saveBaseListFormat(old))

    tmp <- tempfile()
    dir.create(tmp)

    info <- stageObject(vals, tmp, path="stuff")
    expect_identical(length(info$simple_list$children), 0L) 

    attrs <- rhdf5::h5readAttributes(file.path(tmp, "stuff/list.h5"), "contents/data/2/data")
    expect_null(attrs[["missing-value-placeholder"]]) # no missing values... yet.

    attrs <- rhdf5::h5readAttributes(file.path(tmp, "stuff/list.h5"), "contents/data/3")
    expect_identical(attrs$uzuki_type, "factor")

    resource <- .writeMetadata(info, tmp)
    expect_true(file.exists(file.path(tmp, resource$path)))

    roundtrip <- loadBaseList(info, tmp)
    expect_identical(roundtrip, vals)

    # Preserve non-alphabetical ordering.
    rvals <- rev(vals)
    info <- stageObject(rvals, tmp, path="stuff2")
    roundtrip <- loadBaseList(info, tmp)
    expect_identical(roundtrip, rvals)
})

test_that("S4 Lists can also be staged", {
    tmp <- tempfile()
    dir.create(tmp)

    library(S4Vectors)
    vals <- List(
        A = 1.1,
        B = (1:5) * 0.5,
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
    expect_match(info[["$schema"]], "json_simple_list")
    resource <- .writeMetadata(info, tmp)
    expect_true(file.exists(file.path(tmp, resource$path)))

    roundtrip <- loadBaseList(info, tmp)
    expect_identical(roundtrip, vals)

    # Works in HDF5 mode.
    old <- .saveBaseListFormat("hdf5")
    on.exit(.saveBaseListFormat(old))

    info <- stageObject(vals, tmp, path="hstuff")
    resource <- .writeMetadata(info, tmp)
    expect_match(info[["$schema"]], "hdf5_simple_list")

    roundtrip <- loadBaseList(info, tmp)
    expect_identical(roundtrip, vals)
})

test_that("data.frames cause dispatch to external objects", {
    tmp <- tempfile()
    dir.create(tmp)

    vals <- list(
        X = data.frame(X1 = runif(2), X2 = rnorm(2), X3 = LETTERS[1:2]),
        Y = list(data.frame(Y1 = runif(5), Y2 = letters[1:5], row.names=as.character(5:1))),
        Z = data.frame(Z1 = runif(5), row.names=c("alpha", "bravo", "charlie", "delta", "echo"))
    )

    info <- stageObject(vals, tmp, path="stuff")
    expect_match(info[["$schema"]], "json_simple_list")
    resource <- .writeMetadata(info, tmp)
    expect_true(file.exists(file.path(tmp, resource$path)))

    roundtrip <- loadBaseList(info, tmp)
    roundtrip$X <- as.data.frame(roundtrip$X)
    roundtrip$Y[[1]] <- as.data.frame(roundtrip$Y[[1]])
    roundtrip$Z <- as.data.frame(roundtrip$Z)

    expect_equal(roundtrip, vals)

    # Works in HDF5 mode.
    old <- .saveBaseListFormat("hdf5")
    on.exit(.saveBaseListFormat(old))

    info <- stageObject(vals, tmp, path="hstuff")
    resource <- .writeMetadata(info, tmp)
    expect_match(info[["$schema"]], "hdf5_simple_list")

    roundtrip <- loadBaseList(info, tmp)
    roundtrip$X <- as.data.frame(roundtrip$X)
    roundtrip$Y[[1]] <- as.data.frame(roundtrip$Y[[1]])
    roundtrip$Z <- as.data.frame(roundtrip$Z)

    expect_equal(roundtrip, vals) # still equality, not identity, because DF's are saved as CSVs right now.
})

test_that("unnamed lists are properly supported", {
    tmp <- tempfile()
    dir.create(tmp)

    vals <- list("A", 1.5, 2.3, list("C", list(DataFrame(X=1:10)), 3.5), (2:6)*1.5)

    info <- stageObject(vals, tmp, path="stuff")
    expect_match(info[["$schema"]], "json_simple_list")
    expect_identical(length(info$simple_list$children), 1L) 

    resource <- .writeMetadata(info, tmp)
    expect_true(file.exists(file.path(tmp, resource$path)))

    roundtrip <- loadBaseList(info, tmp)
    expect_identical(roundtrip, vals)

    # Works in HDF5 mode.
    old <- .saveBaseListFormat("hdf5")
    on.exit(.saveBaseListFormat(old))

    info <- stageObject(vals, tmp, path="hstuff")
    resource <- .writeMetadata(info, tmp)
    expect_match(info[["$schema"]], "hdf5_simple_list")

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
    expect_identical(length(info$simple_list$children), 3L) 
    expect_match(info[["$schema"]], "json_simple_list")

    roundtrip <- loadBaseList(info, tmp)
    expect_equal(roundtrip, vals)

    # Works in HDF5 mode.
    old <- .saveBaseListFormat("hdf5")
    on.exit(.saveBaseListFormat(old))

    info <- stageObject(vals, tmp, path="hstuff")
    resource <- .writeMetadata(info, tmp)
    expect_match(info[["$schema"]], "hdf5_simple_list")

    roundtrip <- loadBaseList(info, tmp)
    expect_equal(roundtrip, vals) # still equality, not identity, because DF's are saved as CSVs right now.
})

test_that("we handle lists with NAs", {
    vals <- list(A=NA, B1=c(1,2,3,NA), B2=c(4L, 5L, NA), C=c("A", "B", NA))

    tmp <- tempfile()
    dir.create(tmp)
    info <- stageObject(vals, tmp, path="stuff")
    expect_match(info[["$schema"]], "json_simple_list")

    resource <- .writeMetadata(info, tmp)
    expect_true(file.exists(file.path(tmp, resource$path)))

    roundtrip <- loadBaseList(info, tmp)
    expect_equal(roundtrip, vals)

    # More difficult NAs.
    vals$C <- c(vals$C, "NA")

    tmp <- tempfile()
    dir.create(tmp)
    info <- stageObject(vals, tmp, path="stuff")

    resource <- .writeMetadata(info, tmp)
    expect_true(file.exists(file.path(tmp, resource$path)))

    roundtrip <- loadBaseList(info, tmp)
    expect_equal(roundtrip, vals)

    # Works in HDF5 mode.
    old <- .saveBaseListFormat("hdf5")
    on.exit(.saveBaseListFormat(old))

    info <- stageObject(vals, tmp, path="hstuff")
    resource <- .writeMetadata(info, tmp)
    expect_match(info[["$schema"]], "hdf5_simple_list")

    attrs <- rhdf5::h5readAttributes(file.path(tmp, "hstuff/list.h5"), "contents/data/3/data")
    expect_identical(attrs[["missing-value-placeholder"]], "_NA")

    roundtrip <- loadBaseList(info, tmp)
    expect_identical(roundtrip, vals)
})

test_that("we handle the various float specials", {
    vals <- list(XXX=c(1.2, Inf, 2.3, -Inf, 3.4, NaN, 4.5, NA))

    tmp <- tempfile()
    dir.create(tmp)
    info <- stageObject(vals, tmp, path="stuff")
    expect_match(info[["$schema"]], "json_simple_list")

    resource <- .writeMetadata(info, tmp)
    expect_true(file.exists(file.path(tmp, resource$path)))

    roundtrip <- loadBaseList(info, tmp)
    expect_equal(roundtrip, vals)

    # Same for HDF5.
    old <- .saveBaseListFormat("hdf5")
    on.exit(.saveBaseListFormat(old))

    info <- stageObject(vals, tmp, path="hstuff")
    resource <- .writeMetadata(info, tmp)
    expect_match(info[["$schema"]], "hdf5_simple_list")

    roundtrip <- loadBaseList(info, tmp)
    expect_identical(roundtrip, vals)
})

test_that("we handle lists with NULLs", {
    vals <- list(NULL, list(list(NULL), NULL))

    tmp <- tempfile()
    dir.create(tmp)
    info <- stageObject(vals, tmp, path="whee")
    expect_match(info[["$schema"]], "json_simple_list")

    resource <- .writeMetadata(info, tmp)
    expect_true(file.exists(file.path(tmp, resource$path)))

    roundtrip <- loadBaseList(info, tmp)
    expect_identical(roundtrip, vals)

    # Works in HDF5 mode.
    old <- .saveBaseListFormat("hdf5")
    on.exit(.saveBaseListFormat(old))

    info <- stageObject(vals, tmp, path="hstuff")
    resource <- .writeMetadata(info, tmp)
    expect_match(info[["$schema"]], "hdf5_simple_list")

    roundtrip <- loadBaseList(info, tmp)
    expect_identical(roundtrip, vals)
})
