#include "testc/test_suite.h"
#include "testc/test_runner.h"
#include <fcntl.h>
#include <assert.h>
#include <memory.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <pthread.h>

// See https://en.wikipedia.org/wiki/ANSI_escape_code
#define ESC "\033" // Begin an escape sequence
#define CSI ESC "[" // Control Sequence Introducer
#define CLEAR_SCREEN ESC "c" CSI "3J"
#define FAILED_TEST_COLOR CSI "1;31m"
#define PASSED_TEST_COLOR CSI "1;32m"
#define RUNNING_TEST_COLOR CSI "1;34m"
#define RESET_COLOR CSI "0m"

// Takes a time in nanoseconds and writes it as a nice human-readable format to fd
int humanizeDuration(long long nanos, int fd) {
    long long micros = nanos / 1000;
    long long millis = micros / 1000;
    int seconds = (int) (millis / 1000);
    int minutes = seconds / 60;
    if (minutes > 0) {
        return dprintf(fd, "%dm%ds", minutes, seconds % 60);
    }
    if (seconds > 0) {
        return dprintf(fd, "%d.%03llds", seconds % 60, millis % 1000);
    }
    if (millis > 0) {
        return dprintf(fd, "%lld.%03lldms", millis % 1000, micros % 1000);
    }
    if (micros > 0) {
        return dprintf(fd, "%lld.%03lldµs", micros % 1000, nanos % 1000);
    }
    return dprintf(fd, "%lldns", nanos);
}

// TestSuites are constant and only capture the test definition, but TestNodes are variable: they
// have a test state (e.g. running, passed/failed). This function takes a TestSuite and recursively
// builds a TestNode graph
TestNode *buildGraph(TestNode *parent, const TestSuite *suite, int *numTests) {
    TestNode *node = calloc(1, sizeof(TestNode));
    node->parent = parent;
    node->name = suite->name;
    if (suite->isLeaf) {
        node->isLeaf = 1;
        node->test = suite->test;
        node->state = TestState_IDLE;
        ++*numTests;
    } else {
        node->isLeaf = 0;
        int numChildren = suite->numChildren;
        node->numChildren = numChildren;
        node->children = malloc(sizeof(TestNode *) * numChildren);
        node->numTests = 0;
        node->numPassed = 0;
        node->numFailed = 0;
        for (int i = 0; i < numChildren; ++i) {
            node->children[i] = buildGraph(node, suite->children[i], &node->numTests);
        }
        *numTests += node->numTests;
    }
    return node;
}

// Render the state of a test progress spinner
const char *renderProgress(int *state) {
    const int n = 4;
    const char *s;
    switch (*state) {
        case 0:
            s = "◐";
            break;
        case 1:
            s = "◓";
            break;
        case 2:
            s = "◑";
            break;
        case 3:
            s = "◒";
            break;
        default:
            fprintf(stderr, "progress is in invalid state: %d\n", *state);
            exit(EXIT_FAILURE);
    }
    *state = (*state + 1) % n;
    return s;
}

// Get the amount of time elapsed between two timespecs in nanoseconds
long long getElapsedNanos(const struct timespec *start, const struct timespec *end) {
    long long nanos = end->tv_nsec - start->tv_nsec;
    nanos += 1000 * 1000 * 1000 * (end->tv_sec - start->tv_sec);
    return nanos;
}

