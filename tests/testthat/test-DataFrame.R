# Test stageObject on DataFrames.
# library(testthat); library(alabaster.base); source("test-DataFrame.R")

library(S4Vectors)
old <- saveDataFrameFormat("csv.gz")

test_that("DFs handle their column types correctly", {
    tmp <- tempfile()
    dir.create(tmp, recursive=TRUE)

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

    for (version in 1:2) {
        info <- stageObject(df, tmp, paste0("rnaseq", version), .version.df=version)
        expect_match(info$path, ".csv.gz$")
        expect_identical(info$`$schema`, "csv_data_frame/v1.json")
        expect_identical(info$csv_data_frame$compression, "gzip")
        expect_false(is.null(info$csv_data_frame))

        # Should write without errors.
        resource <- writeMetadata(info, tmp)
        expect_true(file.exists(file.path(tmp, resource$path)))

        meta <- info$data_frame
        expect_identical(meta$columns[[1]]$type, "string")
        expect_identical(read.csv(file.path(tmp, meta$columns[[2]]$levels$resource$path))[,1], LETTERS[10:1])
        expect_null(meta$columns[[2]]$ordered)
        expect_identical(meta$columns[[3]]$type, "integer")
        expect_identical(meta$columns[[4]]$type, "number")

        if (version >= 2) {
            expect_identical(meta$columns[[5]]$type, "factor")
            expect_true(meta$columns[[5]]$ordered)
            expect_identical(read.csv(file.path(tmp, meta$columns[[5]]$levels$resource$path))[,1], LETTERS[1:3])
            expect_identical(meta$columns[[6]]$type, "string")
            expect_identical(meta$columns[[6]]$format, "date")
        } else {
            expect_identical(meta$columns[[5]]$type, "ordered")
            expect_identical(meta$columns[[6]]$type, "date")
        }

        # Round-tripping to make sure it's okay.
        out <- loadDataFrame(info, tmp)
        expect_identical(out$stuff, df$stuff)
        expect_identical(out$blah, df$blah)
        expect_identical(out$foo, df$foo)
        expect_identical(out$whee, df$whee)
        expect_identical(out$rabbit, df$rabbit)
        expect_identical(out$birthday, df$birthday)
    }

    # Works for HDF5.
    old <- saveDataFrameFormat("hdf5")
    on.exit(saveDataFrameFormat(old))

    for (version in 1:2) {
        meta2 <- stageObject(df, tmp, path=paste0("WHEE-", version), .version.df=version, .version.hdf5=version)
        expect_match(meta2$path, ".h5$")
        expect_identical(meta2$`$schema`, "hdf5_data_frame/v1.json")
        expect_false(is.null(meta2$hdf5_data_frame))

        # Should write without errors.
        resource <- writeMetadata(meta2, tmp)
        expect_true(file.exists(file.path(tmp, resource$path)))

        round2 <- loadDataFrame(meta2, project=tmp)
        expect_identical(round2, df)
    }

    # Works in the new world.
    tmp2 <- tempfile()
    saveObject(df, tmp2)
    expect_identical(readObjectFile(tmp2)$type, "data_frame")
    round2 <- readDataFrame(tmp2)
    expect_identical(round2, df)
})

