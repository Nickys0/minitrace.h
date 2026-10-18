/// TESTFILE:   minitest_with_inline.c
/// DEPENDENCY: libunwind, libdw
//
/// Test of minitrace.h with aggressive inlining
///
/// Various standard are supported (C99, C11, C17, C23)
///
/// COMMAND: cc -Wall -Wextra -std=c99 -g -O2 -DWITH_GZFILEOP -DMT_IMPLEMENTATION -o minitest_with_inline minitest_with_inline.c -lunwind -ldw -lelf
///
#include "../minitrace.h"

/* Volatile sink used to prevent tail-call optimization and dead-code elimination */
static volatile int g_sink = 0;

__attribute__((always_inline))
static inline int level3_leaf_inlined(int x, int y) {
    g_sink += (x + y);
    mt_print_current_stack_trace();
    __asm__ volatile("" : "+r"(x), "+r"(y));
    g_sink += (x * y);
    return g_sink;
}

__attribute__((always_inline))
static inline int level2_mid_inlined(int x, int y) {
    g_sink += (x * y);
    int res = level3_leaf_inlined(x + 1, y + 2);
    __asm__ volatile("" : "+r"(x), "+r"(res));
    g_sink += (res - x);
    return g_sink;
}

__attribute__((always_inline))
static inline int level1_top_inlined(int x, int y) {
    g_sink += (x - y);
    int res = level2_mid_inlined(x * 2, y + 5);
    __asm__ volatile("" : "+r"(y), "+r"(res));
    g_sink += (res + y);
    return g_sink;
}

__attribute__((noinline)) 
static int worker_function(int x, int y) {
    g_sink += (x ^ y);
    int res = level1_top_inlined(x, y);
    __asm__ volatile("" : "+r"(res));
    g_sink += res;
    return g_sink;
}

int main(void) {
    worker_function(10, 20);
    return 0;
}