// Render a test node recursively
int renderTestNode(TestNode *node, int indent, int fd) {
    dprintf(fd, "%*c%s: ", indent, ' ', node->name);
    if (node->isLeaf) {
        switch (node->state) {
            case TestState_IDLE:
                fprintf(stderr, "tests must be running before render is called\n");
                return -1;
            case TestState_RUNNING:
                dprintf(fd, RUNNING_TEST_COLOR "%s" RESET_COLOR "\n",
                        renderProgress(&node->progressIndicatorState));
                break;
            case TestState_DONE: {
                int exitSignal = node->exitSignal;
                if (WIFEXITED(exitSignal)) {
                    if (WEXITSTATUS(exitSignal) == 0) {
                        dprintf(fd, PASSED_TEST_COLOR "passed" RESET_COLOR);
                    } else {
                        dprintf(fd, FAILED_TEST_COLOR "exited: %s" RESET_COLOR,
                                strsignal(WEXITSTATUS(exitSignal)));
                    }
                } else if (WIFSIGNALED(exitSignal)) {
                    dprintf(fd, FAILED_TEST_COLOR "terminated: %s" RESET_COLOR,
                            strsignal(WTERMSIG(exitSignal)));
                } else if (WIFSTOPPED(exitSignal)) {
                    dprintf(fd, FAILED_TEST_COLOR "stopped: %s" RESET_COLOR,
                            strsignal(WSTOPSIG(exitSignal)));
                } else {
                    fprintf(stderr, "unknown process status for test: %d", exitSignal);
                    return -1;
                }
                dprintf(fd, " (");
                long long nanos = getElapsedNanos(&node->start, &node->end);
                humanizeDuration(nanos, fd);
                dprintf(fd, ")\n");
                break;
            }

            default:
                fprintf(stderr, "found test node in invalid state %s: %d\n", node->name,
                        node->state);
                return -1;
        }
    } else {
        dprintf(fd, "(");
        int numRunning = node->numTests - node->numPassed - node->numFailed;
        int prev = 0;
        if (numRunning > 0) {
            prev = 1;
            dprintf(fd, RUNNING_TEST_COLOR "%d" RESET_COLOR, numRunning);
        }
        if (node->numPassed > 0) {
            if (prev) {
                dprintf(fd, ",");
            }
            dprintf(fd, PASSED_TEST_COLOR "%d" RESET_COLOR, node->numPassed);
            prev = 1;
        }
        if (node->numFailed > 0) {
            if (prev) {
                dprintf(fd, ",");
            }
            dprintf(fd, FAILED_TEST_COLOR "%d" RESET_COLOR, node->numFailed);
        }
        dprintf(fd, ")\n");
        for (int i = 0; i < node->numChildren; ++i) {
            TestNode *child = node->children[i];
            if (
                    renderTestNode(child, indent
                                          + 2, fd)) {
                fprintf(stderr,
                        "%s failed to render child\n", node->name);
                return -1;
            }
        }
    }
    return 0;
}

// Start the test in a child process, redirecting output to the provided file descriptors
int startTest(void (*test)(), int stdoutFd, int stderrFd) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("failed to fork for test");
        return -1;
    }
    if (pid == 0) {
        assert(dup2(stdoutFd, STDOUT_FILENO) != -1);
        assert(dup2(stderrFd, STDERR_FILENO) != -1);
        test();
        exit(EXIT_SUCCESS);
    }
    return pid;
}

// Start all the tests in a node, recursively. The path argument is the filepath where the test
// output will go, and it is modified in-place. It should be a buffer of size PATH_MAX,
// initialized to a c-string of the root directory of where test output will go.
int startTestNode(TestNode *node, char path[PATH_MAX]) {
    size_t pathLength = strlen(path);
    path[pathLength] = '/';
    size_t nameLength = strlen(node->name);
    memcpy(path + pathLength + 1, node->name, nameLength);
    *(path + pathLength + 1 + nameLength) = '\0';

    if (node->isLeaf) {
        node->state = TestState_RUNNING;
        clock_gettime(CLOCK_MONOTONIC, &node->start);
        FILE *file;
        strcat(path, ".txt");
        file = fopen(path, "w");
        if (file == NULL) {
            fprintf(stderr, "failed to create log file at %s: %s\n", path, strerror(errno));
            return -1;
        }
        node->outputFile = file;

        path[pathLength] = '\0';

        pid_t testPid = startTest(node->test, fileno(file), fileno(file));
        if (testPid < 0) {
            fprintf(stderr, "failed to start test: %s\n", node->name);
            return -1;
        }
        node->pid = testPid;
    } else {
        int mkdirStatus = mkdir(path, 0777);
        if (mkdirStatus) {
            if (errno != EEXIST) {
                fprintf(stderr, "mkdir returned %d for %s: %s\n", mkdirStatus, path,
                        strerror(errno));
                return -1;
            }
            printf("%s already exists\n", path);
        }
        for (int i = 0; i < node->numChildren; ++i) {
            TestNode *child = node->children[i];
            if (startTestNode(child, path)) {
                fprintf(stderr, "%s failed to start graph for %s\n", node->name,
                        child->name);
                return -1;
            }
        }
    }
    path[pathLength] = '\0';
    return 0;
}

