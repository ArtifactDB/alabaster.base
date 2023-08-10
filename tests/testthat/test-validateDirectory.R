# This adds some tests for writeMetadata.
# library(testthat); library(alabaster.base); source("test-validateDirectory.R")

library(S4Vectors)
ncols <- 123
df <- DataFrame(
    X = rep(LETTERS[1:3], length.out=ncols),
    Y = runif(ncols)
)
df$Z <- DataFrame(AA = sample(ncols))

test_that("validateDirectory works as expected", {
    tmp <- tempfile()
    dir.create(tmp, recursive=TRUE)

    # Mocking up a directory.
    writeMetadata(stageObject(df, tmp, "foo"), tmp)
    writeMetadata(stageObject(df, tmp, "bar"), tmp)
    dir.create(file.path(tmp, "whee"))
    writeMetadata(stageObject(df, tmp, "whee/stuff"), tmp)
    writeMetadata(createRedirection(tmp, "whee/stuff", "whee/stuff/simple.csv.gz"), tmp)

    expect_error(validateDirectory(tmp), NA)
})

test_that("validateDirectory throws with invalid metadata", {
    tmp <- tempfile()
    dir.create(tmp, recursive=TRUE)
    info <- stageObject(df, tmp, "foo")

    info2 <- info
    info2$data_frame$YAY <- TRUE
    write(file=file.path(tmp, paste0(info$path, ".json")), jsonlite::toJSON(info2, pretty=TRUE, auto_unbox=TRUE, digits=NA))

    expect_error(validateDirectory(tmp), "data_frame")
    expect_error(validateDirectory(tmp, validate.metadata=FALSE), NA)
})

test_that("validateDirectory throws with invalid objects", {
    tmp <- tempfile()
    dir.create(tmp, recursive=TRUE)
    info <- stageObject(df, tmp, "foo")
    writeMetadata(info, tmp)
    write(file=file.path(tmp, info$path), "YAAAA")

    expect_error(validateDirectory(tmp), NA)
    expect_error(validateDirectory(tmp, attempt.load=TRUE), "foo/simple.csv.gz")
})

test_that("validateDirectory throws with inconsistent paths", {
    {
        tmp <- tempfile()
        dir.create(tmp, recursive=TRUE)

        # Path does not match the JSON file name.
        writeMetadata(stageObject(df, tmp, "foo"), tmp)
        file.rename(file.path(tmp, "foo/simple.csv.gz.json"), file.path(tmp, "foo/simple2.csv.gz.json"))

        expect_error(validateDirectory(tmp), "unexpected path")
    }

    {
        tmp <- tempfile()
        dir.create(tmp, recursive=TRUE)

        # Path does not exist.
        writeMetadata(stageObject(df, tmp, "foo"), tmp)
        file.remove(file.path(tmp, "foo/simple.csv.gz"))

        expect_error(validateDirectory(tmp), "non-existent path")
    }
})

test_that("validateDirectory throws with non-nested children", {
    {
        tmp <- tempfile()
        dir.create(tmp, recursive=TRUE)

        meta <- stageObject(df, tmp, "foo")
        oldnest <- meta$data_frame$columns[[3]]$resource$path

        newnest <- file.path("whee", basename(oldnest))
        meta$data_frame$columns[[3]]$resource$path <- newnest
        file.rename(file.path(tmp, dirname(oldnest)), file.path(tmp, "whee"))

        writeMetadata(meta, tmp)

        expect_error(validateDirectory(tmp), "references non-nested child")
    }

    # This time, a more subtle error because it doesn't belong in a subdirectory.
    {
        tmp <- tempfile()
        dir.create(tmp, recursive=TRUE)

        meta <- stageObject(df, tmp, "foo")
        oldnest <- meta$data_frame$columns[[3]]$resource$path

        newnest <- "foo/zzzz.csv.gz"
        meta$data_frame$columns[[3]]$resource$path <- newnest
        file.rename(file.path(tmp, oldnest), file.path(tmp, newnest))
        file.rename(file.path(tmp, paste0(oldnest, ".json")), file.path(tmp, paste0(newnest, '.json')))

        writeMetadata(meta, tmp)

        expect_error(validateDirectory(tmp), "references non-nested child")
    }
})