test_that("staging of weird objects within DFs works correctly", {
    tmp <- tempfile()
    dir.create(tmp)

    df <- DataFrame(A=sample(3, 100, replace=TRUE), B=sample(letters[1:3], 100, replace=TRUE))
    Y <- DataFrameFactor(x=df)

    input <- DataFrame(A=1:3, X=0, B=letters[1:3], Y=0)
    input$X <- df[1:3,] # nested DF.
    input$Y <- Y[1:3] # nested DFFactor.
    input$Z <- DataFrame(whee=4:6) # another nested DF.
    input$AA <- list("AA", "BB", "CC") # nested list
    info <- stageObject(input, tmp, path="WHEE")

    # Should write without errors.
    resource <- writeMetadata(info, tmp)
    expect_true(file.exists(file.path(tmp, resource$path)))

    roundtrip <- loadDataFrame(info, project=tmp)
    expect_identical(roundtrip, input)

    roundtrip2 <- loadDataFrame(info, project=tmp, include.nested=FALSE)
    expect_identical(roundtrip2, input[,c("A", "B", "Y", "AA")])

    # Works for HDF5.
    old <- saveDataFrameFormat("hdf5")
    on.exit(saveDataFrameFormat(old))

    for (version in 1:2) {
        meta <- stageObject(input, tmp, path=paste0("WHEE-", version), .version.df=version, .version.hdf5=version)

        expect_match(meta$path, ".h5$")
        expect_identical(meta$`$schema`, "hdf5_data_frame/v1.json")
        expect_false(is.null(meta$hdf5_data_frame))

        contents <- rhdf5::h5read(file.path(tmp, meta$path), paste0(meta$hdf5_data_frame$group, "/data"))
        expect_identical(names(contents), c("0", "2")) # only simple columns are stored.

        resource <- writeMetadata(meta, tmp)
        round <- loadDataFrame(meta, project=tmp)
        expect_identical(round, input)
    }

    # Works in the new world.
    tmp2 <- tempfile()
    saveObject(input, tmp2)
    expect_identical(readObjectFile(tmp2)$type, "data_frame")
    round2 <- readDataFrame(tmp2)
    expect_identical(round2, input)
})

test_that("staging of uncompressed Gzip works correctly", {
    tmp <- tempfile()
    dir.create(tmp)

    df <- DataFrame(A=sample(3, 100, replace=TRUE), B=sample(letters[1:3], 100, replace=TRUE))

    old <- saveDataFrameFormat("csv")
    on.exit(saveDataFrameFormat(old))
    meta2 <- stageObject(df, tmp, path="WHEE")

    expect_identical(meta2$csv_data_frame$compression, "none")
    expect_match(meta2$path, ".csv$")
    expect_identical(meta2$`$schema`, "csv_data_frame/v1.json")

    # Should write without errors.
    resource <- writeMetadata(meta2, tmp)
    expect_true(file.exists(file.path(tmp, resource$path)))

    round2 <- loadDataFrame(meta2, project=tmp)
    expect_identical(round2, df)
})

test_that("staging with row names works correctly", {
    tmp <- tempfile()
    dir.create(tmp)
    df <- DataFrame(rbind(setNames(1:26, LETTERS)))

    meta <- stageObject(df, tmp, path="FOO")
    round1 <- loadDataFrame(meta, project=tmp)
    expect_identical(round1, df)

    # Works with HDF5.
    old <- saveDataFrameFormat("hdf5")
    on.exit(saveDataFrameFormat(old))

    for (version in 1:2) {
        meta2 <- stageObject(df, tmp, path=paste0("FOO-", version), .version.df=version, .version.hdf5=version)
        round2 <- loadDataFrame(meta2, project=tmp)
        expect_identical(round2, df)
    }

    # Works in the new world.
    tmp2 <- tempfile()
    saveObject(df, tmp2)
    expect_identical(readObjectFile(tmp2)$type, "data_frame")
    round2 <- readDataFrame(tmp2)
    expect_identical(df, round2)
})

test_that("staging of empty objects works correctly", {
    tmp <- tempfile()
    dir.create(tmp)

    # Truly empty.
    df <- DataFrame(matrix(100, 100, 0))
    meta <- stageObject(df, tmp, path="WHEE")
    round <- loadDataFrame(meta, project=tmp)
    expect_identical(round, df)

    # Empty, but has some row names.
    df2 <- df
    rownames(df2) <- sprintf("WHEE_%s", seq_len(nrow(df2)))
    meta <- stageObject(df2, tmp, path="WHEE_rn")
    round <- loadDataFrame(meta, project=tmp)
    expect_identical(round, df2)

    # Works for HDF5 as well.
    old <- saveDataFrameFormat("hdf5")
    on.exit(saveDataFrameFormat(old))

    for (version in 1:2) {
        meta <- stageObject(df, tmp, path=paste0("WHEE_h5", version), .version.df=version, .version.hdf5=version)
        round <- loadDataFrame(meta, project=tmp)
        expect_identical(round, df)

        meta <- stageObject(df2, tmp, path=paste0("FOO_h5", version), .version.df=version, .version.hdf5=version)
        round <- loadDataFrame(meta, project=tmp)
        expect_identical(round, df2)
    }

    # Works in the new world.
    tmp2a <- tempfile()
    saveObject(df, tmp2a)
    round2 <- readDataFrame(tmp2a)
    expect_identical(df, round2)

    tmp2b <- tempfile()
    saveObject(df2, tmp2b)
    round2 <- readDataFrame(tmp2b)
    expect_identical(df2, round2)
})