// Remove a trailing slash, if it exists, from a filepath.
void removeTrailingSlash(char *path) {
    size_t pathLength = strlen(path);
    if (pathLength > 0 && path[pathLength - 1] == '/') {
        path[pathLength - 1] = '\0';
    }
}

// Each test node has a process id; this function searches for a node with the provided pid,
// recursively and returns NULL if no such node exists.
TestNode *findNodeWithPid(TestNode *node, pid_t pid) {
    if (node->isLeaf) {
        if (node->pid == pid) {
            return node;
        }
    } else {
        for (int i = 0; i < node->numChildren; ++i) {
            TestNode *result = findNodeWithPid(node->children[i], pid);
            if (result != NULL) {
                return result;
            }
        }
    }
    return NULL;
}

// Render the root test node, outputting to the provided FILE. It needs to be a FILE and not a
// file descriptor because we need to flush it in order for the ANSI stuff (terminal colors) to
// work properly.
int renderRootTestNode(TestNode *root, FILE *file) {
    printf(CLEAR_SCREEN); // Clear the console
    fflush(file);
    dprintf(fileno(file), "TestC\n");
    if (renderTestNode(root, 0, fileno(file))) {
        fprintf(stderr, "failed to render graph\n");
        return -1;
    }
    fflush(file);
    return 0;
}

typedef struct {
    // There are two threads which can write to the screen, this makes it so only one will render
    // at a time.
    pthread_mutex_t *screenRenderMutex;

    TestNode *root;

    // The rate at which the spinners will re-render.
    float fps;

    // The exit status of the render thread, not the tests.
    int exitStatus;
} RenderThreadArgs;

// The function that the render thread will run. It re-renders the screen every 1/fps seconds, but
// it uses a mutex so that it doesn't interfere with screen renders that come from the main thread.
void *renderLoop(void *input) {
    RenderThreadArgs *args = input;
    long us = (long) (1000.f * 1000.f / args->fps);
    while (1) {
        if (pthread_mutex_lock(args->screenRenderMutex)) {
            perror("render loop failed to lock mutex");
            args->exitStatus = 1;
            return NULL;
        }
        if (args->root->numPassed + args->root->numFailed == args->root->numTests) {
            if (pthread_mutex_unlock(args->screenRenderMutex)) {
                perror("render loop failed to unlock mutex while trying to exit");
                args->exitStatus = 1;
                return NULL;
            }
            args->exitStatus = 0;
            return NULL;
        }
        if (renderRootTestNode(args->root, stdout)) {
            fprintf(stderr, "render loop failed to render graph\n");
            args->exitStatus = 1;
            return NULL;
        }
        if (pthread_mutex_unlock(args->screenRenderMutex)) {
            perror("render loop failed to unlock mutex");
            args->exitStatus = 1;
            return NULL;
        }
        usleep(us);
    }
}

// Recursively free a test node
void freeNode(TestNode *node) {
    if (!node->isLeaf) {
        for (int i = 0; i < node->numChildren; ++i) {
            freeNode(node->children[i]);
        }
        free(node->children);
    }
    free(node);
}

// This method is useful when you want to debug a specific test since follow-fork-mode is
// GDB-specific, IDE specific, and it is not suited to parallel execution.
void TestC_runNoFork(const TestSuite *suite) {
    if (suite->isLeaf) {
        printf(RUNNING_TEST_COLOR "Testing %s\n" RESET_COLOR, suite->name);
        suite->test();
    } else {
        for (int i = 0; i < suite->numChildren; ++i) {
            const struct TestSuite *child = suite->children[i];
            TestC_runNoFork(child);
        }
    }
}

