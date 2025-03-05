# This tests the staging and loading of base lists.
# library(testthat); library(alabaster.base); source("test-list.R")

library(S4Vectors)

vals <- list(
    A = 1:5,
    B = (1:5) * 0.5, # numeric...
    C = LETTERS[1:5],
    D1 = factor(LETTERS[1:5], LETTERS),
    D2 = factor(LETTERS[1:5], rev(LETTERS), ordered=TRUE),
    E = c(Sys.Date(), Sys.Date() - 1, Sys.Date() + 1)
)

test_that("lists handle complex types correctly", {
    tmp <- tempfile()
    dir.create(tmp)

    info <- stageObject(vals, tmp, path="stuff")
    expect_identical(length(info$simple_list$children), 0L) 
    expect_identical(info$json_simple_list$compression, "gzip")

    resource <- writeMetadata(info, tmp)
    expect_true(file.exists(file.path(tmp, resource$path)))

    roundtrip <- loadBaseList(info, tmp)
    expect_identical(roundtrip, vals)

    # Preserve non-alphabetical ordering.
    rvals <- rev(vals)
    info <- stageObject(rvals, tmp, path="stuff2")
    roundtrip <- loadBaseList(info, tmp)
    expect_identical(roundtrip, rvals)

    # Works in the new world.
    tmp2 <- tempfile()
    saveObject(vals, tmp2)
    expect_identical(readObject(tmp2), vals)

    tmp2 <- tempfile()
    saveObject(rvals, tmp2)
    expect_identical(readObject(tmp2), rvals)
})

test_that("list format switch works as expected", {
    expect_identical(saveBaseListFormat(), "json.gz")
    expect_identical(saveBaseListFormat("hdf5"), "json.gz")
    expect_identical(saveBaseListFormat(), "hdf5")
    expect_identical(saveBaseListFormat(NULL), "hdf5")
    expect_identical(saveBaseListFormat(), "json.gz")
})

test_that("lists work in HDF5 mode", {
    old <- saveBaseListFormat("hdf5")
    on.exit(saveBaseListFormat(old))

    tmp <- tempfile()
    dir.create(tmp)

    info <- stageObject(vals, tmp, path="stuff")
    expect_identical(length(info$simple_list$children), 0L) 

    attrs <- rhdf5::h5readAttributes(file.path(tmp, "stuff/list.h5"), "contents/data/2/data")
    expect_null(attrs[["missing-value-placeholder"]]) # no missing values... yet.

    attrs <- rhdf5::h5readAttributes(file.path(tmp, "stuff/list.h5"), "contents/data/3")
    expect_identical(attrs$uzuki_type, "factor")

    resource <- writeMetadata(info, tmp)
    expect_true(file.exists(file.path(tmp, resource$path)))

    roundtrip <- loadBaseList(info, tmp)
    expect_identical(roundtrip, vals)

    # Preserve non-alphabetical ordering.
    rvals <- rev(vals)
    info <- stageObject(rvals, tmp, path="stuff2")
    roundtrip <- loadBaseList(info, tmp)
    expect_identical(roundtrip, rvals)

    # Works in the new world.
    tmp2 <- tempfile()
    saveObject(vals, tmp2)
    expect_identical(readObject(tmp2), vals)

    tmp2 <- tempfile()
    saveObject(rvals, tmp2)
    expect_identical(readObject(tmp2), rvals)
})

test_that("S4 Lists can also be staged", {
    tmp <- tempfile()
    dir.create(tmp)

    library(S4Vectors)
    vals <- List(
        A = 1:5,
        B = (1:5) * 0.5,
        C = LETTERS[1:5]
    )

    info <- stageObject(vals, tmp, path="stuff")
    roundtrip <- loadBaseList(info, tmp)
    expect_identical(roundtrip, as.list(vals))

    # Works in the new world.
    tmp2 <- tempfile()
    saveObject(vals, tmp2)
    expect_identical(readObject(tmp2), as.list(vals))
})

