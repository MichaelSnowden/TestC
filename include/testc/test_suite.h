#ifndef TESTC_TEST_SUITE_H
#define TESTC_TEST_SUITE_H

#include <string.h>
#include <execinfo.h>
#include <testc/stack_trace.h>

typedef void(*test_t)();

/*
 * A test suite defines a directed (hopefully acyclic) graph where nodes are suites and leaves are
 * tests. A suite doesn't have to be a tree, but it usually is. You should use the macros below to
 * declare tests and suites because doing them by hand is really slow.
 */
typedef struct TestSuite {
    const char *name;
    int isLeaf;

    union {
        struct {
            test_t test;
        };

        struct {
            const struct TestSuite **children;
            int numChildren;
        };
    };
} TestSuite;

int TestSuite_numTests(const TestSuite *suite);

/*
 * Usage: TEST(myTestName) { assert(foo() === expectedFoo) }
 */
#define TEST(testName) \
    void testName ## Method();\
    const TestSuite testName = {\
        .name = #testName,\
        .test = testName ## Method,\
        .isLeaf = 1\
    };            \
    void testName ## Method()

/*
 * Use SUITE when you want to define a non-leaf node in a test suite graph.
 * Usage:
 * TEST(a) { ... }
 * TEST(b) { ... }
 * SUITE(ab, &a, b)
 * TEST(c) { ... }
 * SUITE(abc, &ab, &c)
 *
 * then later tun the suite using TestC_run or TestC_main
 */
#define SUITE(suiteName, ...) \
    const TestSuite * suiteName ## Children[] = { __VA_ARGS__ }; \
    const TestSuite suiteName = { \
        .name = #suiteName,     \
        .isLeaf = 0,    \
        .numChildren = sizeof(suiteName ## Children) / sizeof(TestSuite *), \
        .children = suiteName ## Children,                            \
    };

// In the macros below the following variables are used
// exp, x, y: an expression e.g. foo()
// val: the value of an expression (something determined as late as runtime)
// cmp: a binary operator e.g. ==
// type: the type of a variable e.g. int
// format: the format for an expression when placed in a string e.g. %d (don't include quotes)

// If an expression's value is the same as the expression itself, don't print anything e.g. in
// ASSERT_EQ(status, 0), we want to see that status = 1 or whatever, but printing 0 = 0 doesn't
// tell us anything.
#define PRINT_ASSIGNMENT(exp, val, format) \
    {                             \
        int size = snprintf(NULL, 0, #format, val);                          \
        char buffer[size + 1];        \
        sprintf(buffer, #format, val);    \
        if (strcmp(#exp, buffer) != 0) { \
            fprintf(stderr, "  " #exp " = " #format "\n", val);\
        }                            \
    }

// Checks whether (x cmp y) is true and prints a nice error message if it's not.
#define ASSERT_BIN(cmp, x, y, type, format) \
    {                       \
        type xVal = x;       \
        type yVal = y;                   \
        if (!(xVal cmp yVal)) {   \
            fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);                                      \
            fprintf(stderr, "Assertion Failed: " #x " " #cmp " " #y " where:\n"); \
            PRINT_ASSIGNMENT(x, xVal, format)                      \
            PRINT_ASSIGNMENT(y, yVal, format)                                                    \
            printStackTrace(STDOUT_FILENO, 16);                                \
            exit(EXIT_FAILURE);       \
        } \
    }

#define ASSERT_EQ(x, y, type, format) \
    ASSERT_BIN(==, x, y, type, format)

#define ASSERT_NEQ(x, y, type, format) \
    ASSERT_BIN(!=, x, y, type, format)

#endif