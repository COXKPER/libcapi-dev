/*
 * internal.h — Shared internal declarations for libcapi
 *
 * NOT part of the public API.  Do not install this header.
 *
 * Copyright (C) 2020 Neon Corporation
 * Licensed under GPL-3.0
 */

#ifndef LIBCAPI_INTERNAL_H
#define LIBCAPI_INTERNAL_H

#include "libcapi.h"
#include "calib.h"
#include <stddef.h>

/* ── Variable Storage ───────────────────────────────────────────────── */
typedef struct {
    char name[CAPI_NAME_LEN];
    char value[CAPI_VALUE_LEN];
} CapiVar;

/* ── Array Storage ──────────────────────────────────────────────────── */
typedef struct {
    char name[CAPI_NAME_LEN];
    char items[CAPI_MAX_ARRAY_ITEMS][CAPI_VALUE_LEN];
    int  count;
} CapiArray;

/* ── Function Storage ───────────────────────────────────────────────── */
typedef struct {
    char name[CAPI_NAME_LEN];
    char lines[CAPI_MAX_LINES][CAPI_VALUE_LEN];
    int  line_count;
} CapiFunction;

/* ── Global Interpreter State ───────────────────────────────────────── */
extern CapiVar      g_vars[];
extern int          g_var_count;

extern CapiFunction g_funcs[];
extern int          g_func_count;
extern int          g_defining_function;

extern CapiArray    g_arrays[];
extern int          g_array_count;

extern char         g_imports[CAPI_MAX_IMPORTS][CAPI_PATH_LEN];
extern int          g_import_count;

extern CalibFile    g_calib;
extern int          g_calib_loaded;

/* Try/catch state */
extern int          g_in_try_block;
extern int          g_try_error;

/* ── Internal Helpers (libcapi.c) ───────────────────────────────────── */
void        capi_safe_append(char *dest, const char *src, size_t maxlen);
void        capi_append_output(CapiResult *res, const char *data);
void        capi_set_variable(const char *name, const char *value);
const char *capi_get_variable(const char *name);
void        capi_trim(char *str);
void        capi_execute_shell(const char *cmd, CapiResult *res);
void        capi_include_file(const char *path, CapiResult *res);
void        capi_load_so(const char *path, CapiResult *res);
void        capi_run_function(const char *name, CapiResult *res);

/* ── Feature Dispatchers (features.c) ───────────────────────────────── */
int  capi_handle_if(const char *line, CapiResult *res);
int  capi_handle_while(const char *line, CapiResult *res);
int  capi_handle_math(const char *line, CapiResult *res);
int  capi_handle_string_op(const char *line, CapiResult *res);
int  capi_handle_array(const char *line, CapiResult *res);

/* ── Built-in Commands (builtins.c) ─────────────────────────────────── */
int  capi_handle_sleep(const char *line, CapiResult *res);
int  capi_handle_timer(const char *line, CapiResult *res);
int  capi_handle_version(const char *line, CapiResult *res);
int  capi_handle_info(const char *line, CapiResult *res);
int  capi_handle_env(const char *line, CapiResult *res);
int  capi_handle_file_io(const char *line, CapiResult *res);
int  capi_handle_try(const char *line, CapiResult *res);
int  capi_handle_calib(const char *line, CapiResult *res);
int  capi_handle_import(const char *line, CapiResult *res);

#endif /* LIBCAPI_INTERNAL_H */
