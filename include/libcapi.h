/*
 * libcapi — Create Application Programmable Interface
 * Public API Header
 *
 * Copyright (C) 2020 Neon Corporation
 * Licensed under GPL-3.0
 */

#ifndef LIBCAPI_H
#define LIBCAPI_H

#include <stddef.h>

/* ── Version ────────────────────────────────────────────────────────── */
#define LIBCAPI_VERSION_MAJOR 2
#define LIBCAPI_VERSION_MINOR 1
#define LIBCAPI_VERSION_PATCH 1
#define LIBCAPI_VERSION_STRING "2.1.1"

/* ── Limits ─────────────────────────────────────────────────────────── */
#define CAPI_MAX_OUTPUT    16384
#define CAPI_MAX_VARS      256
#define CAPI_MAX_FUNCS     128
#define CAPI_MAX_LINES     1024
#define CAPI_MAX_ARRAYS    64
#define CAPI_MAX_IMPORTS   64
#define CAPI_MAX_ARRAY_ITEMS 256
#define CAPI_LINE_BUF      512
#define CAPI_NAME_LEN      64
#define CAPI_VALUE_LEN     256
#define CAPI_PATH_LEN      512

/* ── Result Structure ───────────────────────────────────────────────── */
typedef struct {
    char   output[CAPI_MAX_OUTPUT];
    int    force_close;
    int    error_code;           /* 0 = success, nonzero = error */
    char   error_msg[256];       /* human-readable error description */
} CapiResult;

/* ── Core API ───────────────────────────────────────────────────────── */

/*
 * Process a .capi script file.
 * Returns a CapiResult with accumulated output.
 */
CapiResult capi_process_file(const char *filepath);

/*
 * Process a single line of capi script.
 * Appends output to `res`.
 */
void capi_process_line(const char *line, CapiResult *res);

/*
 * Reset all interpreter state (variables, functions, arrays, etc.).
 * Called automatically at the start of capi_process_file().
 */
void capi_reset(void);

/*
 * Return the library version string.
 */
const char *capi_version(void);

/*
 * Return build/feature information string.
 */
const char *capi_info(void);

/* ── Error Reporting ────────────────────────────────────────────────── */
void capi_report_error(const char *msg, int line, const char *file);
void capi_report_warning(const char *msg);

/* ── Backward Compatibility (deprecated names) ──────────────────────── */
#define process_capi_file  capi_process_file
#define process_line       capi_process_line
#define report_error       capi_report_error
#define report_warning     capi_report_warning

#endif /* LIBCAPI_H */
