# Test stageObject on various bits and pieces.
# library(testthat); library(alabaster.base); source("test-removeObject.R")

library(S4Vectors)
df <- DataFrame(A=1:10, B=LETTERS[1:10])
ll <- list(A=1, B=LETTERS, C=DataFrame(X=1:5))

populate <- function() {
    tmp <- tempfile()
    dir.create(tmp)

    meta <- stageObject(df, tmp, path="whee")
    .writeMetadata(meta, tmp)

    meta <- stageObject(ll, tmp, path="stuff")
    .writeMetadata(meta, tmp)

    redirect <- .createRedirection(tmp, "whoop", "whee/simple.csv.gz")
    .writeMetadata(redirect, tmp)

    tmp
}

test_that("removeObject works correctly for direct calls", {
    tmp <- populate()
    expect_true(file.exists(file.path(tmp, "stuff/list.json.gz")))
    expect_true(file.exists(file.path(tmp, "whee/simple.csv.gz")))
    expect_true(file.exists(file.path(tmp, "whee/simple.csv.gz.json")))
    expect_true(file.exists(file.path(tmp, "whoop.json")))

    removeObject(tmp, "whee/simple.csv.gz")
    expect_true(file.exists(file.path(tmp, "stuff/list.json.gz")))
    expect_false(file.exists(file.path(tmp, "whee/simple.csv.gz")))
    expect_false(file.exists(file.path(tmp, "whee/simple.csv.gz.json")))
    expect_false(file.exists(file.path(tmp, "whoop.json")))
})

test_that("removeObject works correctly for redirected calls", {
    tmp <- populate()
    expect_true(file.exists(file.path(tmp, "stuff/list.json.gz")))
    expect_true(file.exists(file.path(tmp, "whee/simple.csv.gz")))
    expect_true(file.exists(file.path(tmp, "whee/simple.csv.gz.json")))
    expect_true(file.exists(file.path(tmp, "whoop.json")))

    removeObject(tmp, "whoop")
    expect_true(file.exists(file.path(tmp, "stuff/list.json.gz")))
    expect_false(file.exists(file.path(tmp, "whee/simple.csv.gz")))
    expect_false(file.exists(file.path(tmp, "whee/simple.csv.gz.json")))
    expect_false(file.exists(file.path(tmp, "whoop.json")))
})
