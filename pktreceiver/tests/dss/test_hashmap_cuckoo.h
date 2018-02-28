#ifndef _TEST_HASHMAP_CUCKOO_
#define _TEST_HASHMAP_CUCKOO_

#include "../../src/dss/hashmap_cuckoo.h"
#include <check.h>

START_TEST(test_hashmap_cuckoo_read_after_write) {
    HashMapCuckooPtr hml = hashmap_cuckoo_create(4, 1, 1, 0);
    uint32_t key = 12;
    uint32_t *value = 0;

    value = (uint32_t*)hashmap_cuckoo_get_copy_key(hml, &key);
    *value = 0xdeadbeef;

    value = (uint32_t*)hashmap_cuckoo_get_copy_key(hml, &key);
    ck_assert_uint_eq(*value, 0xdeadbeef);

    hashmap_cuckoo_delete(hml);
}
END_TEST

START_TEST(test_hashmap_cuckoo_can_get_50p_loaded) {
    unsigned size = 1024;
    HashMapCuckooPtr hml = hashmap_cuckoo_create(size, 1, 1, 0);

    unsigned i = 0;
    uint32_t *value;

    for (i = 0; i < size; ++i) {
        value = (uint32_t*)hashmap_cuckoo_get_copy_key(hml, &i);
        *value = i * 0xbeaf;
    }

    for (i = 0; i < size; ++i) {
        value = (uint32_t*)hashmap_cuckoo_get_copy_key(hml, &i);
        ck_assert_uint_eq(*value, i * 0xbeaf);
    }

    hashmap_cuckoo_delete(hml);
}
END_TEST

START_TEST(test_hashmap_cuckoo_reset) {
    unsigned size = 1024;
    HashMapCuckooPtr hml = hashmap_cuckoo_create(size, 1, 1, 0);

    unsigned i = 0;
    uint32_t *value;

    for (i = 0; i < size; ++i) {
        value = (uint32_t*)hashmap_cuckoo_get_copy_key(hml, &i);
        *value = i * 0xbeaf;
    }

    for (i = 0; i < size; ++i) {
        value = (uint32_t*)hashmap_cuckoo_get_copy_key(hml, &i);
        ck_assert_uint_eq(*value, i * 0xbeaf);
    }

    hashmap_cuckoo_reset(hml);

    for (i = 0; i < size; ++i) {
        value = (uint32_t*)hashmap_cuckoo_get_copy_key(hml, &i);
        *value = i * 0xdead;
    }

    for (i = 0; i < size; ++i) {
        value = (uint32_t*)hashmap_cuckoo_get_copy_key(hml, &i);
        ck_assert_uint_eq(*value, i * 0xdead);
    }

    hashmap_cuckoo_delete(hml);
}
END_TEST

static int test_hashmap_cuckoo_suite(void) {
    Suite *s;
    SRunner *sr;
    TCase* tc_raw;
    TCase *tc_full;
    TCase *tc_reset;

    tc_raw = tcase_create("ReadAfterWrite");
    tcase_add_test(tc_raw, test_hashmap_cuckoo_read_after_write);

    tc_full = tcase_create("CanGet50PercentLoaded");
    tcase_add_test(tc_full, test_hashmap_cuckoo_can_get_50p_loaded);

    tc_reset = tcase_create("Reset");
    tcase_add_test(tc_reset, test_hashmap_cuckoo_reset);

    s = suite_create("Hashmap::Cuckoo");
    suite_add_tcase(s, tc_raw);
    suite_add_tcase(s, tc_full);
    suite_add_tcase(s, tc_reset);

    sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    unsigned number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return number_failed;
}

#endif
