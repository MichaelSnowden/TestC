#include <zconf.h>
#include <stdio.h>
#include <stdlib.h>
#include <testc/test_runner.h>
#include <testc/test_suite.h>
#include <testc/assert.h>

TEST(fast) {

}

TEST(sleep1) {
    printf("going to sleep...\n");
    sleep(1);
    printf("done sleeping\n");
}

SUITE(slow, &sleep1)

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
    sleep(1);
    exit(EXIT_FAILURE);
}

TEST(sleepThenDereferenceNullPointer) {
    sleep(1);
    int *x = NULL;
    *x = 0;
}

TEST(modifyConstString) {
    char *s = "a";
    s[0] = 0;
}

SUITE(errors, &sleepThenFail, &sleepThenDereferenceNullPointer, &modifyConstString)


void assertResults(TestNode *root, char *path, int expectedNumPassed, int expectedNumFailed) {
    TestNode *node = findNode(root, path);
    ASSERT_NEQ(node, NULL, TestNode*, %p);
    if (node->isLeaf) {
        ASSERT_EQ(node->state, TestState_DONE, int, %d);
        int passed = WIFEXITED(node->exitSignal)
                && (WEXITSTATUS(node->exitSignal) == EXIT_SUCCESS);
        if (expectedNumPassed == 1) {
            ASSERT_EQ(expectedNumFailed, 0, int, %d);
            ASSERT_EQ(passed, 1, int, %d);
        } else if (expectedNumFailed == 1) {
            ASSERT_EQ(passed, 0, int, %d);
        } else {
            fprintf(stderr, "invalid assertion for leaf test node\n");
            exit(EXIT_FAILURE);
        }
    } else {
        ASSERT_EQ(node->numTests, expectedNumPassed + expectedNumFailed, int, %d);
        ASSERT_EQ(node->numPassed, expectedNumPassed, int, %d);
        ASSERT_EQ(node->numFailed, expectedNumFailed, int, %d);
    }
}

void foo() {
    ASSERT_EQ(2+2, 3, int, %d);
}

TEST(stackTrace) {
    foo();
}

SUITE(exampleTestSuite, &fast, &nestedTestSuite, &fileIO, &slow, &errors, &stackTrace)

TEST(testTestRunner) {
    TestRunOptions options = {
            .animate = 1,
            .fps = 30.f,
            .filter = NULL,
            .noFork = 0,
    };
    TestNode *result;
    ASSERT_EQ(TestC_run(&exampleTestSuite, options, &result),0, int, %d);

    assertResults(result, "exampleTestSuite.fast", 1, 0);
    assertResults(result, "exampleTestSuite.nestedTestSuite", 8, 0);
    assertResults(result, "exampleTestSuite.nestedTestSuite.abcd", 4, 0);
    assertResults(result, "exampleTestSuite.nestedTestSuite.abcd.ab", 2, 0);
    assertResults(result, "exampleTestSuite.nestedTestSuite.abcd.ab.a", 1, 0);
    assertResults(result, "exampleTestSuite.fileIO", 4, 0);
    assertResults(result, "exampleTestSuite.slow", 1, 0);
    assertResults(result, "exampleTestSuite.errors", 0, 3);
    assertResults(result, "exampleTestSuite.stackTrace", 0, 1);

    // TODO: add tests to verify file I/O to test logs directory

    printf("Test runner test passed!\n");
}