test_that("validateDirectory throws with non-child references", {
    tmp <- tempfile()
    dir.create(tmp, recursive=TRUE)

    meta <- stageObject(df, tmp, "foo")
    writeMetadata(meta, tmp)

    child <- meta$data_frame$columns[[3]]$resource$path
    target <- file.path(tmp, paste0(child, ".json"))
    writeLines(sub("\"is_child\": true", "\"is_child\": false", readLines(target)), con=target)

    expect_error(validateDirectory(tmp), "non-child object.*is referenced")
})

test_that("validateDirectory throws with multiple child references", {
    tmp <- tempfile()
    dir.create(tmp, recursive=TRUE)

    meta <- stageObject(df, tmp, "foo")
    meta$data_frame$columns <- meta$data_frame$columns[c(1,2,3,3)]
    writeMetadata(meta, tmp)

    expect_error(validateDirectory(tmp), "multiple references to child at")
})

test_that("validateDirectory throws with missing child object", {
    tmp <- tempfile()
    dir.create(tmp, recursive=TRUE)

    meta <- stageObject(df, tmp, "foo")
    writeMetadata(meta, tmp)

    file.remove(file.path(tmp, paste0(meta$data_frame$columns[[3]]$resource$path, ".json")))
    expect_error(validateDirectory(tmp), "missing child object")
})

test_that("validateDirectory throws with extra child object", {
    tmp <- tempfile()
    dir.create(tmp, recursive=TRUE)

    meta <- stageObject(df, tmp, "foo")
    meta$data_frame$columns <- meta$data_frame$columns[c(1,2)]
    writeMetadata(meta, tmp)

    expect_error(validateDirectory(tmp), "non-referenced child object")
})

test_that("validateDirectory throws with nested non-child object", {
    tmp <- tempfile()
    dir.create(tmp, recursive=TRUE)

    meta <- stageObject(df, tmp, "foo")
    child <- meta$data_frame$columns[[3]]$resource$path
    meta$data_frame$columns <- meta$data_frame$columns[c(1,2)]
    writeMetadata(meta, tmp)

    # Converting it into a non-child.
    target <- file.path(tmp, paste0(child, ".json"))
    writeLines(sub("\"is_child\": true", "\"is_child\": false", readLines(target)), con=target)

    expect_error(validateDirectory(tmp), "non-child object.*is nested")
})

test_that("validateDirectory throws with random extra objects", {
    tmp <- tempfile()
    dir.create(tmp, recursive=TRUE)

    meta <- stageObject(df, tmp, "foo")
    writeMetadata(meta, tmp)
    writeLines(con=file.path(tmp, "foo", "bar.txt"), letters)

    expect_error(validateDirectory(tmp), "unknown file")
})

test_that("validateDirectory handles redirects correctly", {
    {
        tmp <- tempfile()
        dir.create(tmp, recursive=TRUE)

        # Mocking up a directory.
        writeMetadata(stageObject(df, tmp, "foo"), tmp)
        red <- createRedirection(tmp, "foo", "foo/simple.csv.gz")
        red$path <- "whee"
        writeMetadata(red, tmp)
        file.rename(file.path(tmp, "whee.json"), file.path(tmp, "foo.json"))

        expect_error(validateDirectory(tmp), "references an unexpected path")
    }

    {
        tmp <- tempfile()
        dir.create(tmp, recursive=TRUE)

        # Mocking up a directory.
        writeMetadata(stageObject(df, tmp, "foo"), tmp)
        red <- createRedirection(tmp, "foo", "foo/simple.csv.gz")
        red$path <- "foo/simple.csv.gz"
        writeMetadata(red, tmp)

        expect_error(validateDirectory(tmp), "redirection from existing path")
    }

    {
        tmp <- tempfile()
        dir.create(tmp, recursive=TRUE)

        # Mocking up a directory.
        writeMetadata(stageObject(df, tmp, "foo"), tmp)
        red <- createRedirection(tmp, "foo", "foo/whee.csv.gz")
        writeMetadata(red, tmp)

        expect_error(validateDirectory(tmp), "invalid redirection")
    }
})

