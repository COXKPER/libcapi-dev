/*
 * test_main.c — Test runner for libcapi
 *
 * Simple test framework with pass/fail counting.
 * Compile and run via: make test
 *
 * Copyright (C) 2020 Neon Corporation
 * Licensed under GPL-3.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../include/libcapi.h"
#include "../include/calib.h"

/* ── Simple Test Framework ──────────────────────────────────────────── */

static int g_tests_passed = 0;
static int g_tests_failed = 0;
static int g_tests_total  = 0;

#define TEST(name)                                                      \
    do {                                                                \
        g_tests_total++;                                                \
        printf("  [TEST] %-30s ", #name);                               \
        if (run_##name()) {                                             \
            g_tests_passed++;                                           \
            printf("PASS\n");                                           \
        } else {                                                        \
            g_tests_failed++;                                           \
            printf("FAIL\n");                                           \
        }                                                               \
    } while (0)

/* ── Test Cases ─────────────────────────────────────────────────────── */

static int run_test_version(void)
{
    const char *ver = capi_version();
    if (!ver || strlen(ver) == 0)
        return 0;
    /* Should contain the version string */
    if (strstr(ver, "2.1.1") == NULL)
        return 0;
    return 1;
}

static int run_test_reset(void)
{
    /* Set up some state, then reset */
    CapiResult res;
    memset(&res, 0, sizeof(res));
    capi_process_line("int resetvar= \"hello\"", &res);

    /* Reset should clear everything */
    capi_reset();

    /* Process echo — variable should not exist after reset */
    memset(&res, 0, sizeof(res));
    capi_process_line("echo: resetvar;", &res);

    /* After reset the variable is gone, so output should NOT contain "hello" */
    if (strstr(res.output, "hello") != NULL)
        return 0;
    return 1;
}

static int run_test_variable(void)
{
    CapiResult res;
    memset(&res, 0, sizeof(res));
    capi_reset();

    capi_process_line("int myvar= \"test_value_42\"", &res);
    capi_process_line("echo: myvar;", &res);

    if (strstr(res.output, "test_value_42") == NULL)
        return 0;
    return 1;
}

static int run_test_echo_literal(void)
{
    CapiResult res;
    memset(&res, 0, sizeof(res));
    capi_reset();

    capi_process_line("echo: Hello from echo!;", &res);

    if (strstr(res.output, "Hello from echo!") == NULL)
        return 0;
    return 1;
}

static int run_test_function(void)
{
    CapiResult res;
    memset(&res, 0, sizeof(res));
    capi_reset();

    capi_process_line("function testfunc:", &res);
    capi_process_line("    echo: inside function;", &res);
    capi_process_line("funclose;", &res);
    capi_process_line("testfunc();", &res);

    if (strstr(res.output, "inside function") == NULL)
        return 0;
    return 1;
}

static int run_test_math(void)
{
    CapiResult res;
    memset(&res, 0, sizeof(res));
    capi_reset();

    capi_process_line("int a= \"10\"", &res);
    capi_process_line("int b= \"20\"", &res);
    capi_process_line("math c = a + b;", &res);
    capi_process_line("echo: c;", &res);

    if (strstr(res.output, "30") == NULL)
        return 0;
    return 1;
}

static int run_test_process_file(void)
{
    capi_reset();
    CapiResult res = capi_process_file("test.capi");

    if (res.error_code != 0)
        return 0;

    /* The test.capi script should produce these outputs */
    if (strstr(res.output, "Hello World") == NULL)
        return 0;
    if (strstr(res.output, "15") == NULL)
        return 0;
    if (strstr(res.output, "Function called!") == NULL)
        return 0;
    return 1;
}

static int run_test_calib(void)
{
    CalibFile cf;
    memset(&cf, 0, sizeof(cf));

    int rc = calib_load(&cf, "test.calib");
    if (rc != 0)
        return 0;

    /* Check integer values */
    int max_threads = calib_get_int(&cf, "settings", "max_threads", -1);
    if (max_threads != 4)
        return 0;

    int timeout = calib_get_int(&cf, "settings", "timeout", -1);
    if (timeout != 30)
        return 0;

    /* Check boolean */
    int debug = calib_get_bool(&cf, "settings", "debug", 0);
    if (debug != 1)
        return 0;

    /* Check string values */
    const char *data_dir = calib_get_string(&cf, "paths", "data_dir");
    if (strstr(data_dir, "/opt/data") == NULL)
        return 0;

    /* Check limits section */
    int max_conn = calib_get_int(&cf, "limits", "max_connections", -1);
    if (max_conn != 100)
        return 0;

    int buf_size = calib_get_int(&cf, "limits", "buffer_size", -1);
    if (buf_size != 4096)
        return 0;

    calib_free(&cf);
    return 1;
}

static int run_test_conditionals(void)
{
    CapiResult res;
    memset(&res, 0, sizeof(res));
    capi_reset();

    capi_process_line("int flag= \"yes\"", &res);
    capi_process_line("if flag == \"yes\":", &res);
    capi_process_line("    echo: condition met;", &res);
    capi_process_line("else:", &res);
    capi_process_line("    echo: condition not met;", &res);
    capi_process_line("endif;", &res);

    if (strstr(res.output, "condition met") == NULL)
        return 0;
    /* Should NOT contain the else branch */
    if (strstr(res.output, "condition not met") != NULL)
        return 0;
    return 1;
}

static int run_test_string_ops(void)
{
    CapiResult res;
    memset(&res, 0, sizeof(res));
    capi_reset();

    capi_process_line("int word= \"hello\"", &res);

    /* Test str.upper */
    capi_process_line("str.upper word;", &res);
    capi_process_line("echo: word;", &res);

    if (strstr(res.output, "HELLO") == NULL)
        return 0;

    /* Test str.lower */
    capi_process_line("str.lower word;", &res);
    memset(&res, 0, sizeof(res));  /* clear for clean check */
    capi_process_line("echo: word;", &res);

    if (strstr(res.output, "hello") == NULL)
        return 0;

    return 1;
}

static int run_test_import(void)
{
    capi_reset();
    /* We expect import_lib.capi to be included exactly once, so "Library loaded!" should appear exactly once */
    CapiResult res = capi_process_file("test_import.capi");
    
    if (res.error_code != 0)
        return 0;

    /* Check if lib_loaded variable was successfully set by the import */
    if (strstr(res.output, "yes") == NULL)
        return 0;

    /* Check that "Library loaded!" appears */
    char *first = strstr(res.output, "Library loaded!");
    if (first == NULL)
        return 0;

    /* Check that "Library loaded!" does NOT appear a second time */
    if (strstr(first + 1, "Library loaded!") != NULL)
        return 0;

    return 1;
}

/* ── Main ───────────────────────────────────────────────────────────── */

int main(void)
{
    printf("\n========================================\n");
    printf("  libcapi Test Suite v%s\n", LIBCAPI_VERSION_STRING);
    printf("========================================\n\n");

    TEST(test_version);
    TEST(test_reset);
    TEST(test_variable);
    TEST(test_echo_literal);
    TEST(test_function);
    TEST(test_math);
    TEST(test_process_file);
    TEST(test_calib);
    TEST(test_conditionals);
    TEST(test_string_ops);
    TEST(test_import);

    printf("\n========================================\n");
    printf("  Results: %d/%d tests passed\n", g_tests_passed, g_tests_total);
    if (g_tests_failed > 0)
        printf("  ** %d test(s) FAILED **\n", g_tests_failed);
    else
        printf("  All tests passed!\n");
    printf("========================================\n\n");

    return (g_tests_failed > 0) ? 1 : 0;
}
