#ifndef AVAR_TEST_H
#define AVAR_TEST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int avar_tests_run = 0;
static int avar_tests_failed = 0;

static void avar_test_fail(const char *file, int line, const char *expr) {
    fprintf(stderr, "  FAIL: %s:%d: %s\n", file, line, expr);
    avar_tests_failed++;
}

#define AVAR_ASSERT(expr)                                                                          \
    do {                                                                                           \
        avar_tests_run++;                                                                          \
        if (!(expr)) {                                                                             \
            avar_test_fail(__FILE__, __LINE__, #expr);                                             \
            return;                                                                                \
        }                                                                                          \
    } while (0)

#define AVAR_ASSERT_EQ(a, b) AVAR_ASSERT((a) == (b))
#define AVAR_ASSERT_NOT_NULL(p) AVAR_ASSERT((p) != NULL)
#define AVAR_ASSERT_NULL(p) AVAR_ASSERT((p) == NULL)
#define AVAR_ASSERT_STR_EQ(a, b) AVAR_ASSERT(strcmp((a), (b)) == 0)

#define AVAR_TEST(name)                                                                            \
    static void name(void);                                                                        \
    static void run_##name(void) {                                                                 \
        printf("  %s\n", #name);                                                                   \
        name();                                                                                    \
    }                                                                                              \
    static void name(void)

#define AVAR_TEST_MAIN(...)                                                                        \
    int main(void) {                                                                               \
        printf("Running tests...\n");                                                              \
        __VA_ARGS__                                                                                \
        printf("%d assertions, %d failed\n", avar_tests_run, avar_tests_failed);                     \
        return avar_tests_failed > 0 ? EXIT_FAILURE : EXIT_SUCCESS;                                \
    }

#endif