test_that("names are properly supported", {
    tmp <- tempfile()
    dir.create(tmp)

    vals <- list(
        X = setNames((1:5) * 0.5, LETTERS[1:5]),
        Y = setNames(factor(letters[6:15]), 1:10)
    )

    info <- stageObject(vals, tmp, path="stuff")
    expect_match(info[["$schema"]], "json_simple_list")
    resource <- writeMetadata(info, tmp)
    expect_true(file.exists(file.path(tmp, resource$path)))

    roundtrip <- loadBaseList(info, tmp)
    expect_identical(roundtrip, vals)

    tmp2 <- tempfile()
    saveObject(vals, tmp2)
    expect_identical(readObject(tmp2), vals)

    # Works in HDF5 mode.
    old <- saveBaseListFormat("hdf5")
    on.exit(saveBaseListFormat(old))

    info <- stageObject(vals, tmp, path="hstuff")
    resource <- writeMetadata(info, tmp)
    expect_match(info[["$schema"]], "hdf5_simple_list")

    roundtrip <- loadBaseList(info, tmp)
    expect_identical(roundtrip, vals)

    tmp2 <- tempfile()
    saveObject(vals, tmp2)
    expect_identical(readObject(tmp2), vals)
})

test_that("data.frames cause dispatch to external objects", {
    tmp <- tempfile()
    dir.create(tmp)

    vals <- list(
        X = data.frame(X1 = runif(2), X2 = rnorm(2), X3 = LETTERS[1:2]),
        Y = list(data.frame(Y1 = runif(5), Y2 = letters[1:5], row.names=as.character(5:1))),
        Z = data.frame(Z1 = runif(5), row.names=c("alpha", "bravo", "charlie", "delta", "echo"))
    )

    info <- stageObject(vals, tmp, path="stuff")
    expect_match(info[["$schema"]], "json_simple_list")
    resource <- writeMetadata(info, tmp)
    expect_true(file.exists(file.path(tmp, resource$path)))

    roundtrip <- loadBaseList(info, tmp)
    roundtrip$X <- as.data.frame(roundtrip$X)
    roundtrip$Y[[1]] <- as.data.frame(roundtrip$Y[[1]])
    roundtrip$Z <- as.data.frame(roundtrip$Z)

    expect_equal(roundtrip, vals)

    tmp2 <- tempfile()
    saveObject(vals, tmp2)
    roundtrip <- readObject(tmp2)
    roundtrip$X <- as.data.frame(roundtrip$X)
    roundtrip$Y[[1]] <- as.data.frame(roundtrip$Y[[1]])
    roundtrip$Z <- as.data.frame(roundtrip$Z)
    expect_identical(roundtrip, vals)

    # Works in HDF5 mode.
    old <- saveBaseListFormat("hdf5")
    on.exit(saveBaseListFormat(old))

    info <- stageObject(vals, tmp, path="hstuff")
    resource <- writeMetadata(info, tmp)
    expect_match(info[["$schema"]], "hdf5_simple_list")

    roundtrip <- loadBaseList(info, tmp)
    roundtrip$X <- as.data.frame(roundtrip$X)
    roundtrip$Y[[1]] <- as.data.frame(roundtrip$Y[[1]])
    roundtrip$Z <- as.data.frame(roundtrip$Z)

    expect_equal(roundtrip, vals) # still equality, not identity, because DF's are saved as CSVs right now.

    tmp2 <- tempfile()
    saveObject(vals, tmp2)
    roundtrip <- readObject(tmp2)
    roundtrip$X <- as.data.frame(roundtrip$X)
    roundtrip$Y[[1]] <- as.data.frame(roundtrip$Y[[1]])
    roundtrip$Z <- as.data.frame(roundtrip$Z)
    expect_identical(roundtrip, vals)
})

test_that("unnamed lists are properly supported", {
    tmp <- tempfile()
    dir.create(tmp)

    vals <- list("A", 1.5, 2.3, list("C", list(DataFrame(X=1:10)), 3.5), (2:6)*1.5)

    info <- stageObject(vals, tmp, path="stuff")
    expect_match(info[["$schema"]], "json_simple_list")
    expect_identical(length(info$simple_list$children), 1L) 

    resource <- writeMetadata(info, tmp)
    expect_true(file.exists(file.path(tmp, resource$path)))

    roundtrip <- loadBaseList(info, tmp)
    expect_identical(roundtrip, vals)

    tmp2 <- tempfile()
    saveObject(vals, tmp2)
    expect_identical(readObject(tmp2), vals)

    # Works in HDF5 mode.
    old <- saveBaseListFormat("hdf5")
    on.exit(saveBaseListFormat(old))

    info <- stageObject(vals, tmp, path="hstuff")
    resource <- writeMetadata(info, tmp)
    expect_match(info[["$schema"]], "hdf5_simple_list")

    roundtrip <- loadBaseList(info, tmp)
    expect_identical(roundtrip, vals)

    tmp2 <- tempfile()
    saveObject(vals, tmp2)
    expect_identical(readObject(tmp2), vals)
})

