# Test stageObject on simple vectors.
# library(testthat); library(alabaster.base); source("test-vector.R")

test_that("vectors work correctly without names", {
    tmp <- tempfile()
    dir.create(tmp, recursive=TRUE)

    input <- LETTERS
    meta <- stageObject(input, tmp, path="foo")
    expect_identical(meta$atomic_vector$type, "string")
    writeMetadata(meta, tmp)
    expect_identical(loadAtomicVector(meta, tmp), input)

    vals <- runif(25)
    meta <- stageObject(vals, tmp, path="bar")
    expect_identical(meta$atomic_vector$type, "number")
    writeMetadata(meta, tmp)
    expect_equal(loadAtomicVector(meta, tmp), vals)

    vals <- as.integer(rpois(99, 10))
    meta <- stageObject(vals, tmp, path="whee")
    expect_identical(meta$atomic_vector$type, "integer")
    writeMetadata(meta, tmp)
    expect_identical(loadAtomicVector(meta, tmp), vals)

    vals <- rbinom(1000, 1, 0.5) > 0
    meta <- stageObject(vals, tmp, path="stuff")
    expect_identical(meta$atomic_vector$type, "boolean")
    writeMetadata(meta, tmp)
    expect_identical(loadAtomicVector(meta, tmp), vals)

    vals <- c(Sys.Date(), Sys.Date() + 100, Sys.Date() - 100)
    meta <- stageObject(vals, tmp, path="blah")
    expect_identical(meta$atomic_vector$type, "string")
    expect_identical(meta$atomic_vector$format, "date")
    writeMetadata(meta, tmp)
    expect_identical(loadAtomicVector(meta, tmp), vals)

    vals <- c(Sys.time(), Sys.time() + 100, Sys.time() - 100)
    meta <- stageObject(vals, tmp, path="gunk")
    expect_identical(meta$atomic_vector$type, "string")
    expect_identical(meta$atomic_vector$format, "date-time")
    writeMetadata(meta, tmp)
    expect_true(all(abs(loadAtomicVector(meta, tmp) - vals) < 1)) # sub-second resolution on the strings.
})

test_that("vectors work correctly without names in the new world", {
    tmp <- tempfile()
    dir.create(tmp, recursive=TRUE)

    input <- LETTERS
    saveObject(input, file.path(tmp, "foo"))
    expect_identical(readAtomicVector(file.path(tmp, "foo")), input)

    vals <- runif(25)
    saveObject(vals, file.path(tmp, "bar"))
    expect_equal(readAtomicVector(file.path(tmp, "bar")), vals)

    vals <- as.integer(rpois(99, 10))
    saveObject(vals, file.path(tmp, "whee"))
    expect_identical(readAtomicVector(file.path(tmp, "whee")), vals)

    vals <- rbinom(1000, 1, 0.5) > 0
    saveObject(vals, file.path(tmp, "stuff"))
    expect_identical(readAtomicVector(file.path(tmp, "stuff")), vals)

    vals <- c(Sys.Date(), Sys.Date() + 100, Sys.Date() - 100)
    saveObject(vals, file.path(tmp, "blah"))
    expect_identical(readAtomicVector(file.path(tmp, "blah")), vals)
})

test_that("vectors work correctly with names", {
    tmp <- tempfile()
    dir.create(tmp, recursive=TRUE)

    vals <- setNames(runif(26), LETTERS)
    meta <- stageObject(vals, tmp, path="bar")
    expect_identical(meta$atomic_vector$type, "number")
    writeMetadata(meta, tmp)
    expect_equal(loadAtomicVector(meta, tmp), vals)

    saveObject(vals, file.path(tmp, "foo"))
    expect_equal(readAtomicVector(file.path(tmp, "foo")), vals)
})

test_that("vectors preserve date-times as strings", {
    vals <- c(Sys.time(), Sys.time() + 100, Sys.time() - 100)

    tmp <- tempfile()
    dir.create(tmp)

    saveObject(vals, file.path(tmp, "gunk"))
    reloaded <- readAtomicVector(file.path(tmp, "gunk"))
    expect_s3_class(reloaded, "Rfc3339")
    expect_true(all(abs(as.POSIXct(reloaded) - vals) < 1)) # sub-second resolution on the strings.

    saveObject(reloaded, file.path(tmp, "foo"))
    expect_identical(readAtomicVector(file.path(tmp, "foo")), reloaded)
})

test_that("vectors convert package versions to strings", {
    vals <- as.package_version(c("1.0", "1.0.0", "1.2.1"))

    tmp <- tempfile()
    dir.create(tmp)

    saveObject(vals, file.path(tmp, "gunk"))
    reloaded <- readAtomicVector(file.path(tmp, "gunk"))
    expect_identical(reloaded, as.character(vals))
})

test_that("vectors work in VLS mode", {
    tmp <- tempfile()
    dir.create(tmp)

    x <- c("A", "BC", "DEFG", "HIJKL", "MNOP", "QRS", "TU", "V")
    saveObject(x, file.path(tmp, "basic"), character.vls=TRUE)
    reloaded <- readAtomicVector(file.path(tmp, "basic")) 
    expect_identical(x, reloaded)

    y <- x
    y[2] <- NA
    saveObject(y, file.path(tmp, "with_missing"), character.vls=TRUE)
    reloaded <- readAtomicVector(file.path(tmp, "with_missing")) 
    expect_identical(y, reloaded)

    y <- x
    names(y) <- seq_along(y)
    saveObject(y, file.path(tmp, "named"), character.vls=TRUE)
    reloaded <- readAtomicVector(file.path(tmp, "named")) 
    expect_identical(y, reloaded)

    y <- x
    y[4] <- strrep("HIJKL", 100)
    saveObject(y, file.path(tmp, "auto"), character.vls=NULL)
    reloaded <- readAtomicVector(file.path(tmp, "auto")) 
    expect_identical(y, reloaded)
})
