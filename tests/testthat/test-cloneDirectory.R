# library(testthat); library(alabaster.base); source('test-cloneDirectory.R')

create_test_directory <- function(src) {
    dir.create(src)
    write(file=file.path(src, "foobar.txt"), LETTERS)
    dir.create(file.path(src, "stuff"))
    write(file=file.path(src, "stuff", "whee.txt"), as.character(1:10))
    dir.create(file.path(src, "stuff", "blah"))
    write(file=file.path(src, "stuff", "blah", "kanon.txt"), "air")
}

test_that("cloneDirectory works with copies", {
    src <- tempfile()
    create_test_directory(src)

    dest <- tempfile()
    cloneDirectory(src, dest, action="copy")
    expect_identical(readLines(file.path(dest, "foobar.txt")), LETTERS) 
    expect_identical(readLines(file.path(dest, "stuff", "whee.txt")), as.character(1:10))
    expect_identical(readLines(file.path(dest, "stuff", "blah", "kanon.txt")), "air")

    # These are real copies; modifications do not affect the original.
    write(file=file.path(dest, "foobar.txt"), letters, append=TRUE)
    expect_identical(readLines(file.path(dest, "foobar.txt")), c(LETTERS, letters)) 
    expect_identical(readLines(file.path(src, "foobar.txt")), LETTERS)
})

test_that("cloneDirectory works with links", {
    src <- tempfile()
    create_test_directory(src)

    dest <- tempfile()
    cloneDirectory(src, dest, action="link")
    expect_identical(readLines(file.path(dest, "foobar.txt")), LETTERS) 
    expect_identical(readLines(file.path(dest, "stuff", "whee.txt")), as.character(1:10))
    expect_identical(readLines(file.path(dest, "stuff", "blah", "kanon.txt")), "air")

    # These are now linked copies; modifications do affect the original!
    write(file=file.path(dest, "foobar.txt"), letters, append=TRUE)
    expect_identical(readLines(file.path(dest, "foobar.txt")), c(LETTERS, letters)) 
    expect_identical(readLines(file.path(src, "foobar.txt")), c(LETTERS, letters)) 
})

test_that("cloneDirectory works with symlinks", {
    skip_on_os("windows")

    src <- tempfile()
    create_test_directory(src)

    dest <- tempfile()
    cloneDirectory(src, dest, action="symlink")
    expect_identical(readLines(file.path(dest, "foobar.txt")), LETTERS) 
    expect_identical(readLines(file.path(dest, "stuff", "whee.txt")), as.character(1:10))
    expect_identical(readLines(file.path(dest, "stuff", "blah", "kanon.txt")), "air")

    cleaned <- absolutizePath(src)
    expect_identical(Sys.readlink(file.path(dest, "foobar.txt")), file.path(cleaned, "foobar.txt"))
    expect_identical(Sys.readlink(file.path(dest, "stuff", "whee.txt")), file.path(cleaned, "stuff", "whee.txt"))
    expect_identical(Sys.readlink(file.path(dest, "stuff", "blah", "kanon.txt")), file.path(cleaned, "stuff", "blah", "kanon.txt"))

    # Checking that we save absolute paths, even if the input 'src' is relative.
    pwd <- getwd()
    setwd(dirname(src))
    on.exit(setwd(pwd))

    dest <- tempfile()
    cloneDirectory(basename(src), dest, action="symlink")
    expect_identical(readLines(file.path(dest, "foobar.txt")), LETTERS) 
    expect_identical(readLines(file.path(dest, "stuff", "whee.txt")), as.character(1:10))
    expect_identical(readLines(file.path(dest, "stuff", "blah", "kanon.txt")), "air")

    expect_identical(Sys.readlink(file.path(dest, "foobar.txt")), file.path(cleaned, "foobar.txt"))
    expect_identical(Sys.readlink(file.path(dest, "stuff", "whee.txt")), file.path(cleaned, "stuff", "whee.txt"))
    expect_identical(Sys.readlink(file.path(dest, "stuff", "blah", "kanon.txt")), file.path(cleaned, "stuff", "blah", "kanon.txt"))
})