test_that("partially named or duplicate named lists fail", {
    tmp <- tempfile()
    dir.create(tmp)

    expect_error(stageObject(list(1, A=2), dir=tmp, path="whee"), "non-empty")
    expect_error(stageObject(list(A=1, A=2), dir=tmp, path="whee2"), "multiple instances of 'A'")
})

test_that("external references work correctly", {
    tmp <- tempfile()
    dir.create(tmp)

    vals <- list(
        A = 1:5,
        B = list(
            C = DataFrame(X = 1:10),
            D = DataFrame(Y = 2:5)
        ),
        E = DataFrame(Z = runif(5))
    )

    info <- stageObject(vals, tmp, path="stuff")
    expect_identical(length(info$simple_list$children), 3L) 
    expect_match(info[["$schema"]], "json_simple_list")

    roundtrip <- loadBaseList(info, tmp)
    expect_equal(roundtrip, vals)

    tmp2 <- tempfile()
    saveObject(vals, tmp2)
    expect_identical(readObject(tmp2), vals)

    # Works in HDF5 mode.
    old <- saveBaseListFormat("hdf5")
    on.exit(saveBaseListFormat(old))

    info <- stageObject(vals, tmp, path="hstuff")
    resource <- writeMetadata(info, tmp)
    expect_match(info[["$schema"]], "hdf5_simple_list")

    roundtrip <- loadBaseList(info, tmp)
    expect_equal(roundtrip, vals) # still equality, not identity, because DF's are saved as CSVs right now.

    tmp2 <- tempfile()
    saveObject(vals, tmp2)
    expect_identical(readObject(tmp2), vals)
})

test_that("external references work correctly with lots of objects", {
    tmp <- tempfile()
    dir.create(tmp)

    # We use lots of objects to check that the sorting order is reproduced;
    # this is not always safe to assume, as the directories in other_content
    # are sorted by string and not number, e.g., 10 sorts before 2.
    vals <- list()
    for (i in 1:20) {
        vals[[i]] <- DataFrame(X = i)
    }

    tmp <- tempfile()
    saveObject(vals, tmp)
    expect_identical(readObject(tmp), vals)

    # Same result with HDF5.
    tmp2 <- tempfile()
    saveObject(vals, tmp2, list.format="hdf5")
    expect_identical(readObject(tmp2), vals)
})