// Find the test suite with the given path e.g. `all.http.parser.badRequest`
const TestSuite *findSuite(const TestSuite *suite, const char *filter) {
    if (strcmp(suite->name, filter) == 0) {
        return suite;
    }

    if (!suite->isLeaf) {
        char *split = strchr(filter, '.');
        assert(split != NULL);
        filter = split + 1;
        for (int i = 0; i < suite->numChildren; ++i) {
            const TestSuite *child = suite->children[i];
            const TestSuite *result = findSuite(child, filter);
            if (result != NULL) {
                return result;
            }
        }
    }
    return NULL;
}

// Utility method which is so far only used in the test for this file.
TestNode *findNode(TestNode *node, const char *filter) {
    if (strcmp(node->name, filter) == 0) {
        return node;
    }

    if (!node->isLeaf) {
        char *split = strchr(filter, '.');
        if (split == NULL) {
            return NULL;
        }
        filter = split + 1;
        for (int i = 0; i < node->numChildren; ++i) {
            TestNode *child = node->children[i];
            TestNode *result = findNode(child, filter);
            if (result != NULL) {
                return result;
            }
        }
    }
    return NULL;
}

// When the test runner finishes, there probably be a lot of empty files which were created during
// the test, but never written to. This method cleans up all those empty files.
int deleteEmptyLogs(const TestNode *node, char *path, int *deleted) {
    int len = (int) strlen(path);
    strcat(path, "/");
    strcat(path, node->name);
    if (node->isLeaf) {
        strcat(path, ".txt");
        if (fseek(node->outputFile, 0, SEEK_END) != 0) {
            fprintf(stderr, "failed to seek end of output %s: %s\n", path, strerror(errno));
            return 1;
        }
        long size = ftell(node->outputFile);
        if (fclose(node->outputFile) != 0) {
            fprintf(stderr, "failed to close %s: %s\n", path, strerror(errno));
            return 1;
        }
        if (size == 0) {
            if (remove(path) != 0) {
                perror("failed to delete node's output file");
                return 1;
            }
            *deleted = 1;
        } else {
            *deleted = 0;
        }
    } else {
        int numChildren = node->numChildren;
        int numDeleted = 0;
        int childDeleted = 0;
        for (int i = 0; i < node->numChildren; ++i) {
            TestNode *child = node->children[i];
            if (deleteEmptyLogs(child, path, &childDeleted)) {
                return 1;
            }
            if (childDeleted) {
                ++numDeleted;
            }
        }
        if (numDeleted == numChildren && node->parent != NULL) {
            if (remove(path) != 0) {
                perror("failed to delete test log subdirectory");
                return 1;
            }
            *deleted = 1;
        } else {
            *deleted = 0;
        }
    }
    *(path + len) = '\0';
    return 0;
}

