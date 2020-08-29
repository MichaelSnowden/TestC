// no-header includes make writing tests less verbose
#pragma clang diagnostic push
#pragma ide diagnostic ignored "bugprone-suspicious-include"
#include "test_runner_test.c"
#pragma clang diagnostic pop
#include <testc/test_runner.h>

SUITE(all, &testTestRunner);

int main(int argc, char **argv) {
    return TestC_main(&all, argc, argv);
}