test_that("handling of NAs works correctly", {
    tmp <- tempfile()
    dir.create(tmp)

    df <- DataFrame(
        a=c("A", "B", NA), 
        b=c("A", "NA", NA), 
        c=factor(c("A", "B", NA)), 
        d=factor(c("A", "B", "NA")),
        e=c(1L,2L,NA),
        f=c(1.5,2.5,NA),
        g=c(TRUE,FALSE,NA),
        h=1:3,
        i=c(TRUE,FALSE,TRUE),
        j=letters[1:3]
    )

    # Staged and validated correctly.
    meta <- stageObject(df, tmp, path="WHEE")
    expect_error(writeMetadata(meta, tmp), NA) 

    # Round trip works.
    round1 <- loadDataFrame(meta, project=tmp)
    expect_identical(df, round1)

    # Works for HDF5-based DFs.
    old <- saveDataFrameFormat("hdf5")
    on.exit(saveDataFrameFormat(old))

    meta2 <- stageObject(df, tmp, path="WHEE2", .version.df=2, .version.hdf5=2)
    expect_match(meta2[["$schema"]], "hdf5_data_frame")

    fpath <- file.path(tmp, meta2$path)
    attrs <- rhdf5::h5readAttributes(fpath, "contents/data/0")
    expect_identical(attrs[["missing-value-placeholder"]], "NA")
    attrs <- rhdf5::h5readAttributes(fpath, "contents/data/1")
    expect_identical(attrs[["missing-value-placeholder"]], "_NA")

    attrs <- rhdf5::h5readAttributes(fpath, "contents/data/2")
    expect_identical(attrs[["missing-value-placeholder"]], NA_integer_)
    attrs <- rhdf5::h5readAttributes(fpath, "contents/data/3")
    expect_null(attrs[["missing-value-placeholder"]])

    attrs <- rhdf5::h5readAttributes(fpath, "contents/data/4")
    expect_true(is.na(attrs[["missing-value-placeholder"]]))
    attrs <- rhdf5::h5readAttributes(fpath, "contents/data/5")
    place <- attrs[["missing-value-placeholder"]]
    expect_true(is.na(place) && !is.nan(place))
    attrs <- rhdf5::h5readAttributes(fpath, "contents/data/6")
    expect_equal(attrs[["missing-value-placeholder"]], -1L)

    attrs <- rhdf5::h5readAttributes(fpath, "contents/data/7")
    expect_null(attrs[["missing-value-placeholder"]])
    attrs <- rhdf5::h5readAttributes(fpath, "contents/data/8")
    expect_null(attrs[["missing-value-placeholder"]])
    attrs <- rhdf5::h5readAttributes(fpath, "contents/data/9")
    expect_null(attrs[["missing-value-placeholder"]])

    round2 <- loadDataFrame(meta2, project=tmp)
    expect_identical(df, round2)

    # Works in the new world.
    tmp2 <- tempfile()
    saveObject(df, tmp2)

    fpath <- file.path(tmp2, "basic_columns.h5")
    attrs <- rhdf5::h5readAttributes(fpath, "data_frame/data/2/codes")
    expect_identical(attrs[["missing-value-placeholder"]], 2L)
    attrs <- rhdf5::h5readAttributes(fpath, "data_frame/data/3/codes")
    expect_null(attrs[["missing-value-placeholder"]])

    round2 <- readDataFrame(tmp2)
    expect_identical(df, round2)
})

