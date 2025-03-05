# library(testthat); library(alabaster.base); source("test-vls.R")

test_that("h5_use_vls behaves as expected", {
    expect_false(h5_use_vls(LETTERS))
    expect_true(h5_use_vls(c(LETTERS, strrep("Z", 100))))
})

library(rhdf5)
test_that("VLS writing works for arrays", {
    N <- as.integer(runif(1200, 1, 100))
    collection <- strrep(sample(LETTERS, length(N), replace=TRUE), N)

    for (buffer.size in c(200, 500, 1000, 2000)) {
        temp <- tempfile(fileext=".h5")
        h5createFile(temp)
        h5createGroup(temp, "FOO")
        h5_write_vls_array(temp, "FOO", "pointers", "heap", collection, chunk=31, buffer.size=buffer.size)

        roundtrip <- h5_read_vls_array(temp, "FOO/pointers", "FOO/heap", buffer.size=buffer.size, native=FALSE)
        expect_identical(as.vector(roundtrip), collection)
        nroundtrip <- h5_read_vls_array(temp, "FOO/pointers", "FOO/heap", buffer.size=buffer.size, native=TRUE)
        expect_identical(as.vector(nroundtrip), collection)
    }
})

test_that("VLS writing works for matrices", {
    N <- as.integer(runif(1200, 1, 100))
    collection <- strrep(sample(LETTERS, length(N), replace=TRUE), N)
    mat <- matrix(collection, 40, 30)

    for (buffer.size in c(200, 500, 1000, 2000)) {
        temp <- tempfile(fileext=".h5")
        h5createFile(temp)
        h5createGroup(temp, "FOO")
        h5_write_vls_array(temp, "FOO", "pointers", "heap", mat, chunk=c(9, 11), buffer.size=buffer.size)

        roundtrip <- h5_read_vls_array(temp, "FOO/pointers", "FOO/heap", buffer.size=buffer.size, native=FALSE)
        expect_identical(roundtrip, mat)
        nroundtrip <- h5_read_vls_array(temp, "FOO/pointers", "FOO/heap", buffer.size=buffer.size, native=TRUE)
        expect_identical(t(nroundtrip), mat)
    }
})

test_that("VLS writing works for arrays", {
    N <- as.integer(runif(30000, 1, 100))
    collection <- strrep(sample(LETTERS, length(N), replace=TRUE), N)
    arr <- array(collection, c(20, 30, 50))

    for (buffer.size in c(200, 500, 1000, 2000)) {
        temp <- tempfile(fileext=".h5")
        h5createFile(temp)
        h5createGroup(temp, "FOO")
        h5_write_vls_array(temp, "FOO", "pointers", "heap", arr, chunk=c(8, 6, 11), buffer.size=buffer.size)

        roundtrip <- h5_read_vls_array(temp, "FOO/pointers", "FOO/heap", buffer.size=buffer.size, native=FALSE)
        expect_identical(roundtrip, arr)
        nroundtrip <- h5_read_vls_array(temp, "FOO/pointers", "FOO/heap", buffer.size=buffer.size, native=TRUE)
        expect_identical(aperm(nroundtrip, 3:1), arr)
    }
})