// Run a test suite by converting it into a test node and then running that test node. If the result
// argument is non-NULL, the results of the test can be inspected, but it's up to the caller to
// run freeNode(*result).
int TestC_run(const TestSuite *suite, TestRunOptions options, TestNode **result) {
    char dir[PATH_MAX];

    if (options.noFork == 0) {
        if (options.dir == NULL) {
            getcwd(dir, PATH_MAX - 1);
            removeTrailingSlash(dir);
            strcat(dir, "/test_logs");

            if (mkdir(dir, 0777) < 0 && errno != EEXIST) {
                fprintf(stderr, "failed to create root test logs directory at %s: %s\n", dir,
                        strerror(errno));
                return 1;
            }
        } else {
            strcpy(dir, options.dir);
        }

        struct timespec start;
        clock_gettime(CLOCK_MONOTONIC, &start);
        long micros = start.tv_sec * 1000 * 1000 + start.tv_nsec / 1000;
        size_t rootDirPathLength = strlen(dir);
        sprintf(dir + rootDirPathLength, "/%016ld", micros);
        if (mkdir(dir, 0777) < 0) {
            fprintf(stderr, "failed to create test run directory for this run at %s: %s", dir,
                    strerror
                            (errno));
            return 1;
        }
        char rootDirAbsolutePath[PATH_MAX];
        dir[rootDirPathLength] = '\0';
        if (realpath(dir, rootDirAbsolutePath) == NULL) {
            fprintf(stderr, "failed to convert directory to absolute path %s: %s\n", dir,
                    strerror(errno));
            return -1;
        }
        dir[rootDirPathLength] = '/';

        char symlinkTargetPath[PATH_MAX];
        strcpy(symlinkTargetPath, rootDirAbsolutePath);
        strcat(symlinkTargetPath, dir + rootDirPathLength);
        char symlinkLabelPath[PATH_MAX];
        strcpy(symlinkLabelPath, rootDirAbsolutePath);
        strcat(symlinkLabelPath, "/latest");
        remove(symlinkLabelPath);
        if (symlink(symlinkTargetPath, symlinkLabelPath)) {
            fprintf(stderr, "failed to symlink target=%s, link_name=%s: %s\n", symlinkTargetPath,
                    symlinkLabelPath, strerror(errno));
            return -1;
        }
        printf("running suite and outputting logs to %s\n", dir);
    }

    if (options.filter != NULL) {
        suite = findSuite(suite, options.filter);
        if (suite == NULL) {
            fprintf(stderr, "failed to find test suite at path %s\n", options.filter);
            return -1;
        }
    }

    if (options.noFork) {
        TestC_runNoFork(suite);
        return 0;
    }

    const float fps = options.fps;
    const int renderProgress = options.animate;

    if (renderProgress && fps <= 0) {
        fprintf(stderr, "fps (%f) must be greater than zero if progress rendering is on\n", fps);
        return -1;
    }
    int numTests = 0;
    TestNode *root = buildGraph(NULL, suite, &numTests);
    if (result != NULL) {
        *result = root;
    }
    int numDone = 0;
    if (startTestNode(root, dir)) {
        fprintf(stderr, "failed to start tests\n");
        freeNode(root);
        return -1;
    }

    //region: Double-buffer stdout output to reduce jitters
    // So far doesn't seem to help in embedded CLion terminal
    size_t bufferCapacity = 4096;
    char buffer[bufferCapacity];
    if (setvbuf(stdout, buffer, _IOFBF, bufferCapacity)) {
        perror("failed to set stdout buffer");
    }
    //endregion

    pthread_mutex_t renderMutex;
    pthread_mutex_init(&renderMutex, NULL);
    RenderThreadArgs renderThreadArgs = {
            .root = root,
            .screenRenderMutex = &renderMutex,
            .fps = fps
    };
    pthread_t renderThread;
    if ((renderProgress) && pthread_create(&renderThread, NULL, renderLoop,
                                           &renderThreadArgs)) {
        perror("failed to create render thread");
        freeNode(root);
        return -1;
    }

    while (numDone < numTests) {
        int testSignal, waitStatus;
        wait:
        waitStatus = wait(&testSignal);
        if (waitStatus < 0) {
            if (errno == EINTR) {
                goto wait;
            }
            fprintf(stderr, "failed to wait with %d/%d done: %s\n", numDone, numTests, strerror
                    (errno));
            err:
            if (renderProgress && pthread_cancel(renderThread)) {
                perror("failed to cancel render thread while cleaning up wait loop");
            }
            freeNode(root);
            return -1;
        }
        pid_t pid = waitStatus;
        TestNode *node = findNodeWithPid(root, pid);
        if (node == NULL) {
            fprintf(stderr, "got a signal for a subprocess that doesn't exist in the test "
                            "suite (pid=%d, signal=%d), ignoring.\n", pid, testSignal);
            continue;
        }
        if (WIFCONTINUED(testSignal)) {
            printf("received continue signal for test: %s\n", node->name);
            continue;
        }
        if (renderProgress && pthread_mutex_lock(&renderMutex)) {
            perror("wait loop failed to lock mutex");
            goto err;
        }
        if (node->state == TestState_DONE) {
            fprintf(stderr, "got a signal from the subprocess for test %s but that test is "
                            "already marked done\n", node->name);
            goto err;
        }
        node->state = TestState_DONE;
        clock_gettime(CLOCK_MONOTONIC, &node->end);
        node->exitSignal = testSignal;
        while ((node = node->parent) != NULL) {
            if (WIFEXITED(testSignal) && WEXITSTATUS(testSignal) == 0) {
                node->numPassed += 1;
            } else {
                node->numFailed += 1;
            }
        }
        if (renderRootTestNode(root, stdout)) {
            fprintf(stderr, "failed to render graph in wait loop\n");
            goto err;
        }
        if (renderProgress && pthread_mutex_unlock(&renderMutex)) {
            perror("wait loop failed to unlock mutex");
            goto err;
        }
        ++numDone;
    }
    int status = renderRootTestNode(root, stdout);

    int rootDeleted = 0;
    if (deleteEmptyLogs(root, dir, &rootDeleted) != 0) {
        fprintf(stderr, "failed to delete logs at %s\n", dir);
        return -1;
    }
    if (rootDeleted) {
        fprintf(stderr, "test log root was accidentally deleted--this will break the \"latest\" "
                        "symlink");
        return -1;
    }

    if (result == NULL) {
        freeNode(root);
    }

    printf("Test results written to:\n%s\n", dir);
    return status;
}

