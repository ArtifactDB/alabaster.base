# library(testthat); library(alabaster.base); source("test-getSaveEnvironment.R")

test_that("formatSaveEnvironment works correctly", {
    env <- formatSaveEnvironment()
    expect_identical(env$type, "R")
    expect_type(env$platform, "character")
    expect_type(env$version, "character")
    expect_identical(env$packages$alabaster.base, as.character(packageVersion("alabaster.base")))

    expect_identical(env$packages$jsonlite, as.character(packageVersion("jsonlite")))
})

test_that("useSaveEnvironment works correctly", {
    prev <- useSaveEnvironment(TRUE)
    on.exit(useSaveEnvironment(prev), add=TRUE, after=FALSE)
    expect_true(useSaveEnvironment())
    useSaveEnvironment(FALSE)
    expect_false(useSaveEnvironment())
})

test_that("registerSaveEnvironment works correctly", {
    prev <- useSaveEnvironment(TRUE)
    on.exit(useSaveEnvironment(prev), add=TRUE, after=FALSE)
    expect_null(getSaveEnvironment())

    wfuns <- registerSaveEnvironment(info=list(type="TEST"))
    on.exit(wfuns$restore(), add=TRUE, after=FALSE)
    expect_identical(getSaveEnvironment()$type, "TEST")

    tmp <- tempfile()
    dir.create(tmp)
    wfuns$write(tmp)
    expect_true(file.exists(alabaster.base:::.get_environment_path(tmp)))
    expect_identical(jsonlite::fromJSON(alabaster.base:::.get_environment_path(tmp), simplifyVector=FALSE)$type, "TEST")

    # Further calls are no-ops.
    wfuns2 <- registerSaveEnvironment(info=list(type="TEST2"))
    on.exit(wfuns2$restore(), add=TRUE, after=FALSE)
    expect_identical(getSaveEnvironment()$type, "TEST")
})

test_that("registerSaveEnvironment uses the default info", {
    prev <- useSaveEnvironment(TRUE)
    on.exit(useSaveEnvironment(prev), add=TRUE, after=FALSE)
    expect_null(getSaveEnvironment())

    wfuns <- registerSaveEnvironment()
    on.exit(wfuns$restore(), add=TRUE, after=FALSE)
    expect_identical(getSaveEnvironment()$type, "R")
})

test_that("registerSaveEnvironment can be disabled", {
    prev <- useSaveEnvironment(FALSE)
    on.exit(useSaveEnvironment(prev), add=TRUE, after=FALSE)
    expect_null(getSaveEnvironment())

    wfuns <- registerSaveEnvironment()
    on.exit(wfuns$restore(), add=TRUE, after=FALSE)
    expect_null(getSaveEnvironment())
})

test_that("loadSaveEnvironment works correctly", {
    prev <- useSaveEnvironment(TRUE)
    on.exit(useSaveEnvironment(prev), add=TRUE, after=FALSE)
    expect_null(getSaveEnvironment())

    tmp <- tempfile()
    dir.create(tmp)
    write(file=alabaster.base:::.get_environment_path(tmp), '{ "type": "TEST" }')

    lfun <- loadSaveEnvironment(tmp)
    on.exit(lfun(), add=TRUE, after=FALSE)
    expect_identical(getSaveEnvironment()$type, "TEST")

    # Further calls are NOT no-ops.
    write(file=alabaster.base:::.get_environment_path(tmp), '{ "type": "TEST2" }')
    lfun2 <- loadSaveEnvironment(tmp)
    on.exit(lfun2(), add=TRUE, after=FALSE)
    expect_identical(getSaveEnvironment()$type, "TEST2")
})

test_that("loadSaveEnvironment warns correctly for invalid files", {
    prev <- useSaveEnvironment(TRUE)
    on.exit(useSaveEnvironment(prev), add=TRUE, after=FALSE)
    expect_null(getSaveEnvironment())

    tmp <- tempfile()
    dir.create(tmp)
    write(file=alabaster.base:::.get_environment_path(tmp), 'adsds')

    expect_warning(lfun2 <- loadSaveEnvironment(tmp), "failed to read")
    on.exit(lfun2(), add=TRUE, after=FALSE)
    expect_null(getSaveEnvironment())
})

