#include <stdlib.h>
#include <check.h>

#include "dss/test_hashmap_cuckoo_bucket.h"
#include "dss/test_hashmap_cuckoo.h"
#include "dss/test_hashmap_linear.h"
#include "test.h"

int run_tests(void) {
    unsigned nfailed = 0;

    nfailed += test_hashmap_cuckoo_bucket_suite();
    nfailed += test_hashmap_cuckoo_suite();
    nfailed += test_hashmap_linear_suite();

    return (nfailed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