test_that("handling of the integer minimum limit works correctly", {
    tmp <- tempfile()
    dir.create(tmp)

    df <- DataFrame(foobar=c(1L,2L,NA))
    actual <- c(1, 2, -2^31)

    # For CSV:
    meta <- stageObject(df, tmp, path="exceptions")
    expect_identical(meta[["$schema"]], "csv_data_frame/v1.json")
    fpath <- file.path(tmp, meta$path)
    write(paste0(c("\"foobar\"", actual), collapse="\n"), file=gzfile(fpath))

    round <- loadDataFrame(meta, project=tmp)
    expect_identical(round$foobar, actual)

    # For HDF5: 
    old <- saveDataFrameFormat("hdf5")
    on.exit(saveDataFrameFormat(old))

    for (version in 1:2) { 
        meta <- stageObject(df, tmp, path=paste0("hdf5-", version), .version.df=version, .version.hdf5=version)
        fpath <- file.path(tmp, meta$path)
        rhdf5::h5deleteAttribute(fpath, "contents/data/0", "missing-value-placeholder")

        round <- loadDataFrame(meta, project=tmp)
        expect_identical(round$foobar, actual)

        # Trying again.
        fhandle <- rhdf5::H5Fopen(fpath)
        dhandle <- rhdf5::H5Dopen(fhandle, "contents/data/0")
        rhdf5::h5writeAttribute(1, dhandle, "missing-value-placeholder", asScalar=TRUE)
        rhdf5::H5Dclose(dhandle)
        rhdf5::H5Fclose(fhandle)

        round <- loadDataFrame(meta, project=tmp)
        expect_identical(round$foobar, c(NA, 2, -2^31))
    }

    # Works in the new world.
    tmp2 <- tempfile()
    saveObject(df, tmp2)

    fpath <- file.path(tmp2, "basic_columns.h5")
    rhdf5::h5deleteAttribute(fpath, "data_frame/data/0", "missing-value-placeholder")
    round2 <- readDataFrame(tmp2)
    expect_identical(round2$foobar, actual)

    fhandle <- rhdf5::H5Fopen(fpath)
    dhandle <- rhdf5::H5Dopen(fhandle, "data_frame/data/0")
    rhdf5::h5writeAttribute(1, dhandle, "missing-value-placeholder", asScalar=TRUE)
    rhdf5::H5Dclose(dhandle)
    rhdf5::H5Fclose(fhandle)

    round <- readDataFrame(tmp2)
    expect_identical(round$foobar, c(NA, 2, -2^31))
})

test_that("handling of IEEE special values work correctly", {
    tmp <- tempfile()
    dir.create(tmp)

    df <- DataFrame(specials=c(NaN, 1, 2.12345678, Inf, NA, -Inf))
    meta <- stageObject(df, tmp, path="FOO1")
    round <- loadDataFrame(meta, project=tmp)
    expect_identical(round, df)

    df2 <- cbind(df, normals=1:6) # check that quoting works
    meta <- stageObject(df2, tmp, path="FOO2")
    round <- loadDataFrame(meta, project=tmp)
    expect_identical(round, df2)

    df3 <- cbind(df2, more_normals=LETTERS[1:6])
    meta <- stageObject(df3, tmp, path="FOO3")
    round <- loadDataFrame(meta, project=tmp)
    expect_identical(round, df3)

    # Works for HDF5.
    old <- saveDataFrameFormat("hdf5")
    on.exit(saveDataFrameFormat(old))

    for (version in 1:2) { 
        meta <- stageObject(df, tmp, path=paste0("hFOO1-", version), .version.df=version, .version.hdf5=version)
        round <- loadDataFrame(meta, project=tmp)
        expect_identical(round, df)

        meta <- stageObject(df2, tmp, path=paste0("hFOO2-", version), .version.df=version, .version.hdf5=version)
        round <- loadDataFrame(meta, project=tmp)
        expect_identical(round, df2)
    }

    # Works in the new world.
    tmp2a <- tempfile()
    saveObject(df, tmp2a)
    expect_identical(readObjectFile(tmp2a)$type, "data_frame")
    round2 <- readDataFrame(tmp2a)
    expect_identical(round2, df)

    tmp2b <- tempfile()
    saveObject(df2, tmp2b)
    expect_identical(readObjectFile(tmp2b)$type, "data_frame")
    round2 <- readDataFrame(tmp2b)
    expect_identical(round2, df2)
})

