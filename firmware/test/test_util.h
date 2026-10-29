/* Minimal dependency-free test helpers (no framework needed for CI). */
#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include <stdio.h>

static int g_checks;
static int g_failures;

#define CHECK(cond)                                                              \
    do {                                                                         \
        g_checks++;                                                              \
        if (!(cond)) {                                                           \
            g_failures++;                                                        \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);               \
        }                                                                        \
    } while (0)

#define TEST_EXIT()                                                              \
    do {                                                                         \
        printf("%s: %d checks, %d failures\n", __FILE__, g_checks, g_failures);  \
        return (g_failures == 0) ? 0 : 1;                                        \
    } while (0)

/* Deterministic xorshift32 so failures are reproducible. */
static inline unsigned int xorshift32(unsigned int *s)
{
    unsigned int x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

#endif /* TEST_UTIL_H */
