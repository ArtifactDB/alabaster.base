# Test the acquireMetadata handling of the redirection.
# library(testthat); library(alabaster.base); source("test-acquireMetadata.R")

library(S4Vectors)
df <- DataFrame(A = 1:5, B = LETTERS[1:5])

test_that("acquireMetadata works correctly in simple cases", {
    tmp <- tempfile()
    dir.create(tmp, recursive=TRUE)
    info <- stageObject(df, tmp, "rnaseq")
    writeMetadata(info, tmp)

    expect_identical(acquireFile(tmp, info$path), file.path(tmp, info$path))
    meta <- acquireMetadata(tmp, info$path)
    expect_identical(meta$`$schema`, info$`$schema`)
    expect_identical(meta$path, info$path)

    # Works if we pass the JSON path directly.
    meta2 <- acquireMetadata(tmp, paste0(info$path, ".json"))
    expect_identical(meta, meta2)
})

test_that("acquireMetadata correctly redirects to other things", {
    tmp <- tempfile()
    dir.create(tmp, recursive=TRUE)
    info <- stageObject(df, tmp, "rnaseq")
    writeMetadata(info, tmp)

    redirect <- createRedirection(tmp, "rnaseq", "rnaseq/simple.csv.gz")
    writeMetadata(redirect, tmp)

    check <- jsonlite::fromJSON(file.path(tmp, "rnaseq.json"), simplifyVector=FALSE)
    expect_identical(check$path, "rnaseq")
    expect_identical(check$redirection$targets[[1]]$location, "rnaseq/simple.csv.gz")

    # Redirects automatically.
    meta <- acquireMetadata(tmp, "rnaseq")
    expect_identical(meta$`$schema`, info$`$schema`)
    expect_identical(meta$path, info$path)
})
