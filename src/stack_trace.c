#include <execinfo.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/wait.h>
#include <stdlib.h>

void parseTraceMessage(char *message, char **executable, char **address) {
    char *cursor = message;
    while (*cursor != ' ') {
        ++cursor;
    }
    while (*cursor == ' ') {
        ++cursor;
    }
    *executable = cursor;
    cursor = strchr(cursor, ' ');
    *cursor = '\0';
    cursor = cursor + 1;
    while (*cursor == ' ') {
        ++cursor;
    }
    *address = cursor;
    cursor = strchr(cursor, ' ');
    *cursor = '\0';
}

void printStackTrace(int fd, int maxDepth) {
    void *trace[maxDepth];

    int depth = backtrace(trace, maxDepth);
    char **messages = backtrace_symbols(trace, depth);
    char *executable, *address;

    /* skip first stack frame (points here) */
    for (int i = 1; i < depth - 1; ++i) {
        parseTraceMessage(messages[i], &executable, &address);

        char command[256];
#ifdef __APPLE__
        sprintf(command, "atos --fullPath -o %.256s %s 2>&1", executable, address);
#else
        sprintf(command,"addr2line -f -p -e %.256s %p", executable, addr);
#endif
        FILE *outputFile = popen(command, "r");
        assert(outputFile != NULL);
        char output[1024];
        assert(fgets(output, sizeof(output), outputFile) != NULL);
        int status = pclose(outputFile);
        assert(WIFEXITED(status));
        if (WEXITSTATUS(status) == EXIT_SUCCESS) {
            if (strncmp(output, executable, strlen(executable)) == 0) {
                dprintf(fd, "%s", output);
                dprintf(fd, "looks like your compiler is missing debug information (add -g)\n");
            } else if (strncmp(output, "0x", 2) == 0) {
                dprintf(fd, "%s", output);
                dprintf(fd, "looks like your compiler has PIE turned on (add -fno-pie)\n");
            } else {
                char *function = output;
                char *sourceCodeLine = strrchr(output, '(') + 1;
                *strrchr(output, ')') = '\0';
                *strchr(function, ' ') = '\0';
                dprintf(fd, "%s (%s)\n", sourceCodeLine, function);
            }
        } else {
            dprintf(fd, "atos error: %soriginal command: %s\n", output, command);
        }
    }
}