test_that("loaders work correctly from HDF5 with non-default placeholders", {
    tmp <- tempfile()
    dir.create(tmp)

    df <- DataFrame(
        a=c(1L,2L,3L),
        b=c(1.5,2.5,3.5),
        c=c(1.5,2.5,NaN)
    )

    old <- saveDataFrameFormat("hdf5")
    on.exit(saveDataFrameFormat(old))

    # In the old world.
    meta2 <- stageObject(df, tmp, path="WHEE2", .version.df=2, .version.hdf5=2)

    fpath <- file.path(tmp, meta2$path)
    addMissingPlaceholderAttributeForHdf5(fpath, "contents/data/0", 1L)
    addMissingPlaceholderAttributeForHdf5(fpath, "contents/data/1", 2.5)
    addMissingPlaceholderAttributeForHdf5(fpath, "contents/data/2", NaN)

    round <- loadDataFrame(meta2, project=tmp)
    expect_identical(round$a, c(NA, 2L, 3L))
    expect_identical(round$b, c(1.5, NA, 3.5))
    expect_identical(round$c, c(1.5, 2.5, NA))

    # Works in the new world.
    tmp2 <- tempfile()
    saveObject(df, tmp2)

    fpath <- file.path(tmp2, "basic_columns.h5")
    addMissingPlaceholderAttributeForHdf5(fpath, "data_frame/data/0", 1L)
    addMissingPlaceholderAttributeForHdf5(fpath, "data_frame/data/1", 2.5)
    addMissingPlaceholderAttributeForHdf5(fpath, "data_frame/data/2", NaN)

    round2 <- readDataFrame(tmp2)
    expect_identical(round2$a, c(NA, 2L, 3L))
    expect_identical(round2$b, c(1.5, NA, 3.5))
    expect_identical(round2$c, c(1.5, 2.5, NA))
})

test_that("stageObject works with extra mcols", {
    tmp <- tempfile()
    dir.create(tmp)

    df <- DataFrame(A=sample(3, 100, replace=TRUE), B=sample(letters[1:3], 100, replace=TRUE))
    out <- stageObject(df, tmp, "raw_thing")
    expect_null(out$data_frame$column_data)
    expect_null(out$data_frame$other_data)

    # Ignores it when the mcols have no columns.
    mcols(df) <- make_zero_col_DFrame(2)
    out <- stageObject(df, tmp, "raw_thing2")
    expect_null(out$data_frame$column_data)
    expect_null(out$data_frame$other_data)

    tmp2 <- tempfile()
    saveObject(df, tmp2)
    round2 <- readDataFrame(tmp2)
    expect_null(mcols(round2))
    mcols(round2) <- mcols(df)
    expect_identical(df, round2)

    # Alright, adding some mcols.
    mcols(df)$stuff <- runif(ncol(df))
    mcols(df)$foo <- sample(LETTERS, ncol(df), replace=TRUE)
    metadata(df) <- list(WHEE="foo")

    out <- stageObject(df, tmp, "thing")
    expect_false(is.null(out$data_frame$column_data))
    expect_false(is.null(out$data_frame$other_data))

    resource <- writeMetadata(out, tmp)
    expect_true(file.exists(file.path(tmp, resource$path)))
    df2 <- loadDataFrame(out, tmp)
    expect_equal(df, df2)

    tmp2 <- tempfile()
    saveObject(df, tmp2)
    round2 <- readDataFrame(tmp2)
    expect_identical(df, round2)

    # Eliminates redundant row names.
    mc <- mcols(df)
    rownames(mc) <- c("C", "D")
    mcols(df, use.names=FALSE) <- mc
    out <- stageObject(df, tmp, "thing2")
    resource <- writeMetadata(out, tmp)

    df2 <- loadDataFrame(out, tmp)
    expect_equal(df, df2)
    mc <- mcols(df2, use.names=FALSE)
    expect_null(rownames(mc))

    tmp2 <- tempfile()
    saveObject(df, tmp2)
    round2 <- readDataFrame(tmp2)
    expect_identical(df, round2)
})