test_that("loadSaveEnvironment can be disabled", {
    prev <- useSaveEnvironment(FALSE)
    on.exit(useSaveEnvironment(prev), add=TRUE, after=FALSE)
    expect_null(getSaveEnvironment())

    tmp <- tempfile()
    dir.create(tmp)
    write(file=alabaster.base:::.get_environment_path(tmp), '{ "type": "TEST" }')

    lfun <- loadSaveEnvironment(tmp)
    on.exit(lfun(), add=TRUE, after=FALSE)
    expect_null(getSaveEnvironment())
})

test_that("saving the object tracks its environment", {
    prev <- useSaveEnvironment(TRUE)
    on.exit(useSaveEnvironment(prev), add=TRUE, after=FALSE)
    expect_null(getSaveEnvironment())

    library(S4Vectors)
    df <- DataFrame(A=LETTERS)
    tmp <- tempfile()
    saveObject(df, tmp)
    expect_null(getSaveEnvironment()) # correctly unset at the end of all calls.

    envpath <- alabaster.base:::.get_environment_path(tmp)
    expect_true(file.exists(envpath))
    out <- jsonlite::fromJSON(envpath, simplifyVector=FALSE)
    expect_identical(out$type, "R")

    # Works when we use altSaveObject overrides.
    prevalt <- altSaveObjectFunction(saveObject)
    on.exit(altSaveObjectFunction(prevalt), add=TRUE, after=FALSE)
    tmp <- tempfile()
    altSaveObject(df, tmp)
    expect_null(getSaveEnvironment()) # correctly unset at the end of all calls.
    expect_true(file.exists(alabaster.base:::.get_environment_path(tmp)))

    # Child objects don't get an _environment.json as they inherit it from their parents.
    df$B <- DataFrame(B=runif(nrow(df)))
    tmp <- tempfile()
    saveObject(df, tmp)
    expect_null(getSaveEnvironment()) # correctly unset at the end of all calls.
    expect_true(file.exists(alabaster.base:::.get_environment_path(tmp)))
    expect_true(file.exists(file.path(tmp, "other_columns", "1")))
    expect_false(file.exists(alabaster.base:::.get_environment_path(file.path(tmp, "other_columns", "1"))))
})

test_that("reading the object respects its environment", {
    prev <- useSaveEnvironment(TRUE)
    on.exit(useSaveEnvironment(prev), add=TRUE, after=FALSE)
    expect_null(getSaveEnvironment())

    library(S4Vectors)
    df <- DataFrame(A=LETTERS)
    tmp <- tempfile()
    saveObject(df, tmp)

    # Checking that we correctly read the environment file.
    readObject(tmp) # priming the registry so that the loading function is realized from a string.
    oldfun <- readObjectFunctionRegistry()$data_frame
    registerReadObjectFunction("data_frame", function(...) {
        expect_identical(getSaveEnvironment()$type, "R")
        oldfun(...)
    }, existing="new")
    on.exit(registerReadObjectFunction("data_frame", oldfun, existing="new"), add=TRUE, after=FALSE)

    expect_error(roundtrip <- readObject(tmp), NA)
    expect_null(getSaveEnvironment()) # correctly unset at the end of all calls.
    expect_identical(df, roundtrip)

    # Works when we use altSaveObject overrides.
    prevalt <- altReadObjectFunction(readObject)
    on.exit(altReadObjectFunction(prevalt), add=TRUE, after=FALSE)
    expect_error(df <- altReadObject(tmp), NA)
    expect_null(getSaveEnvironment()) # correctly unset at the end of all calls.
    expect_identical(df, roundtrip)

    # Child objects don't get an _environment.json as they inherit it from their parents.
    df$B <- DataFrame(B=runif(nrow(df)))
    tmp <- tempfile()
    saveObject(df, tmp)
    expect_error(roundtrip <- readObject(tmp), NA)
    expect_null(getSaveEnvironment()) # correctly unset at the end of all calls.
    expect_identical(df, roundtrip)

    # Works when no environment is available.
    registerReadObjectFunction("data_frame", function(...) {
        expect_null(getSaveEnvironment())
        oldfun(...)
    }, existing="new")
    unlink(file.path(alabaster.base:::.get_environment_path(tmp)))
    expect_error(roundtrip <- readObject(tmp), NA)
    expect_null(getSaveEnvironment()) # correctly unset at the end of all calls.
})

