# This tests the staging hints in stageObject's ANY method.
# library(alabaster.base); library(testthat); source("test-stageANY.R")

test_that("stageObject redirects to an appropriate hint with name checks", {
    a <- matrix(runif(100), 10, 10)
    tmp <- tempfile()
    dir.create(tmp) 

    if (alabaster.base:::package.exists("alabaster.matrix")) {
        expect_error(info <- stageObject(a, tmp, "foo"), NA)
        expect_identical(as.character(info[["$schema"]]), "hdf5_dense_array/v1.json")
        expect_true("matrix" %in% alabaster.base:::package.lookup$found)
    } else {
        expect_error(stageObject(a, tmp, "foo"), "alabaster.matrix")
    }
})

test_that("stageObject redirects to an appropriate hint via is()", {
    library(Matrix)
    x <- rsparsematrix(100, 10, 0.05)
    tmp <- tempfile()
    dir.create(tmp) 

    if (alabaster.base:::package.exists("alabaster.matrix")) {
        expect_error(info <- stageObject(x, tmp, "foo"), NA)
        expect_identical(as.character(info[["$schema"]]), "hdf5_sparse_matrix/v1.json")
        expect_true("dgCMatrix" %in% alabaster.base:::package.lookup$found)
    } else {
        expect_error(stageObject(x, tmp, "foo"), "alabaster.matrix")
    }
})

test_that("stageObject fails for unknown classes", {
    setClass("MyClass", slots=c(x = "integer"))
    a <- new("MyClass", x = 1L)

    tmp <- tempfile()
    dir.create(tmp) 
    expect_error(stageObject(a, tmp, "foo"), "MyClass")
})

test_that("stageObject fails for existing paths", {
    a <- S4Vectors::DataFrame(X = 1L)

    tmp <- tempfile()
    dir.create(tmp) 
    stageObject(a, tmp, "foo")
    expect_error(stageObject(a, tmp, "foo"), "existing path")

    tmp <- tempfile()
    dir.create(tmp) 
    expect_error(info <- stageObject(a, tmp, "."), NA)
    expect_identical(info$path, "./simple.csv.gz")
})

