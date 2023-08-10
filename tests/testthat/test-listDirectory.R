# Test loadDirectory on various bits and pieces.
# library(testthat); library(alabaster.base); source("test-loadDirectory.R")

tmp <- tempfile()
dir.create(tmp)

library(S4Vectors)
df <- DataFrame(A=1:10, B=LETTERS[1:10])
meta <- stageObject(df, tmp, path="whee")
writeMetadata(meta, tmp)

ll <- list(A=1, B=LETTERS, C=DataFrame(X=1:5))
meta <- stageObject(ll, tmp, path="stuff")
writeMetadata(meta, tmp)

redirect <- createRedirection(tmp, "whoop", "whee/simple.csv.gz")
writeMetadata(redirect, tmp)

test_that("listDirectory works as expected", {
    all.meta <- listDirectory(tmp, ignore.children=FALSE)
    paths <- vapply(all.meta, function(x) x$path, "")
    is_child <- vapply(all.meta, function(x) isTRUE(x$is_child), TRUE)
    expect_identical(unname(paths), names(all.meta))
    expect_true(any(is_child))

    child.meta <- listDirectory(tmp, ignore.children=TRUE)
    all.meta <- all.meta[!is_child]
    expect_identical(child.meta, all.meta)
    expect_true(length(child.meta) > 0)
})

test_that("loadDirectory works as expected", {
    all.obj <- loadDirectory(tmp, redirect.action="both")
    expect_identical(all.obj[["whoop"]], df)
    expect_identical(all.obj[["stuff/list.json.gz"]], ll)
    expect_identical(all.obj[["whee/simple.csv.gz"]], df)

    to.obj <- loadDirectory(tmp, redirect.action="to")
    expect_identical(to.obj, all.obj[setdiff(names(all.obj), "whoop")])

    from.obj <- loadDirectory(tmp, redirect.action="from")
    expect_identical(from.obj, all.obj[setdiff(names(all.obj), "whee/simple.csv.gz")])
})