test_that("cloneDirectory symlinks and copies/hardlinks interact correctly", {
    skip_on_os("windows")

    src0 <- tempfile()
    create_test_directory(src0)
    src <- tempfile()
    cloneDirectory(src0, src, action="symlink")

    expect_identical(alabaster.base:::resolve_symlinks(file.path(src, "foobar.txt")), file.path(src0, "foobar.txt"))
    expect_identical(alabaster.base:::resolve_symlinks(file.path(src, "stuff", "whee.txt")), file.path(src0, "stuff", "whee.txt"))
    expect_identical(alabaster.base:::resolve_symlinks(file.path(src, "stuff", "blah", "kanon.txt")), file.path(src0, "stuff", "blah", "kanon.txt"))

    dest1 <- tempfile()
    cloneDirectory(src, dest1, action="link")
    expect_identical(readLines(file.path(dest1, "foobar.txt")), LETTERS) 
    expect_identical(readLines(file.path(dest1, "stuff", "whee.txt")), as.character(1:10))
    expect_identical(readLines(file.path(dest1, "stuff", "blah", "kanon.txt")), "air")
    expect_identical(Sys.readlink(file.path(dest1, "foobar.txt")), "")
    expect_identical(Sys.readlink(file.path(dest1, "stuff", "whee.txt")), "")
    expect_identical(Sys.readlink(file.path(dest1, "stuff", "blah", "kanon.txt")), "")

    dest2 <- tempfile()
    cloneDirectory(src, dest2, action="copy")
    expect_identical(readLines(file.path(dest2, "foobar.txt")), LETTERS) 
    expect_identical(readLines(file.path(dest2, "stuff", "whee.txt")), as.character(1:10))
    expect_identical(readLines(file.path(dest2, "stuff", "blah", "kanon.txt")), "air")
    expect_identical(Sys.readlink(file.path(dest2, "foobar.txt")), "")
    expect_identical(Sys.readlink(file.path(dest2, "stuff", "whee.txt")), "")
    expect_identical(Sys.readlink(file.path(dest2, "stuff", "blah", "kanon.txt")), "")

    # Checking what happens if the test directory has relative symlinks.
    src2 <- tempfile()
    dir.create(file.path(src2, "stuff", "blah"), recursive=TRUE)
    write(file=file.path(src2, "foobar.txt"), LETTERS)
    file.symlink(file.path("..", "..", "foobar.txt"), file.path(src2, "stuff", "blah", "kanon.txt"))
    file.symlink(file.path("blah", "kanon.txt"), file.path(src2, "stuff", "whee.txt")) # recursive symlink
    alt <- tempfile(tmpdir=dirname(src2))
    write(file=alt, "ayu")
    file.symlink(file.path("..", "..", "..", basename(alt)), file.path(src2, "stuff", "blah", "tsukimiya.txt"))

    expect_identical(absolutizePath(alabaster.base:::resolve_symlinks(file.path(src2, "stuff", "whee.txt"))), absolutizePath(file.path(src2, "foobar.txt")))
    expect_identical(absolutizePath(alabaster.base:::resolve_symlinks(file.path(src2, "stuff", "blah", "tsukimiya.txt"))), absolutizePath(alt))

    dest3 <- tempfile()
    cloneDirectory(src2, dest3, action="link")
    expect_identical(readLines(file.path(dest3, "foobar.txt")), LETTERS) 
    expect_identical(readLines(file.path(dest3, "stuff", "whee.txt")), LETTERS)
    expect_identical(readLines(file.path(dest3, "stuff", "blah", "kanon.txt")), LETTERS)
    expect_identical(readLines(file.path(dest3, "stuff", "blah", "tsukimiya.txt")), "ayu")
    expect_identical(Sys.readlink(file.path(dest3, "foobar.txt")), "")
    expect_identical(Sys.readlink(file.path(dest3, "stuff", "whee.txt")), "")
    expect_identical(Sys.readlink(file.path(dest3, "stuff", "blah", "kanon.txt")), "")
})

test_that("cloneDirectory works with relative symlinks", {
    skip_on_os("windows")

    workspace <- tempfile()
    dir.create(workspace)
    src <- file.path(workspace, "A")
    create_test_directory(src)

    dest <- file.path(workspace, "B")
    cloneDirectory(src, dest, action="relsymlink")
    expect_identical(readLines(file.path(dest, "foobar.txt")), LETTERS) 
    expect_identical(readLines(file.path(dest, "stuff", "whee.txt")), as.character(1:10))
    expect_identical(readLines(file.path(dest, "stuff", "blah", "kanon.txt")), "air")

    expect_identical(Sys.readlink(file.path(dest, "foobar.txt")), file.path("..", "A", "foobar.txt"))
    expect_identical(Sys.readlink(file.path(dest, "stuff", "whee.txt")), file.path("..", "..", "A", "stuff", "whee.txt"))
    expect_identical(Sys.readlink(file.path(dest, "stuff", "blah", "kanon.txt")), file.path("..", "..", "..", "A", "stuff", "blah", "kanon.txt"))

    # Checking that the relative path discovery works as expected.
    test <- file.path(workspace, "C", "D")
    dir.create(test, recursive=TRUE)
    expect_identical(alabaster.base:::relative_path_to_src(src, test), "../../A")
    expect_identical(alabaster.base:::relative_path_to_src(test, dest), "../C/D")
})