test_that("we handle lists with NAs", {
    vals <- list(
        A=NA, 
        B1=c(1,2,3,NA), 
        B2=c(4L, 5L, NA), 
        C=c("A", "B", NA), 
        D=factor(c("A", NA, "C"), c("a", "A", "b", "B", "c", "C"))
    )

    tmp <- tempfile()
    dir.create(tmp)
    info <- stageObject(vals, tmp, path="stuff")
    expect_match(info[["$schema"]], "json_simple_list")

    resource <- writeMetadata(info, tmp)
    expect_true(file.exists(file.path(tmp, resource$path)))

    roundtrip <- loadBaseList(info, tmp)
    expect_equal(roundtrip, vals)

    tmp2 <- tempfile()
    saveObject(vals, tmp2)
    expect_identical(readObject(tmp2), vals)

    # More difficult NAs.
    vals$C <- c(vals$C, "NA")

    tmp <- tempfile()
    dir.create(tmp)
    info <- stageObject(vals, tmp, path="stuff")

    resource <- writeMetadata(info, tmp)
    expect_true(file.exists(file.path(tmp, resource$path)))

    roundtrip <- loadBaseList(info, tmp)
    expect_equal(roundtrip, vals)

    tmp2 <- tempfile()
    saveObject(vals, tmp2)
    expect_identical(readObject(tmp2), vals)

    # Works in HDF5 mode.
    old <- saveBaseListFormat("hdf5")
    on.exit(saveBaseListFormat(old))

    tmp <- tempfile()
    dir.create(tmp)
    info <- stageObject(vals, tmp, path="hstuff")
    resource <- writeMetadata(info, tmp)
    expect_match(info[["$schema"]], "hdf5_simple_list")

    attrs <- rhdf5::h5readAttributes(file.path(tmp, "hstuff/list.h5"), "contents/data/1/data")
    place <- attrs[["missing-value-placeholder"]]
    expect_true(is.na(place) && !is.nan(place))
    attrs <- rhdf5::h5readAttributes(file.path(tmp, "hstuff/list.h5"), "contents/data/3/data")
    expect_identical(attrs[["missing-value-placeholder"]], "_NA")
    attrs <- rhdf5::h5readAttributes(file.path(tmp, "hstuff/list.h5"), "contents/data/4/data")
    place <- attrs[["missing-value-placeholder"]]
    expect_identical(place, -1L)

    roundtrip <- loadBaseList(info, tmp)
    expect_identical(roundtrip, vals)

    tmp2 <- tempfile()
    saveObject(vals, tmp2)
    expect_identical(readObject(tmp2), vals)

    # Avoid unnecessary NA attributes for NaNs, unless they're mixed in with NAs.
    revals <- list(A=c(NaN, 1.0, 3, NaN), B=c(NaN, 1.0, NA, NaN))

    tmp <- tempfile()
    dir.create(tmp)
    info <- stageObject(revals, tmp, path="hstuff2")
    resource <- writeMetadata(info, tmp)

    attrs <- rhdf5::h5readAttributes(file.path(tmp, "hstuff2/list.h5"), "contents/data/0/data")
    place <- attrs[["missing-value-placeholder"]]
    expect_null(place) # avoid saving an unnecessary placeholder.
    attrs <- rhdf5::h5readAttributes(file.path(tmp, "hstuff2/list.h5"), "contents/data/1/data")
    place <- attrs[["missing-value-placeholder"]]
    expect_identical(place, Inf)

    roundtrip <- loadBaseList(info, tmp)
    expect_identical(roundtrip, revals)

    tmp2 <- tempfile()
    saveObject(vals, tmp2)
    expect_identical(readObject(tmp2), vals)
})

test_that("we handle lists with minimum integers", {
    vals <- list(A=c(1L,2L,3L,NA))

    tmp <- tempfile()
    dir.create(tmp)

    info <- stageObject(vals, tmp, path="stuff")
    expect_match(info[["$schema"]], "json_simple_list")

    fpath <- file.path(tmp, info$path)
    x <- jsonlite::fromJSON(fpath, simplifyVector=FALSE)
    x$values[[1]]$values[4] <- -2^31
    write(jsonlite::toJSON(x, auto_unbox=TRUE), file=gzfile(fpath))

    roundtrip <- loadBaseList(info, tmp)
    expect_equal(roundtrip$A, c(1,2,3,-2^31))

    tmp2 <- tempfile()
    saveObject(vals, tmp2)
    expect_identical(readObject(tmp2), vals)

    # Works for HDF5.
    old <- saveBaseListFormat("hdf5")
    on.exit(saveBaseListFormat(old))

    info <- stageObject(vals, tmp, path="stuff2")
    fpath <- file.path(tmp, info$path)
    rhdf5::h5deleteAttribute(fpath, "contents/data/0/data", "missing-value-placeholder")

    roundtrip <- loadBaseList(info, tmp)
    expect_equal(roundtrip$A, c(1,2,3,-2^31))

    tmp2 <- tempfile()
    saveObject(vals, tmp2)
    expect_identical(readObject(tmp2), vals)
})

