#include "foo.h"
#include <testc/stack_trace.h>
#include <unistd.h>

void bar() {
    printStackTrace(STDOUT_FILENO, 16);
}