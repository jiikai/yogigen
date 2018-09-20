#include "yogigen.h"
#include "yogiserver.h"
#include "minunit.h"
#include "dbg.h"

#define TEST_COUNT 7

// UNIT TESTS

char *test_server_init()
{
    YogiServer *server = YogiServer_init();
    mu_assert(server, "Server failed to initialize.");
    muWrapper *muwr = container_of(test_server_init, muWrapper, test_function);
    muwr->ptr = server;
    return NULL;
}

char *test_server_gen_request()
{
    muWrapper *muwr = container_of(test_server_init, muWrapper, test_function);
    return NULL;
}

char *test_server_permalink_request()
{
    return NULL;
}

char *test_server_getbyid_request()
{
    return NULL;
}

char *test_server_exit_request()
{
    return NULL;
}

char *test_server_clean_exit_on_sigterm()
{
    return NULL;
}

muWrapper *muWrapper_init(int test_count) {
    muWrapper *muwr = malloc(sizeof(muWrapper));
    check_mem(muwr);
    return muwr;
error:
    return NULL;
}

char *all_tests()
{
    mu_suite_start();
    muWrapper *muwr = muWrapper_init(7);
    mu_test_function mu_tests[] = {
        test_server_init, test_server_gen_request,
        test_server_permalink_request, test_server_getbyid_request, test_server_exit_request, test_server_clean_exit_on_sigterm
    };
    int i;
    do {
        muwr->test_function = mu_tests[i];
        mu_run_test(mu_tests[i]);
    } while (++i < TEST_COUNT);

    return NULL;
}

RUN_TESTS(all_tests);