test_that("loaders work correctly from HDF5 with non-default placeholders", {
    vals <- list(a=c(1,2,3), b=c(4L, 5L, 6L), c=c(7, 8, NaN))

    old <- saveBaseListFormat("hdf5")
    on.exit(saveBaseListFormat(old))

    tmp <- tempfile()
    dir.create(tmp)
    info <- stageObject(vals, tmp, path="hstuff2")
    resource <- writeMetadata(info, tmp)

    fpath <- file.path(tmp, info$path)
    addMissingPlaceholderAttributeForHdf5(fpath, "contents/data/0/data", 1)
    addMissingPlaceholderAttributeForHdf5(fpath, "contents/data/1/data", 5L)
    addMissingPlaceholderAttributeForHdf5(fpath, "contents/data/2/data", NaN)

    roundtrip <- loadBaseList(info, tmp)
    expect_identical(roundtrip$a, c(NA, 2, 3))
    expect_identical(roundtrip$b, c(4L, NA, 6L))
    expect_identical(roundtrip$c, c(7, 8, NA))

    tmp2 <- tempfile()
    saveObject(vals, tmp2)
    expect_identical(readObject(tmp2), vals)
})

test_that("we handle the various float specials", {
    vals <- list(XXX=c(1.2, Inf, 2.3, -Inf, 3.4, NaN, 4.5, NA))

    tmp <- tempfile()
    dir.create(tmp)
    info <- stageObject(vals, tmp, path="stuff")
    expect_match(info[["$schema"]], "json_simple_list")

    resource <- writeMetadata(info, tmp)
    expect_true(file.exists(file.path(tmp, resource$path)))

    roundtrip <- loadBaseList(info, tmp)
    expect_equal(roundtrip, vals)

    tmp2 <- tempfile()
    saveObject(vals, tmp2)
    expect_identical(readObject(tmp2), vals)

    # Same for HDF5.
    old <- saveBaseListFormat("hdf5")
    on.exit(saveBaseListFormat(old))

    info <- stageObject(vals, tmp, path="hstuff")
    resource <- writeMetadata(info, tmp)
    expect_match(info[["$schema"]], "hdf5_simple_list")

    roundtrip <- loadBaseList(info, tmp)
    expect_identical(roundtrip, vals)

    tmp2 <- tempfile()
    saveObject(vals, tmp2)
    expect_identical(readObject(tmp2), vals)
})

test_that("we handle lists with NULLs", {
    vals <- list(NULL, list(list(NULL), NULL))

    tmp <- tempfile()
    dir.create(tmp)
    info <- stageObject(vals, tmp, path="whee")
    expect_match(info[["$schema"]], "json_simple_list")

    resource <- writeMetadata(info, tmp)
    expect_true(file.exists(file.path(tmp, resource$path)))

    roundtrip <- loadBaseList(info, tmp)
    expect_identical(roundtrip, vals)

    tmp2 <- tempfile()
    saveObject(vals, tmp2)
    expect_identical(readObject(tmp2), vals)

    # Works in HDF5 mode.
    old <- saveBaseListFormat("hdf5")
    on.exit(saveBaseListFormat(old))

    info <- stageObject(vals, tmp, path="hstuff")
    resource <- writeMetadata(info, tmp)
    expect_match(info[["$schema"]], "hdf5_simple_list")

    roundtrip <- loadBaseList(info, tmp)
    expect_identical(roundtrip, vals)

    tmp2 <- tempfile()
    saveObject(vals, tmp2)
    expect_identical(readObject(tmp2), vals)
})

test_that("we handle lists with times", {
    now <- as.POSIXct(round(Sys.time()), tz="")
    vals <- list(now, list(list(now + 10000), c(X=now + 400000, Y=now + 1000000)))
    to_posix <- function(x, fun) {
        if (is.list(x)) {
            lapply(x, to_posix, fun=fun)
        } else {
            fun(x)
        }
    }

    tmp <- tempfile()
    dir.create(tmp)
    info <- stageObject(vals, tmp, path="whee")
    expect_match(info[["$schema"]], "json_simple_list")

    roundtrip <- loadBaseList(info, tmp)
    expect_equal(roundtrip, vals)

    tmp2 <- tempfile()
    saveObject(vals, tmp2)
    reloaded <- readObject(tmp2)
    expect_s3_class(reloaded[[1]], "Rfc3339")
    expect_equal(to_posix(reloaded, as.POSIXct), vals)

    # Works in HDF5 mode.
    old <- saveBaseListFormat("hdf5")
    on.exit(saveBaseListFormat(old))

    info <- stageObject(vals, tmp, path="hstuff")
    resource <- writeMetadata(info, tmp)
    expect_match(info[["$schema"]], "hdf5_simple_list")

    roundtrip <- loadBaseList(info, tmp)
    expect_equal(roundtrip, vals)

    tmp2 <- tempfile()
    saveObject(vals, tmp2)
    reloaded <- readObject(tmp2)
    expect_equal(to_posix(reloaded, as.POSIXct), vals)

    # Works with POSIXlt objects, though these lose some precision when they go to POSIXct on back-conversion.
    now2 <- as.POSIXlt(now, tz="")
    vals2 <- list(now2, list(list(now2 + 10000), c(X=now2 + 400000, Y=now2 + 1000000)))

    info2 <- stageObject(vals, tmp, path="whee2")
    roundtrip2 <- loadBaseList(info2, tmp)
    expect_equal(to_posix(roundtrip2, as.POSIXlt), vals2)

    tmp2 <- tempfile()
    saveObject(vals, tmp2)
    reloaded <- readObject(tmp2)
    expect_equal(to_posix(reloaded, as.POSIXlt), vals2)
})

