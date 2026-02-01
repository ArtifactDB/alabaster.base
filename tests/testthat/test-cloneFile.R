# library(testthat); library(alabaster.base); source('test-cloneFile.R')

test_that("cloneFile works with copies", {
    src <- tempfile()
    dir.create(src)
    write(file=file.path(src, "foobar.txt"), LETTERS)

    dest <- tempfile()
    cloneFile(file.path(src, "foobar.txt"), dest, action="copy")
    expect_identical(readLines(dest), LETTERS) 

    # These are real copies; modifications do not affect the original.
    write(file=dest, letters, append=TRUE)
    expect_identical(readLines(dest), c(LETTERS, letters)) 
    expect_identical(readLines(file.path(src, "foobar.txt")), LETTERS)
})

test_that("cloneFile works with links", {
    src <- tempfile()
    dir.create(src)
    write(file=file.path(src, "foobar.txt"), LETTERS)

    dest <- tempfile()
    cloneFile(file.path(src, "foobar.txt"), dest, action="link")
    expect_identical(readLines(dest), LETTERS) 

    # These are now linked copies; modifications do affect the original!
    write(file=dest, letters, append=TRUE)
    expect_identical(readLines(dest), c(LETTERS, letters)) 
    expect_identical(readLines(file.path(src, "foobar.txt")), c(LETTERS, letters)) 
})

test_that("cloneFile works with symlinks", {
    skip_on_os("windows")

    src <- tempfile()
    dir.create(src)
    write(file=file.path(src, "foobar.txt"), LETTERS)

    dest <- tempfile()
    cloneFile(file.path(src, "foobar.txt"), dest, action="symlink")
    expect_identical(readLines(dest), LETTERS) 

    cleaned <- absolutizePath(src)
    expect_identical(Sys.readlink(dest), file.path(cleaned, "foobar.txt"))

    # Check that it still works if we provide a relative path to the target.
    pwd <- getwd()
    setwd(src)
    on.exit(setwd(pwd))

    dest <- tempfile()
    cloneFile("foobar.txt", dest, action="symlink")
    expect_identical(readLines(dest), LETTERS) 
    expect_identical(Sys.readlink(dest), file.path(cleaned, "foobar.txt"))
})

test_that("cloneFile symlinks and copies/hardlinks interact correctly", {
    skip_on_os("windows")

    src0 <- tempfile()
    dir.create(src0)
    write(file=file.path(src0, "foobar.txt"), LETTERS)

    src <- tempfile()
    dir.create(src)
    file.symlink(file.path(src0, "foobar.txt"), file.path(src, "foobar.txt"))

    dest1 <- tempfile()
    cloneFile(file.path(src, "foobar.txt"), dest1, action="link")
    expect_identical(readLines(dest1), LETTERS) 
    expect_identical(Sys.readlink(dest1), "")

    dest2 <- tempfile()
    cloneFile(file.path(src, "foobar.txt"), dest2, action="copy")
    expect_identical(readLines(dest2), LETTERS) 
    expect_identical(Sys.readlink(dest2), "")

    # Checking what happens if the test directory has relative symlinks.
    src2 <- tempfile()
    dir.create(file.path(src2, "stuff", "blah"), recursive=TRUE)
    write(file=file.path(src2, "foobar.txt"), LETTERS)
    file.symlink(file.path("..", "..", "foobar.txt"), file.path(src2, "stuff", "blah", "kanon.txt"))

    dest3 <- tempfile()
    cloneFile(file.path(src2, "stuff", "blah", "kanon.txt"), dest3, action="link")
    expect_identical(readLines(dest3), LETTERS) 
    expect_identical(Sys.readlink(dest3), "")
})

test_that("cloneFile works with relative symlinks", {
    skip_on_os("windows")

    workspace <- tempfile()
    dir.create(workspace)
    src <- file.path(workspace, "A")
    dir.create(src)
    write(file=file.path(src, "foobar.txt"), LETTERS)

    destdir <- file.path(workspace, "B")
    dest <- file.path(destdir, "foobar.txt")
    cloneFile(file.path(src, "foobar.txt"), dest, action="relsymlink")
    expect_identical(readLines(dest), LETTERS) 
    expect_identical(Sys.readlink(dest), file.path("..", "A", "foobar.txt"))
})
