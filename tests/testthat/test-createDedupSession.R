# library(testthat); library(alabaster.base); source('test-createDedupSession.R')

library(S4Vectors)
test_that("deduplication utilities work as expected", {
    y <- DataFrame(A=1:10, B=1:10)

    # Saving the first instance of the object, which is now stored in the session.
    session <- createDedupSession()
    expect_null(checkObjectInDedupSession(y, session))

    tmp <- tempfile()
    dir.create(tmp)
    addObjectToDedupSession(y, session, tmp)
    expect_identical(checkObjectInDedupSession(y, session), absolutizePath(tmp)) # need to clean the path for a valid comparison.

    # Doesn't respond this way for different objects.
    z <- y
    z$A <- z$A + 1
    expect_null(checkObjectInDedupSession(z, session))

    tmp2 <- tempfile()
    dir.create(tmp2)
    addObjectToDedupSession(z, session, tmp2)
    expect_identical(checkObjectInDedupSession(y, session), absolutizePath(tmp))
    expect_identical(checkObjectInDedupSession(z, session), absolutizePath(tmp2))

    # Doesn't respond this way for a different session.
    session2 <- createDedupSession()
    expect_null(checkObjectInDedupSession(y, session2))
    expect_null(checkObjectInDedupSession(z, session2))
})
