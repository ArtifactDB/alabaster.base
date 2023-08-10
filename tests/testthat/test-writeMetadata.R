# This adds some tests for writeMetadata.
# library(testthat); library(alabaster.base); source("test-writeMetadata.R")

library(S4Vectors)

ncols <- 123
df <- DataFrame(
    stuff = rep(LETTERS[1:3], length.out=ncols),
    blah = 0, # placeholder
    foo = seq_len(ncols),
    whee = as.numeric(10 + seq_len(ncols)),
    rabbit = 1,
    birthday = rep(Sys.Date(), ncols) - sample(100, ncols, replace=TRUE)
)
df$blah <- factor(df$stuff, LETTERS[10:1])
df$rabbit <- factor(df$stuff, LETTERS[1:3], ordered=TRUE)

test_that("writing saves the MD5 sums", {
    tmp <- tempfile()
    dir.create(tmp)

    info <- stageObject(df, tmp, "rnaseq")
    out <- writeMetadata(info, tmp)

    expect_identical(out$path, info$path) 
    round <- jsonlite::fromJSON(file.path(tmp, paste0(out$path, ".json")))
    expect_identical(round$md5sum, digest::digest(file=file.path(tmp, out$path)))

    # ... unless the MD5sum was already available, in which case it's just used.
    info$md5sum <- "WHEE"
    out <- writeMetadata(info, tmp)
    round <- jsonlite::fromJSON(file.path(tmp, paste0(out$path, ".json")))
    expect_identical(round$md5sum, "WHEE")
})

test_that("writing strips the dots", {
    tmp <- tempfile()
    dir.create(tmp)

    info <- stageObject(df, tmp, "./whee")
    out <- writeMetadata(info, tmp)

    round <- jsonlite::fromJSON(file.path(tmp, paste0(out$path, ".json")))
    expect_identical(round$path, "whee/simple.csv.gz")
})

test_that("writing doesn't allow windows-style separators", {
    tmp <- tempfile()
    dir.create(tmp)
    expect_error(stageObject(df, tmp, "whee\\asdasd"), "Windows-style")
})

test_that("writing respects the package attribute", {
    tmp <- tempfile()
    dir.create(tmp)

    info <- stageObject(df, tmp, "./whee")
    attr(info[["$schema"]], "package") <- "FOOBAR"
    expect_error(writeMetadata(info, tmp), "failed to find")
})
