#include <stddef.h>
#include "utils.h"

void stripTrailingNewlines(char *buffer, size_t *n) {
    int x = *n;
    if (x == 0) {
        return;
    }
    while (x > 0 && buffer[x - 1] == '\n') {
        --x;
    }
    buffer[x] = '\0';
    *n = x;
}
