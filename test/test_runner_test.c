#include <zconf.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <testc/test_runner.h>
#include <testc/test_suite.h>

TEST(fast) {

}

TEST(sleep1) {
    printf("going to sleep...\n");
    sleep(1);
    printf("done sleeping\n");
}

TEST(sleep3) {
    printf("going to sleep...\n");
    sleep(3);
    printf("done sleeping!\n");
}

SUITE(slow, &sleep1, &sleep3)

TEST(a) {}

TEST(b) {}

TEST(c) {}

TEST(d) {}

TEST(e) {}

TEST(f) {}

TEST(g) {}

TEST(h) {}

SUITE(ab, &a, &b)

SUITE(cd, &c, &d)

SUITE(ef, &e, &f)

SUITE(efg, &ef, &g)

SUITE(efgh, &efg, &h)

SUITE(abcd, &ab, &cd)


SUITE(nestedTestSuite, &abcd, &efgh)

TEST(printToStdout) {
    printf("hi: %d\n", __LINE__);
}

TEST(printToStderr) {
    fprintf(stderr, "hi: %d\n", __LINE__);
}

TEST(printToStdoutNoNewline) {
    printf("hi: %d", __LINE__);
}

TEST(printToStderrNoNewline) {
    fprintf(stderr, "hi: %d", __LINE__);
}

SUITE(fileIO, &printToStdout, &printToStderr, &printToStdoutNoNewline, &printToStderrNoNewline)

TEST(sleepThenFail) {
    sleep(2);
    exit(EXIT_FAILURE);
}

TEST(sleepThenDereferenceNullPointer) {
    sleep(3);
    int *x = NULL;
    *x = 0;
}

TEST(modifyConstString) {
    char *s = "a";
    s[0] = 0;
}

SUITE(errors, &sleepThenFail, &sleepThenDereferenceNullPointer, &modifyConstString)

SUITE(exampleTestSuite, &fast, &nestedTestSuite, &fileIO, &slow, &errors)

void assertResults(TestNode *root, char *path, int numPassed, int numFailed) {
    printf("verifying that the results of %s were %d passed and %d failed...\n", path, numPassed,
           numFailed);
    TestNode *node = findNode(root, path);
    assert(node != NULL);
    if (node->isLeaf) {
        assert(node->state == TestState_DONE);
        int passed = WIFEXITED(node->exitSignal) && (WEXITSTATUS(node->exitSignal) == 0);
        if (numPassed == 1) {
            assert(numFailed == 0);
            assert(passed);
        } else if (numFailed == 1) {
            assert(!passed);
        } else {
            fprintf(stderr, "invalid assertion for leaf test node\n");
            exit(EXIT_FAILURE);
        }
    } else {
        assert(node->numTests == numPassed + numFailed);
        assert(node->numPassed == numPassed);
        assert(node->numFailed == numFailed);
    }
}

TEST(testTestRunner) {
    TestRunOptions options = {
            .animate = 1,
            .fps = 30.f,
            .filter = NULL,
            .noFork = 0,
    };
    TestNode *result;
    assert(TestC_run(&exampleTestSuite, options, &result) == 0);

    assertResults(result, "exampleTestSuite.fast", 1, 0);
    assertResults(result, "exampleTestSuite.nestedTestSuite", 8, 0);
    assertResults(result, "exampleTestSuite.nestedTestSuite.abcd", 4, 0);
    assertResults(result, "exampleTestSuite.nestedTestSuite.abcd.ab", 2, 0);
    assertResults(result, "exampleTestSuite.nestedTestSuite.abcd.ab.a", 1, 0);
    assertResults(result, "exampleTestSuite.fileIO", 4, 0);
    assertResults(result, "exampleTestSuite.slow", 2, 0);
    assertResults(result, "exampleTestSuite.errors", 0, 3);

    // TODO: add tests to verify file I/O to test logs directory

    printf("Test runner test passed!\n");
}