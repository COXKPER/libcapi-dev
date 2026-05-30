/*
 * libcapi.c — Refactored core engine for libcapi
 *
 * Copyright (C) 2020 Neon Corporation
 * Licensed under GPL-3.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <time.h>

#include "../include/internal.h"

/* ── Global State Definitions ──────────────────────────────────────── */

CapiVar      g_vars[CAPI_MAX_VARS];
int          g_var_count = 0;

CapiFunction g_funcs[CAPI_MAX_FUNCS];
int          g_func_count = 0;
int          g_defining_function = -1;

CapiArray    g_arrays[CAPI_MAX_ARRAYS];
int          g_array_count = 0;

char         g_imports[CAPI_MAX_IMPORTS][CAPI_PATH_LEN];
int          g_import_count = 0;

CalibFile    g_calib;
int          g_calib_loaded = 0;

int          g_in_try_block = 0;
int          g_try_error = 0;

/* ── Internal Helpers ──────────────────────────────────────────────── */

/*
 * capi_trim — trim leading and trailing whitespace in-place.
 */
void capi_trim(char *str)
{
    char *start, *end;

    if (!str) return;

    /* skip leading whitespace */
    start = str;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n')
        start++;

    /* all spaces */
    if (*start == '\0') {
        str[0] = '\0';
        return;
    }

    /* find end of non-whitespace */
    end = start + strlen(start) - 1;
    while (end > start &&
           (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n'))
        end--;

    /* shift trimmed content to the front */
    {
        size_t len = (size_t)(end - start + 1);
        if (start != str)
            memmove(str, start, len);
        str[len] = '\0';
    }
}

/*
 * capi_safe_append — append src to dest using strncat, respecting maxlen.
 * maxlen is the total buffer size of dest (including '\0').
 */
void capi_safe_append(char *dest, const char *src, size_t maxlen)
{
    size_t dlen;

    if (!dest || !src || maxlen == 0)
        return;

    dlen = strlen(dest);
    if (dlen >= maxlen - 1)
        return;

    strncat(dest, src, maxlen - dlen - 1);
    dest[maxlen - 1] = '\0';
}

/*
 * capi_append_output — append data to a CapiResult's output buffer.
 */
void capi_append_output(CapiResult *res, const char *data)
{
    if (!res || !data)
        return;
    capi_safe_append(res->output, data, sizeof(res->output));
}

/*
 * capi_set_variable — store a named variable, null-terminating properly.
 */
void capi_set_variable(const char *name, const char *value)
{
    int i;

    if (!name || !value)
        return;

    /* update existing variable */
    for (i = 0; i < g_var_count; i++) {
        if (strcmp(g_vars[i].name, name) == 0) {
            strncpy(g_vars[i].value, value, CAPI_VALUE_LEN - 1);
            g_vars[i].value[CAPI_VALUE_LEN - 1] = '\0';
            return;
        }
    }

    /* create new variable */
    if (g_var_count < CAPI_MAX_VARS) {
        strncpy(g_vars[g_var_count].name, name, CAPI_NAME_LEN - 1);
        g_vars[g_var_count].name[CAPI_NAME_LEN - 1] = '\0';
        strncpy(g_vars[g_var_count].value, value, CAPI_VALUE_LEN - 1);
        g_vars[g_var_count].value[CAPI_VALUE_LEN - 1] = '\0';
        g_var_count++;
    } else {
        capi_report_warning("maximum variable count reached");
    }
}

/*
 * capi_get_variable — retrieve a variable's value by name.
 * Returns NULL if not found.
 */
const char *capi_get_variable(const char *name)
{
    int i;

    if (!name) return NULL;

    for (i = 0; i < g_var_count; i++) {
        if (strcmp(g_vars[i].name, name) == 0)
            return g_vars[i].value;
    }
    return NULL;
}

/*
 * capi_execute_shell — run a shell command via popen and capture output.
 */
void capi_execute_shell(const char *cmd, CapiResult *res)
{
    FILE *fp;
    char buf[CAPI_LINE_BUF];

    if (!cmd || !res)
        return;

    fp = popen(cmd, "r");
    if (!fp) {
        if (g_in_try_block) {
            g_try_error = 1;
            return;
        }
        capi_report_error("failed to execute shell command", 0, NULL);
        return;
    }

    while (fgets(buf, sizeof(buf), fp) != NULL) {
        capi_append_output(res, buf);
    }

    pclose(fp);
}

/*
 * capi_include_file — include and execute another .capi file.
 * Uses CAPI_PATH_LEN+64 for error message buffer to avoid truncation.
 */
void capi_include_file(const char *path, CapiResult *res)
{
    FILE *fp;
    char line_buf[CAPI_LINE_BUF];
    char errmsg[CAPI_PATH_LEN + 64];

    if (!path || !res)
        return;

    fp = fopen(path, "r");
    if (!fp) {
        snprintf(errmsg, sizeof(errmsg), "cannot include file: %s", path);
        if (g_in_try_block) {
            g_try_error = 1;
            return;
        }
        capi_report_error(errmsg, 0, NULL);
        return;
    }

    while (fgets(line_buf, sizeof(line_buf), fp) != NULL) {
        capi_process_line(line_buf, res);
        if (res->force_close)
            break;
    }

    fclose(fp);
}

/*
 * capi_load_so — load a shared library and execute its initialization function.
 */
void capi_load_so(const char *path, CapiResult *res)
{
    void *handle;
    char errmsg[CAPI_PATH_LEN + 64];

    if (!path || !res)
        return;

    handle = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        snprintf(errmsg, sizeof(errmsg), "cannot load shared library: %s", dlerror());
        if (g_in_try_block) {
            g_try_error = 1;
            return;
        }
        capi_report_error(errmsg, 0, NULL);
        return;
    }

    void (*init_func)(void) = (void (*)(void))dlsym(handle, "capi_init");
    if (init_func) {
        init_func();
    }
}

/*
 * capi_run_function — execute a stored function by name.
 */
void capi_run_function(const char *name, CapiResult *res)
{
    int i, j;

    if (!name || !res)
        return;

    for (i = 0; i < g_func_count; i++) {
        if (strcmp(g_funcs[i].name, name) == 0) {
            for (j = 0; j < g_funcs[i].line_count; j++) {
                capi_process_line(g_funcs[i].lines[j], res);
                if (res->force_close)
                    return;
            }
            return;
        }
    }

    {
        char warn_buf[CAPI_NAME_LEN + 32];
        snprintf(warn_buf, sizeof(warn_buf), "undefined function: %s", name);
        if (g_in_try_block) {
            g_try_error = 1;
            return;
        }
        capi_report_warning(warn_buf);
    }
}

/* ── State Management ──────────────────────────────────────────────── */

/*
 * capi_reset — clear all global interpreter state.
 */
void capi_reset(void)
{
    g_var_count = 0;
    g_func_count = 0;
    g_defining_function = -1;
    g_array_count = 0;
    g_import_count = 0;
    g_calib_loaded = 0;
    g_in_try_block = 0;
    g_try_error = 0;
    memset(g_vars, 0, sizeof(g_vars));
    memset(g_funcs, 0, sizeof(g_funcs));
    memset(g_arrays, 0, sizeof(g_arrays));
    memset(&g_calib, 0, sizeof(g_calib));
}

/* ── Error Reporting ───────────────────────────────────────────────── */

void capi_report_error(const char *msg, int line, const char *file)
{
    if (file && line > 0)
        fprintf(stderr, "[CAPI ERROR] %s:%d: %s\n", file, line, msg ? msg : "");
    else if (file)
        fprintf(stderr, "[CAPI ERROR] %s: %s\n", file, msg ? msg : "");
    else
        fprintf(stderr, "[CAPI ERROR] %s\n", msg ? msg : "");
}

void capi_report_warning(const char *msg)
{
    fprintf(stderr, "[CAPI WARNING] %s\n", msg ? msg : "");
}

/* ── Version / Info ────────────────────────────────────────────────── */

const char *capi_version(void)
{
    return LIBCAPI_VERSION_STRING;
}

const char *capi_info(void)
{
    static const char info[] =
        "libcapi " LIBCAPI_VERSION_STRING "\n"
        "Build: C99, POSIX, shared library\n"
        "Features: variables, functions, arrays, math, strings,\n"
        "          file I/O, shell exec, includes, calib config,\n"
        "          if/else, while loops, try/catch, timers, env vars";
    return info;
}

/* ── Main Line Dispatcher ──────────────────────────────────────────── */

void capi_process_line(const char *raw, CapiResult *res)
{
    char line[CAPI_LINE_BUF];
    char name_buf[CAPI_NAME_LEN];
    char value_buf[CAPI_VALUE_LEN];
    size_t len;
    char *p;

    if (!raw || !res)
        return;

    /* copy raw line into mutable buffer, strip newline, trim */
    strncpy(line, raw, sizeof(line) - 1);
    line[sizeof(line) - 1] = '\0';

    /* strip trailing newline / carriage return */
    len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
        line[--len] = '\0';
    }

    capi_trim(line);

    /* skip empty lines */
    if (line[0] == '\0')
        return;

    /* skip comments: lines starting with # or // */
    if (line[0] == '#')
        return;
    if (line[0] == '/' && line[1] == '/')
        return;

    /* ── Function definition mode ─────────────────────────────────── */
    if (g_defining_function >= 0) {
        /* BUG FIX: use strcmp after trimming, not strstr */
        if (strcmp(line, "funclose;") == 0) {
            g_defining_function = -1;
            return;
        }
        /* store line into current function */
        if (g_funcs[g_defining_function].line_count < CAPI_MAX_LINES) {
            snprintf(
                g_funcs[g_defining_function].lines[g_funcs[g_defining_function].line_count],
                CAPI_VALUE_LEN,
                "%s", line
            );
            g_funcs[g_defining_function].line_count++;
        } else {
            capi_report_warning("function line limit reached");
        }
        return;
    }

    /* ── Function declaration: "function name:" ───────────────────── */
    if (strncmp(line, "function ", 9) == 0) {
        p = line + 9;
        len = strlen(p);
        if (len > 0 && p[len - 1] == ':') {
            p[len - 1] = '\0'; /* remove trailing colon */
            capi_trim(p);
            if (g_func_count < CAPI_MAX_FUNCS) {
                snprintf(g_funcs[g_func_count].name, CAPI_NAME_LEN, "%s", p);
                g_funcs[g_func_count].line_count = 0;
                g_defining_function = g_func_count;
                g_func_count++;
            } else {
                capi_report_warning("maximum function count reached");
            }
            return;
        }
    }

    /* ── Feature handlers from features.c ─────────────────────────── */
    if (capi_handle_if(line, res))       return;
    if (capi_handle_while(line, res))    return;
    if (capi_handle_math(line, res))     return;
    if (capi_handle_string_op(line, res)) return;
    if (capi_handle_array(line, res))    return;

    /* ── Built-in command handlers from builtins.c ────────────────── */
    if (capi_handle_sleep(line, res))    return;
    if (capi_handle_timer(line, res))    return;
    if (capi_handle_version(line, res))  return;
    if (capi_handle_info(line, res))     return;
    if (capi_handle_env(line, res))      return;
    if (capi_handle_file_io(line, res))  return;
    if (capi_handle_try(line, res))      return;
    if (capi_handle_calib(line, res))    return;
    if (capi_handle_import(line, res))   return;
    /* ── Variable declaration: "int name= value" ──────────────────── */
    if (strncmp(line, "int ", 4) == 0) {
        memset(name_buf, 0, sizeof(name_buf));
        memset(value_buf, 0, sizeof(value_buf));
        if (sscanf(line, "int %63[^=]= %255[^\n]", name_buf, value_buf) >= 1) {
            /* BUG FIX: trim whitespace from variable name after sscanf */
            capi_trim(name_buf);
            name_buf[CAPI_NAME_LEN - 1] = '\0';
            
            capi_trim(value_buf);
            size_t vlen = strlen(value_buf);
            if (vlen >= 2 && value_buf[0] == '"' && value_buf[vlen - 1] == '"') {
                memmove(value_buf, value_buf + 1, vlen - 2);
                value_buf[vlen - 2] = '\0';
            }
            value_buf[CAPI_VALUE_LEN - 1] = '\0';
            
            capi_set_variable(name_buf, value_buf);
            return;
        }
    }

    /* ── Echo: "echo: var;" ───────────────────────────────────────── */
    if (strncmp(line, "echo:", 5) == 0) {
        char echo_arg[CAPI_VALUE_LEN];
        const char *val;

        strncpy(echo_arg, line + 5, sizeof(echo_arg) - 1);
        echo_arg[sizeof(echo_arg) - 1] = '\0';
        capi_trim(echo_arg);

        /* strip trailing semicolon */
        len = strlen(echo_arg);
        if (len > 0 && echo_arg[len - 1] == ';')
            echo_arg[len - 1] = '\0';
        capi_trim(echo_arg);

        val = capi_get_variable(echo_arg);
        if (val) {
            capi_append_output(res, val);
            capi_append_output(res, "\n");
        } else {
            /* print literal */
            capi_append_output(res, echo_arg);
            capi_append_output(res, "\n");
        }
        return;
    }

    /* ── Include: "incl <path>;" ──────────────────────────────────── */
    if (strncmp(line, "incl ", 5) == 0) {
        char inc_path[CAPI_PATH_LEN];

        strncpy(inc_path, line + 5, sizeof(inc_path) - 1);
        inc_path[sizeof(inc_path) - 1] = '\0';
        capi_trim(inc_path);

        /* strip trailing semicolon */
        len = strlen(inc_path);
        if (len > 0 && inc_path[len - 1] == ';')
            inc_path[len - 1] = '\0';

        /* parse <sys> or "pwd" */
        int use_sys = 0;
        int use_pwd = 0;
        len = strlen(inc_path);
        if (len >= 2 && inc_path[0] == '<' && inc_path[len - 1] == '>') {
            memmove(inc_path, inc_path + 1, len - 2);
            inc_path[len - 2] = '\0';
            use_sys = 1;
        } else if (len >= 2 && inc_path[0] == '"' && inc_path[len - 1] == '"') {
            memmove(inc_path, inc_path + 1, len - 2);
            inc_path[len - 2] = '\0';
            use_pwd = 1;
        }
        capi_trim(inc_path);

        char final_path[CAPI_PATH_LEN];
        if (use_sys) {
            snprintf(final_path, sizeof(final_path), "/usr/capi/libs/%s", inc_path);
        } else if (use_pwd) {
            if (inc_path[0] != '/' && strncmp(inc_path, "./", 2) != 0) {
                snprintf(final_path, sizeof(final_path), "./%s", inc_path);
            } else {
                snprintf(final_path, sizeof(final_path), "%s", inc_path);
            }
        } else {
            snprintf(final_path, sizeof(final_path), "%s", inc_path);
        }

        len = strlen(final_path);
        if (len >= 3 && strcmp(final_path + len - 3, ".so") == 0) {
            capi_load_so(final_path, res);
        } else {
            capi_include_file(final_path, res);
        }
        return;
    }

    /* ── Shell exec: capi.exec: "cmd"; ────────────────────────────── */
    if (strncmp(line, "capi.exec:", 10) == 0) {
        char cmd_buf[CAPI_VALUE_LEN];

        strncpy(cmd_buf, line + 10, sizeof(cmd_buf) - 1);
        cmd_buf[sizeof(cmd_buf) - 1] = '\0';
        capi_trim(cmd_buf);

        /* strip trailing semicolon */
        len = strlen(cmd_buf);
        if (len > 0 && cmd_buf[len - 1] == ';')
            cmd_buf[--len] = '\0';
        capi_trim(cmd_buf);

        /* strip surrounding quotes */
        if (len >= 2 && cmd_buf[0] == '"' && cmd_buf[len - 1] == '"') {
            memmove(cmd_buf, cmd_buf + 1, len - 2);
            cmd_buf[len - 2] = '\0';
        }

        capi_execute_shell(cmd_buf, res);
        return;
    }

    /* ── Force close (deprecated) ─────────────────────────────────── */
    if (strcmp(line, "capi.forceclose()") == 0 ||
        strcmp(line, "capi.forceclose();") == 0) {
        capi_report_warning("capi.forceclose() is deprecated");
        res->force_close = 1;
        return;
    }

    len = strlen(line);
    if (len >= 3 && line[len - 1] == ')' && line[len - 2] == '(') {
        /* strip optional trailing semicolon before () */
        char fn_name[CAPI_NAME_LEN];
        size_t name_len = len - 2;

        if (name_len >= sizeof(fn_name))
            name_len = sizeof(fn_name) - 1;

        memcpy(fn_name, line, name_len);
        fn_name[name_len] = '\0';
        capi_trim(fn_name);
        capi_run_function(fn_name, res);
        return;
    }

    /* check for name(); with trailing semicolon */
    if (len >= 4 && line[len - 1] == ';' && line[len - 2] == ')' && line[len - 3] == '(') {
        char fn_name[CAPI_NAME_LEN];
        size_t name_len = len - 3;

        if (name_len >= sizeof(fn_name))
            name_len = sizeof(fn_name) - 1;

        memcpy(fn_name, line, name_len);
        fn_name[name_len] = '\0';
        capi_trim(fn_name);
        capi_run_function(fn_name, res);
        return;
    }

    /* ── Fallback: unknown syntax ─────────────────────────────────── */
    {
        char warn_buf[CAPI_LINE_BUF + 32];
        snprintf(warn_buf, sizeof(warn_buf), "unknown syntax: %.500s", line);
        capi_report_warning(warn_buf);
    }
}

/* ── File Processor ────────────────────────────────────────────────── */

CapiResult capi_process_file(const char *filepath)
{
    CapiResult res;
    FILE *fp;
    char line_buf[CAPI_LINE_BUF];

    memset(&res, 0, sizeof(res));

    if (!filepath) {
        res.error_code = 1;
        strncpy(res.error_msg, "filepath is NULL", sizeof(res.error_msg) - 1);
        res.error_msg[sizeof(res.error_msg) - 1] = '\0';
        return res;
    }

    capi_reset();

    fp = fopen(filepath, "r");
    if (!fp) {
        res.error_code = 1;
        snprintf(res.error_msg, sizeof(res.error_msg), "cannot open file: %s", filepath);
        res.error_msg[sizeof(res.error_msg) - 1] = '\0';
        return res;
    }

    while (fgets(line_buf, sizeof(line_buf), fp) != NULL) {
        capi_process_line(line_buf, &res);
        if (res.force_close)
            break;
    }

    fclose(fp);
    return res;
}
