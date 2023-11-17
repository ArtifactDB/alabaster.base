# Test staging and loading of DataFrameFactors.
# library(testthat); library(alabaster.base); source("test-DataFrameFactor.R")

library(S4Vectors)

test_that("staging and loading of data frame factors work as expected", {
    tmp <- tempfile()
    dir.create(tmp)

    df <- DataFrame(A=sample(3, 100, replace=TRUE), B=sample(letters[1:3], 100, replace=TRUE))
    X <- DataFrameFactor(x=df)
    staged <- stageObject(X, tmp, path="test1")
    expect_error(writeMetadata(staged, tmp), NA)

    expect_true(file.exists(file.path(tmp, staged$path)))
    expect_true(file.exists(file.path(tmp, staged$data_frame_factor$levels$resource$path)))
    expect_identical(staged$factor$compression, "gzip")

    Y <- loadDataFrameFactor(staged, project=tmp)
    expect_identical(X, Y)

    # Works with factor names.
    rownames(df) <- 1:100
    X <- DataFrameFactor(x=df)
    expect_identical(rownames(X), as.character(1:100))

    staged <- stageObject(X, tmp, path="test2")
    expect_error(writeMetadata(staged, tmp), NA)

    Y <- loadDataFrameFactor(staged, project=tmp)
    expect_identical(X, Y)

    # Works with internal rownames.
    rownames(X@levels) <- paste0("INTERNAL_", seq_len(nrow(X@levels)))

    staged <- stageObject(X, tmp, path="test3")
    expect_error(writeMetadata(staged, tmp), NA)

    Y <- loadDataFrameFactor(staged, project=tmp)
    expect_identical(X, Y)
})

test_that("staging and loading of data frame factors work in the new world", {
    tmp <- tempfile()
    dir.create(tmp)

    df <- DataFrame(A=sample(3, 100, replace=TRUE), B=sample(letters[1:3], 100, replace=TRUE))
    X <- DataFrameFactor(x=df)
    saveObject(X, file.path(tmp, "test1"))
    Y <- readObject(file.path(tmp, "test1"))
    expect_identical(X, Y)

    # Works with factor names.
    rownames(df) <- 1:100
    X <- DataFrameFactor(x=df)
    saveObject(X, file.path(tmp, "test2"))
    Y <- readDataFrameFactor(file.path(tmp, "test2"))
    expect_identical(X, Y)

    # Works with internal rownames.
    rownames(X@levels) <- paste0("INTERNAL_", seq_len(nrow(X@levels)))
    saveObject(X, file.path(tmp, "test3"))
    Y <- readDataFrameFactor(file.path(tmp, "test3"))
    expect_identical(X, Y)
})