test_that("lists correctly distinguish between scalars and length-1 vectors", {
    ll <- list(
        A1 = 1L,
        A2 = I(2L),
        B1 = 1.5,
        B2 = I(2.5),
        C1 = "foo",
        C2 = I("bar"),
        D1 = TRUE,
        D2 = I(FALSE),
        E1 = Sys.Date(),
        E2 = I(Sys.Date()),
        F1 = as.Rfc3339(Sys.time()),
        F2 = I(as.Rfc3339(Sys.time()))
    )

    tmp <- tempfile()
    saveObject(ll, tmp)
    reloaded <- readObject(tmp)
    expect_equal(ll, reloaded)

    # Checking that the values are indeed scalar.
    y <- jsonlite::fromJSON(file.path(tmp, "list_contents.json.gz"), simplifyVector=FALSE)
    expect_type(y$values[[1]]$values, "integer")
    expect_type(y$values[[2]]$values, "list")
    expect_type(y$values[[3]]$values, "double")
    expect_type(y$values[[4]]$values, "list")
    expect_type(y$values[[5]]$values, "character")
    expect_type(y$values[[6]]$values, "list")
    expect_type(y$values[[7]]$values, "logical")
    expect_type(y$values[[8]]$values, "list")
    expect_type(y$values[[9]]$values, "character")
    expect_type(y$values[[10]]$values, "list")
    expect_type(y$values[[11]]$values, "character")
    expect_type(y$values[[12]]$values, "list")

    # Same for HDF5.
    tmp2 <- tempfile()
    saveObject(ll, tmp2, list.format="hdf5")
    reloaded2 <- readObject(tmp2)
    expect_equal(ll, reloaded2)

    # Checking that the values are indeed scalar.
    fhandle <- rhdf5::H5Fopen(file.path(tmp2, "list_contents.h5"))
    ghandle <- rhdf5::H5Gopen(fhandle, "simple_list")
    dhandle <- rhdf5::H5Gopen(ghandle, "data")
    peek_at_shape <- function(name) {
        ihandle <- rhdf5::H5Gopen(dhandle, name)
        on.exit(rhdf5::H5Gclose(ihandle), add=TRUE, after=FALSE)
        xhandle <- rhdf5::H5Dopen(ihandle, "data")
        on.exit(rhdf5::H5Dclose(xhandle), add=TRUE, after=FALSE)
        shandle <- rhdf5::H5Dget_space(xhandle)
        on.exit(rhdf5::H5Sclose(shandle), add=TRUE, after=FALSE)
        rhdf5::H5Sget_simple_extent_dims(shandle)$size
    }
    expect_identical(peek_at_shape("0"), integer(0))
    expect_identical(peek_at_shape("1"), 1L)
    expect_identical(peek_at_shape("2"), integer(0))
    expect_identical(peek_at_shape("3"), 1L)
    expect_identical(peek_at_shape("4"), integer(0))
    expect_identical(peek_at_shape("5"), 1L)
    expect_identical(peek_at_shape("6"), integer(0))
    expect_identical(peek_at_shape("7"), 1L)
    expect_identical(peek_at_shape("8"), integer(0))
    expect_identical(peek_at_shape("9"), 1L)
    expect_identical(peek_at_shape("10"), integer(0))
    expect_identical(peek_at_shape("11"), 1L)
})

