#include "testc/test_suite.h"

int TestSuite_numTests(const TestSuite *suite) {
    if (!suite->isLeaf) {
        int count = 0;
        for (int i = 0; i < suite->numChildren; ++i) {
           count += TestSuite_numTests(suite->children[i]);
        }
        return count;
    }
    return 1;
}