test_that("DF staging preserves odd colnames", {
    tmp <- tempfile()
    dir.create(tmp)

    ncols <- 123
    df <- DataFrame(
        `foo bar` = seq_len(ncols),
        `rabbit+2+3/5` = as.numeric(10 + seq_len(ncols)),
        check.names=FALSE
    )

    # Correct in the CSV file:
    meta <- stageObject(df, tmp, path="WHEE")
    resource <- writeMetadata(meta, tmp)
    expect_equal(loadDataFrame(meta, tmp), df)

    fpath <- file.path(tmp, meta$path)
    first <- readLines(fpath, n=1)
    expect_identical(first, "\"foo bar\",\"rabbit+2+3/5\"")

    # Correct in the HDF5 file:
    old <- saveDataFrameFormat("hdf5")
    on.exit(saveDataFrameFormat(old))

    meta <- stageObject(df, tmp, path="WHEE2", .version.df=2, .version.hdf5=2)
    resource <- writeMetadata(meta, tmp)
    expect_identical(loadDataFrame(meta, tmp), df)

    fpath <- file.path(tmp, meta$path)
    expect_identical(as.vector(rhdf5::h5read(fpath, "contents/column_names")), colnames(df))

    # Works in the new world.
    tmp2 <- tempfile()
    saveObject(df, tmp2)
    round2 <- readDataFrame(tmp2)
    expect_identical(df, round2)
})

test_that("DFs fails with duplicate or empty colnames", {
    tmp <- tempfile()
    dir.create(tmp)

    ncols <- 123
    df <- DataFrame(
        foo = seq_len(ncols),
        rabbit = as.numeric(10 + seq_len(ncols)),
        rabbit = factor(sample(LETTERS, ncols, replace=TRUE)),
        check.names=FALSE
    )

    expect_error(info <- stageObject(df, tmp, "rnaseq"), "duplicate")

    unlink(file.path(tmp, "rnaseq"), recursive=TRUE)
    df2 <- df
    colnames(df2)[2] <- ""
    expect_error(info <- stageObject(df2, tmp, "rnaseq"), "empty")

    # Fails in the new world.
#    tmp2 <- tempfile()
#    expect_error(saveObject(df, tmp2), "duplicate")
#    tmp2 <- tempfile()
#    expect_error(saveObject(df2, tmp2), "empty")
})

test_that("DFs handle POSIX times correctly", {
    tmp <- tempfile()
    dir.create(tmp, recursive=TRUE)

    df <- DataFrame(
        foo = as.POSIXct(c(123123, 124235235, 96546546)),
        bar = as.POSIXct(c(123123, 124235235, 96546546)) # TODO: should be POSIXlt, but see Bioconductor/S4Vectors#113
    )

    info <- stageObject(df, tmp, "rnaseq")
    meta <- info$data_frame
    expect_identical(meta$columns[[1]]$type, "string")
    expect_identical(meta$columns[[1]]$format, "date-time")
    expect_identical(meta$columns[[2]]$type, "string")
    expect_identical(meta$columns[[2]]$format, "date-time")

    # Round-tripping to make sure it's okay. Note that this requires some care
    # because on some machines, R doesn't set the timezone properly if you call
    # as.POSIXct with format=.  Dunno why, but it goes away if we just call
    # as.POSIXct again, so we'll just stop fighting and do that.
    out <- loadDataFrame(info, tmp)
    expect_s3_class(out$foo, "POSIXct")
    expect_s3_class(out$bar, "POSIXct")
    expect_identical(df$foo, as.POSIXct(out$foo))
    expect_identical(df$bar, as.POSIXct(out$bar))

    # Same for HDF5.
    old <- saveDataFrameFormat("hdf5")
    on.exit(saveDataFrameFormat(old))

    for (version in 1:2) {
        info <- stageObject(df, tmp, paste0("bar-", version), .version.df=version, .version.hdf5=version)
        out <- loadDataFrame(info, tmp)
        expect_s3_class(out$foo, "POSIXct")
        expect_s3_class(out$bar, "POSIXct")
        expect_identical(df$foo, as.POSIXct(out$foo))
        expect_identical(df$bar, as.POSIXct(out$bar))
    }

    # Works in the new world.
    tmp2 <- tempfile()
    saveObject(df, tmp2)
    round2 <- readDataFrame(tmp2)
    expect_s3_class(round2$foo, "Rfc3339")
    expect_s3_class(round2$bar, "Rfc3339")
    expect_identical(df$foo, as.POSIXct(round2$foo))
    expect_identical(df$bar, as.POSIXct(round2$bar))

    # Rfc3339 objects are also correctly saved.
    tmp3 <- tempfile()
    saveObject(round2, tmp3)
    expect_identical(readDataFrame(tmp3), round2)
})

