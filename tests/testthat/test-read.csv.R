# This tests the raw CSV reader in a variety of situations.
# library(testthat); library(alabaster.base); source("test-read.csv.R")

df <- data.frame(
    asdasd = LETTERS[1:10], 
    qwerty = runif(10), 
    stuff = rbinom(10, 1, 0.5) == 1,
    compx = rnorm(10) + rnorm(10) * 1i
)

write.csv2 <- function(file, ...) {
    handle <- file(file, "wb")
    write.csv(file=handle, ..., eol="\n")
    close(handle)
}

test_that("read.csv3 handles the different types correctly", {
    path <- tempfile(fileext=".csv")
    write.csv2(file=path, df, row.names=FALSE)
    out <- alabaster.base:::read.csv3(path, compression="none", nrows=nrow(df))
    expect_equal(df, out)
})

test_that("read.csv3 handles weird names", {
    colnames(df) <- c("asdasd asdasd", "qwerty\nasdasd", "stuff,\"asdasd\"", "compx,5\n\"")
    path <- tempfile(fileext=".csv")
    write.csv2(file=path, df, row.names=FALSE)
    out <- alabaster.base:::read.csv3(path, compression="none", nrows=nrow(df))
    expect_equal(df, data.frame(out, check.names=FALSE))
})

test_that("read.csv3 handles missing values correctly", {
    df[1,1] <- NA
    df[2,2] <- NA
    df[3,3] <- NA
    df[4,4] <- NA

    path <- tempfile(fileext=".csv")
    write.csv2(file=path, df, row.names=FALSE)
    out <- alabaster.base:::read.csv3(path, compression="none", nrows=nrow(df))
    expect_equal(df, data.frame(out, check.names=FALSE))
})

test_that("read.csv3 handles all-missing columns correctly", {
    df$asdasd <- NA
    path <- tempfile(fileext=".csv")
    write.csv2(file=path, df, row.names=FALSE)
    out <- alabaster.base:::read.csv3(path, compression="none", nrows=nrow(df))
    expect_equal(df, data.frame(out, check.names=FALSE))
})

test_that("read.csv3 handles empty DFs correctly", {
    df <- df[0,]

    path <- tempfile(fileext=".csv")
    write.csv2(file=path, df, row.names=FALSE)

    out <- alabaster.base:::read.csv3(path, compression="none", nrows=nrow(df))
    expect_identical(nrow(out), 0L)
    expect_identical(colnames(df), colnames(out))
})

test_that("read.csv3 handles single- and no-column DFs correctly", {
    df <- data.frame(asdasd = 1:10)
    path <- tempfile(fileext=".csv")
    write.csv2(file=path, df, row.names=FALSE)
    out <- alabaster.base:::read.csv3(path, compression="none", nrows=nrow(df))
    expect_equal(df, data.frame(out, check.names=FALSE))

    write(character(11), file=path)
    out <- alabaster.base:::read.csv3(path, compression="none", nrows=nrow(df))
    expect_equal(df[,0], data.frame(out, check.names=FALSE))
})

test_that("read.csv3 handles gzip'd files correctly", {
    path <- tempfile(fileext=".csv.gz")
    write.csv(file=gzfile(path), df, row.names=FALSE)
    out <- alabaster.base:::read.csv3(path, compression="gzip", nrows=nrow(df))
    expect_equal(df, out)
})
