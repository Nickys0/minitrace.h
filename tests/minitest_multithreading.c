/// TESTFILE:   minitest_multithreading.c
/// DEPENDENCY: libunwind, libdw
///
/// Test of minitrace.h in multithreading
///
/// Various standard are supported (C99, C11, C17, C23)
///
/// COMMAND: cc -Wall -Wextra -std=c99 -ggdb -O1 -DWITH_GZFILEOP -DMT_IMPLEMENTATION -o minitest_multithreading minitest_multithreading.c -lunwind -ldw -lelf -lm
///
#include "../minitrace.h"
#include <pthread.h>
#include <math.h>

#define lock_stream(stream, ...)        \
        do{                             \
            flockfile((stream));        \
            __VA_ARGS__                 \
            funlockfile((stream));      \
        } while(0)

typedef struct data {
    float n1;
    float n2;
    pthread_t* thread;
} data;

static int level3(int x, int y) {
    mt_print_current_stack_trace();
    bool i = x % 2 == 0;
    return i ? x + y : x * y;
}

static int level2(int x, int y) {
    double p = pow(x, y);
    return level3(sqrt(p*x), y);
}

static int level1(int x, int y) {
    if(x % 2 == 0) {
        return level2(x*2, y*2);
    } else {
        return level2(y/2, x/2);
    }
}

void* thread_worker_1(void* arg) {
    lock_stream(stderr, {
        if(arg != NULL) {
            data* d = (data*)arg;
            fprintf(stderr, "\n======================== STACK TRACE (%lu) ", *(d->thread));
            fprintf(stderr, "========================\n");
            int result = level1(d->n1, d->n2);
            fprintf(stderr, "[THREAD(%lu)]: result %d (%.3f, %.3f)\n", *(d->thread), result, d->n1, d->n2);
        } else {
            int x = 127/3;
            int y = 5/2;
            int result = level1(x, y);
            fprintf(stderr, "[THREAD:??]: result %d (%d, %d)\n", result, x, y);    
        }
    });
    return NULL;
}

void* thread_worker_2(void* arg) {
    lock_stream(stderr, {
        if(arg != NULL) {
            data* d = (data*)arg;
            fprintf(stderr, "\n======================== STACK TRACE (%lu) ", *(d->thread));
            fprintf(stderr, "========================\n");
            int result = level1((int)(d->n1)%5, (int)((d->n2*5)/2));
            fprintf(stderr, "[THREAD:%lu]: result %d (%.3f, %.3f)\n", *(d->thread), result, d->n1, d->n2);
        } else {
            int x = 127/3;
            int y = 5/2;
            int result = level1(x, y);
            fprintf(stderr, "[THREAD:??]: result %d (%d, %d)\n", result, x, y);    
        }
    });

    return NULL;
}

int main(void) {
    pthread_t thread_1;
    pthread_t thread_2;

    data thread_1_data = { .n1 = 5.5f, .n2 = 6.9f,  .thread = &thread_1 };
    data thread_2_data = { .n1 = 6.7f, .n2 = 10.4f, .thread = &thread_2 };

    pthread_create(&thread_1, NULL, thread_worker_1, &thread_1_data);
    pthread_create(&thread_2, NULL, thread_worker_2, &thread_2_data);

    pthread_join(thread_1, NULL);
    pthread_join(thread_2, NULL);

    fprintf(stderr, "\n======================== MAIN ========================\n");
    mt_print_current_stack_trace();

    return 0;
}