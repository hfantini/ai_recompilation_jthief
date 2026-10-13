#ifndef JT_TEST_H
#define JT_TEST_H

#include <stdio.h>

/* CTest treats this exit code as "skipped" (see SKIP_RETURN_CODE). */
#define TEST_SKIP_CODE 77

static int test_failures = 0;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__,   \
                    #condition);                                                \
            test_failures++;                                                    \
        }                                                                       \
    } while (0)

#define TEST_RESULT() (test_failures == 0 ? 0 : 1)

#endif
