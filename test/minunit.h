#undef NDEBUG
#ifndef _minunit_h
#define _minunit_h

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
//#include <stddef.h>
#include "dbg.h"

/* BEGIN MOD */

typedef char* (*mu_test_function) ();

typedef struct mu_wrapper {
    void *ptr;
    clock_t time_a;
    clock_t time_b;
    mu_test_function test_function;
} muWrapper;

muWrapper *muWrapper_init(int test_count);

// Linux kernel container_of to get the at the wrapper struct from the enclosed functions

#define container_of(ptr, type, member) (type*)((char*)(ptr) - offsetof(type, member))
/* END MOD */

#define mu_suite_start() char *message = NULL

#define mu_assert(test, message) if (!(test)) {\
    log_err(message); return message; }
#define mu_run_test(test) debug("\n-----%s", " " #test); \
    message = test(); tests_run++; if (message) return message;

#define RUN_TESTS(name) int main(int argc, char *argv[]) {\
    argc = 1; \
    debug("----- RUNNING: %s", argv[0]);\
    printf("----\nRUNNING: %s\n", argv[0]);\
    char *result = name();\
    if (result != 0) {\
        printf("FAILED: %s\n", result);\
    }\
    else {\
        printf("ALL TESTS PASSED\n");\
    }\
    printf("Tests run: %d\n", tests_run);\
    exit(result != 0);\
}

int tests_run;

#endif