test_that("lists work correctly in legacy mode (JSON)", {
    tmp <- tempfile()
    dir.create(tmp)
    info <- stageObject(vals, tmp, path="stuff", .version=1)
    writeMetadata(info, tmp)

    roundtrip <- loadBaseList(info, tmp)
    expect_identical(roundtrip, vals)
})

test_that("lists work correctly in legacy mode (HDF5)", {
    old <- saveBaseListFormat("hdf5")
    on.exit(saveBaseListFormat(old))

    tmp <- tempfile()
    dir.create(tmp)
    info <- stageObject(vals, tmp, path="stuff", .version=1)
    writeMetadata(info, tmp)

    roundtrip <- loadBaseList(info, tmp)
    expect_identical(roundtrip, vals)

    # Where it gets really fun is with the NA/NaN mixtures.
    vals$F <- c(NA, 1:5)
    vals$G <- c(NA, NaN, 0.5)

    info <- stageObject(vals, tmp, path="stuff2", .version=2)
    writeMetadata(info, tmp)
    roundtrip <- loadBaseList(info, tmp)
    expect_identical(roundtrip, vals)

    attrs <- rhdf5::h5readAttributes(file.path(tmp, "stuff2/list.h5"), "contents/data/6/data")
    expect_identical(attrs[["missing-value-placeholder"]], NA_integer_)
    attrs <- rhdf5::h5readAttributes(file.path(tmp, "stuff2/list.h5"), "contents/data/7/data")
    expect_identical(attrs[["missing-value-placeholder"]], NA_real_) # still relying on the payloads.
})

test_that("lists convert package versions to strings", {
    vals <- list(foo = as.package_version(c("1.0", "1.0.0", "1.2.1")), bar = package_version("1.2.3.4"))

    tmp <- tempfile()
    dir.create(tmp)

    saveObject(vals, file.path(tmp, "gunk"))
    reloaded <- readObject(file.path(tmp, "gunk"))
    expect_identical(reloaded, lapply(vals, as.character))

    saveObject(vals, file.path(tmp, "gunk.h5"), list.format="hdf5")
    reloaded <- readObject(file.path(tmp, "gunk.h5"))
    expect_identical(reloaded, lapply(vals, as.character))
})

test_that("lists work in VLS mode", {
    tmp <- tempfile()
    dir.create(tmp)

    x <- list(u=runif(5), w=c("A", "BC", "DEFG"), x=TRUE, y=list(a=c("HIJKL", "MNOP", "QRS", "TU", "V")), z=2:10)
    saveObject(x, file.path(tmp, "basic"), list.format="hdf5", list.character.vls=TRUE)
    reloaded <- readObject(file.path(tmp, "basic")) 
    expect_identical(x, reloaded)

    copy <- x
    copy$w[3] <- NA
    saveObject(copy, file.path(tmp, "with_missing"), list.format="hdf5", list.character.vls=TRUE)
    reloaded <- readObject(file.path(tmp, "with_missing")) 
    expect_identical(copy, reloaded)

    copy <- x
    names(copy$w) <- seq_along(copy$w)
    saveObject(copy, file.path(tmp, "named"), list.format="hdf5", list.character.vls=TRUE)
    reloaded <- readObject(file.path(tmp, "named")) 
    expect_identical(copy, reloaded)

    copy <- x
    copy$w <- c(copy$w, strrep("XXXXX", 100))
    saveObject(copy, file.path(tmp, "auto"), list.format="hdf5", list.character.vls=NULL)
    reloaded <- readObject(file.path(tmp, "auto")) 
    expect_identical(copy, reloaded)

    copy <- x
    copy$w <- "ABCDEFGHIJK"
    saveObject(copy, file.path(tmp, "scalar"), list.format="hdf5", list.character.vls=TRUE)
    reloaded <- readObject(file.path(tmp, "scalar")) 
    expect_identical(copy, reloaded)

    copy <- x
    copy$w <- I("ABCDEFGHIJK")
    saveObject(copy, file.path(tmp, "not_scalar"), list.format="hdf5", list.character.vls=TRUE)
    reloaded <- readObject(file.path(tmp, "not_scalar")) 
    expect_identical(copy, reloaded)
})
