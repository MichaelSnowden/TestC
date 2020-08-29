#ifndef TESTC_TEST_SUITE_H
#define TESTC_TEST_SUITE_H

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

#endif