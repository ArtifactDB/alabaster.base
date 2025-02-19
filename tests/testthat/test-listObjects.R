# library(testthat); library(alabaster.base); source("test-listObjects.R")

test_that("listObjects works as expected", {
    tmp <- tempfile()
    dir.create(tmp)

    library(S4Vectors)
    df <- DataFrame(A=1:10, B=LETTERS[1:10])
    saveObject(df, file.path(tmp, "whee"))

    ll <- list(A=1, B=LETTERS, C=DataFrame(X=1:5))
    saveObject(ll, file.path(tmp, "stuff"))

    out <- listObjects(tmp)
    expect_identical(nrow(out), 2L)
    m <- match(c("whee", "stuff"), out$path)
    expect_true(!anyNA(m))
    expect_identical(out$type[m], c("data_frame", "simple_list"))
    expect_false(any(out$child[m]))

    out <- listObjects(tmp, include.children=TRUE)
    m <- match(c("whee", "stuff", "stuff/other_contents/0"), out$path)
    expect_identical(out$type[m], c("data_frame", "simple_list", "data_frame"))
    expect_identical(out$child[m], c(FALSE, FALSE, TRUE))
})