// Some stuff for parsing command line arguments

typedef enum {
    // A void parameter means something like --nofork
    CommandLineParameterType_void,
    CommandLineParameterType_int,
    CommandLineParameterType_float,
    CommandLineParameterType_str,
} CommandLineParameterType;

typedef struct {
    // The name should not include dashes--those are added by the user only.
    const char *name;
    CommandLineParameterType type;
    int required;

    union {
        int *int_;
        float *float_;
        const char **str_;
    } parsedArgument;
    int numOptions;
    union {
        int *int_;
    } options;

    const char *doc;
} CommandLineParameter;

void printUsage(CommandLineParameter *parameters, int numParameters) {
    printf("TestC_main usage:\n");

    for (int i = 0; i < numParameters; ++i) {
        CommandLineParameter parameter = parameters[i];
        printf("  --%s", parameter.name);
        switch (parameter.type) {
            case CommandLineParameterType_void:
                printf(" [void]");
                break;
            case CommandLineParameterType_int:
                printf(" [int]");
                break;
            case CommandLineParameterType_float:
                printf(" [float]");
                break;
            case CommandLineParameterType_str:
                printf(" [string]");
                break;
        }
        printf("\n");
        if (parameter.required) {
            printf("    required");
        } else {
            printf("    default = ");
            switch (parameter.type) {
                case CommandLineParameterType_void:
                case CommandLineParameterType_int:
                    printf("%d", *parameter.parsedArgument.int_);
                    break;
                case CommandLineParameterType_float:
                    printf("%f", *parameter.parsedArgument.float_);
                    break;
                case CommandLineParameterType_str:
                    printf("%s", *parameter.parsedArgument.str_);
                    break;
            }
        }
        printf("\n");
        if (parameter.numOptions > 0) {
            printf("    options = ");
            for (int j = 0; j < parameter.numOptions; ++j) {
                if (j > 0) {
                    printf(",");
                }
                switch (parameter.type) {
                    case CommandLineParameterType_int:
                        printf("%d", parameter.options.int_[j]);
                        break;
                    default:
                        break;
                }
            }
            printf("\n");
        }
        printf("    description = %s\n\n", parameter.doc);
    }
}

typedef enum {
    ParseArgumentsResult_PARSED,
    ParseArgumentsResult_BAD_ARGS,
    ParseArgumentsResult_HELP,
} ParseArgumentsResult;

