#ifndef TESTC_TEST_RUNNER_H
#define TESTC_TEST_RUNNER_H

#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include "testc/test_suite.h"

typedef struct {
    const char *dir;
    int animate;
    float fps;
    int noFork;

    // path to a test suite
    const char *filter;
} TestRunOptions;


typedef enum {
    TestState_IDLE,
    TestState_RUNNING,
    TestState_DONE
} TestState;

typedef struct TestNode {
    int isLeaf;
    const char *name;

    // time that the test started/ended
    struct timespec start;
    struct timespec end;

    // an int representing which state the progress indicator is in (0-3 for a 4-state spinner)
    int progressIndicatorState;

    struct TestNode *parent;

    union {
        // Properties only valid for leaf nodes
        struct {
            void (*test)();

            TestState state;

            // The signal that the test subprocess terminated with. See
            // https://linux.die.net/man/2/wait under "status", where WIFCONTINUED is false
            int exitSignal;

            // File descriptors for where the test output should be written

            // The pid of the test subprocess (test is forked to ensure parent process doesn't
            // crash).
            pid_t pid;

            FILE *outputFile;
        };

        // ...for parent nodes
        struct {
            int numChildren;
            struct TestNode **children;

            // Total number taken from children and indirect children (not just immediate children)
            int numTests;
            int numPassed;
            int numFailed;
        };
    };
} TestNode;

TestNode *findNode(TestNode *node, const char *filter);

/*
 * This function will run a test suite in parallel, spitting logs out to the target directory. The
 * target directory must exist. The fps argument is the number of frames per second that the
 * output should be rendered at. This will only work well on console that support ANSI escape codes.
 * Unfortunately, CLion does not support console clearing because its embedded terminal is more
 * like a log file. Instead, you should run this in an external terminal. You can reuse the
 * builds that CLion makes by finding the binary in cmake-build-debug.
 */
int TestC_run(const TestSuite *suite, TestRunOptions options, TestNode **result);

/*
 * Parses the arguments and then calls TestC_run--see docs in the implementation.
 */
int TestC_main(const TestSuite *suite, int argc, char **argv);

#endif