test_that("staging of arrays within DFs works correctly", {
    skip_if_not_installed("alabaster.matrix")
    tmp <- tempfile()
    dir.create(tmp)

    input <- DataFrame(A=1:3, X=0, Y=0)
    input$X <- array(runif(3)) # 1D array
    input$Y <- cbind(runif(3)) # matrix with 1 column 
    input$Z <- cbind(V=runif(3), Z=rnorm(3)) # matrix with 2 columns.
    info <- stageObject(input, tmp, path="WHEE")

    suppressWarnings(roundtrip <- loadDataFrame(info, project=tmp))
    input$X <- as.numeric(input$X) # strip 1D arrays as these are rarely desirable.
    roundtrip$Y <- as.matrix(roundtrip$Y)
    roundtrip$Z <- as.matrix(roundtrip$Z)
    expect_equal(roundtrip, input)

    # Works in the new world.
    tmp <- tempfile()
    saveObject(input, tmp)
    roundtrip <- readObject(tmp)
    roundtrip$Y <- as.matrix(roundtrip$Y)
    roundtrip$Z <- as.matrix(roundtrip$Z)
    expect_identical(roundtrip, input)
})

test_that("staging of arrays continues to work with character matrices", {
    skip_if_not_installed("alabaster.matrix")
    tmp <- tempfile()
    dir.create(tmp)

    input <- DataFrame(A=1:3, X=0, Y=0)
    input$X <- array(letters[1:3]) # 1D array
    input$Y <- cbind(LETTERS[1:3]) # matrix with 1 column 
    input$Z <- cbind(V=letters[4:6], Z=LETTERS[4:6]) # matrix with 2 columns.
    info <- stageObject(input, tmp, path="WHEE")

    suppressWarnings(roundtrip <- loadDataFrame(info, project=tmp))
    input$X <- as.character(input$X) # strip 1D arrays as these are rarely desirable.
    roundtrip$Y <- as.matrix(roundtrip$Y)
    roundtrip$Z <- as.matrix(roundtrip$Z)
    expect_equal(roundtrip, input)

    # Works in the new world.
    tmp <- tempfile()
    saveObject(input, tmp)
    roundtrip <- readObject(tmp)
    roundtrip$Y <- as.matrix(roundtrip$Y)
    roundtrip$Z <- as.matrix(roundtrip$Z)
    expect_identical(roundtrip, input)
})

test_that("saving works for base data.frames", {
    nrows <- 123
    df <- data.frame(
        stuff = rep(LETTERS[1:3], length.out=nrows),
        foo = seq_len(nrows),
        whee = as.numeric(10 + seq_len(nrows))
    )
    df$blah <- factor(df$stuff, LETTERS[10:1])
    df$rabbit <- factor(df$stuff, LETTERS[1:3], ordered=TRUE)

    tmp <- tempfile()
    saveObject(df, tmp)
    roundtrip <- readObject(tmp)
    expect_null(rownames(roundtrip))
    expect_identical(as.data.frame(roundtrip), df)

    # Respects row names.
    rownames(df) <- sprintf("GENE_%i", seq_len(nrows))
    tmp <- tempfile()
    saveObject(df, tmp)
    roundtrip <- readObject(tmp)
    expect_identical(as.data.frame(roundtrip), df)
})

test_that("saving works for data.frames containing package_version columns", {
    df <- data.frame(
        stuff = as.package_version(c("1.2", "2.3")),
        whee = as.package_version(c("2.3.4", "4.5.6"))
    )

    tmp <- tempfile()
    saveObject(df, tmp)
    roundtrip <- readObject(tmp)

    expect_identical(roundtrip$stuff, as.character(df$stuff))
    expect_identical(roundtrip$whee, as.character(df$whee))
})