ParseArgumentsResult
parseArguments(CommandLineParameter *parameters, int numParameters, int argc, char **argv) {
    char **end = argv + argc;

    // skip the first argument, which is usually the path to the executable
    argv = argv + 1;

    while (argv != end) {
        char *parameterName = *(argv++);
        parameterName = parameterName + 2; // arg=--key so ignore the first 2 chars

        if (strcmp(parameterName, "help") == 0) {
            return ParseArgumentsResult_HELP;
        }

        int matched = 0;
        for (int i = 0; i < numParameters; ++i) {
            CommandLineParameter parameter = parameters[i];
            if (strcmp(parameterName, parameter.name) == 0) {
                matched = 1;

                char *value;
                if (parameter.type != CommandLineParameterType_void) {
                    if (argv == end) {
                        printf("expected an argument to %s\n", parameter.name);
                        goto badArgs;
                    }
                    value = *(argv++);
                }

                switch (parameter.type) {
                    case CommandLineParameterType_void: {
                        *parameter.parsedArgument.int_ = 1;
                    }
                        break;
                    case CommandLineParameterType_int: {
                        char *endptr;
                        int intVal = (int) strtol(value, &endptr, 10);
                        if (endptr == NULL) {
                            printf("%s expected an int but got %s\n", parameterName, value);
                            goto badArgs;
                        }
                        int gotOption = 0;
                        if (parameter.numOptions > 0) {
                            for (int j = 0; j < parameter.numOptions; ++j) {
                                if (intVal == parameter.options.int_[j]) {
                                    gotOption = 1;
                                    break;
                                }
                            }
                        }
                        if (!gotOption) {
                            printf("%s got an invalid value: %s\n", parameterName, value);
                            goto badArgs;
                        }
                        *parameter.parsedArgument.int_ = intVal;
                    }
                        break;
                    case CommandLineParameterType_float: {
                        char *endptr;
                        float floatVal = strtof(value, &endptr);
                        if (endptr == NULL) {
                            printf("%s expected a float but got %s\n", parameterName, value);
                            goto badArgs;
                        }
                        *parameter.parsedArgument.float_ = floatVal;
                    }
                        break;
                    case CommandLineParameterType_str: {
                        *parameter.parsedArgument.str_ = value;
                    }
                        break;
                }

                break;
            }
        }
        if (!matched) {
            fprintf(stderr, "Unknown option %s\n", parameterName);
            goto badArgs;
        }
    }
    return ParseArgumentsResult_PARSED;

    badArgs:
    printUsage(parameters, numParameters);
    return ParseArgumentsResult_BAD_ARGS;
}

// The journey of a thousand miles begins with a single step.
int TestC_main(const TestSuite *suite, int argc, char **argv) {

    TestRunOptions options;
    options.animate = 1;
    options.fps = 30.f;
    options.noFork = 0;
    options.dir = NULL;
    options.filter = NULL;

    CommandLineParameter parameters[] = {
            {
                    .name = "animate",
                    .type = CommandLineParameterType_int,
                    .numOptions = 2,
                    .options.int_ = (int[]) {0, 1},
                    .parsedArgument.int_ = &options.animate,
                    .doc = "should progress indicators animate"
            },
            {
                    .name = "fps",
                    .type = CommandLineParameterType_float,
                    .parsedArgument.float_ = &options.fps,
                    .doc = "framerate of progress indicator animation"
            },
            {
                    .name = "nofork",
                    .type = CommandLineParameterType_void,
                    .parsedArgument.int_ = &options.noFork,
                    .doc = "tests won't fork--good for debugging, but will crash upon first test "
                           "failure"
            },
            {
                    .name = "dir",
                    .type = CommandLineParameterType_str,
                    .parsedArgument.str_ = &options.dir,
                    .doc = "root of test logs--will place logs alongside other test log "
                           "directories within this root (the null default means this will be set"
                           " to $PWD/test_logs at runtime)"
            },
            {
                    .name = "filter",
                    .type = CommandLineParameterType_str,
                    .parsedArgument.str_ = &options.filter,
                    .doc = "a period-separated path to a test suite to run"
            }
    };
    int numParameters = sizeof(parameters) / sizeof(*parameters);


    ParseArgumentsResult parseArgumentsResult = parseArguments(parameters, numParameters, argc,
                                                               argv);
    if (parseArgumentsResult == ParseArgumentsResult_BAD_ARGS) {
        return 1;
    }
    if (parseArgumentsResult == ParseArgumentsResult_HELP) {
        printUsage(parameters, numParameters);
        return 0;
    }

    return TestC_run(suite, options, NULL);
}