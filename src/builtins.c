/*
 * builtins.c — Built-in command handlers for libcapi
 *
 * Part of libcapi — Create Application Programmable Interface
 * Copyright (C) 2020 Neon Corporation
 * Licensed under GPL-3.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "../include/internal.h"

/* ── capi_handle_sleep ─────────────────────────────────────────────── */

int capi_handle_sleep(const char *line, CapiResult *res)
{
    (void)res;

    if (strncmp(line, "capi.sleep ", 11) != 0)
        return 0;

    char buf[CAPI_VALUE_LEN];
    strncpy(buf, line + 11, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    /* Strip trailing semicolon */
    size_t blen = strlen(buf);
    if (blen > 0 && buf[blen - 1] == ';')
        buf[blen - 1] = '\0';

    long ms = strtol(buf, NULL, 10);
    if (ms > 0)
        usleep((useconds_t)(ms * 1000));

    return 1;
}

/* ── capi_handle_timer ─────────────────────────────────────────────── */

static struct timespec timer_start_ts;

int capi_handle_timer(const char *line, CapiResult *res)
{
    if (strncmp(line, "capi.timer ", 11) != 0)
        return 0;

    const char *arg = line + 11;

    /* Strip trailing semicolon for comparison */
    char cmd[CAPI_VALUE_LEN];
    strncpy(cmd, arg, sizeof(cmd) - 1);
    cmd[sizeof(cmd) - 1] = '\0';
    size_t clen = strlen(cmd);
    if (clen > 0 && cmd[clen - 1] == ';')
        cmd[clen - 1] = '\0';

    if (strcmp(cmd, "start") == 0) {
        clock_gettime(CLOCK_MONOTONIC, &timer_start_ts);
        return 1;
    }

    if (strcmp(cmd, "stop") == 0) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);

        long elapsed_ms = (now.tv_sec - timer_start_ts.tv_sec) * 1000L +
                          (now.tv_nsec - timer_start_ts.tv_nsec) / 1000000L;

        char out[CAPI_VALUE_LEN];
        snprintf(out, sizeof(out), "%ld ms\n", elapsed_ms);
        capi_append_output(res, out);
        return 1;
    }

    return 0;
}

/* ── capi_handle_version ───────────────────────────────────────────── */

int capi_handle_version(const char *line, CapiResult *res)
{
    if (strcmp(line, "capi.version;") != 0)
        return 0;

    capi_append_output(res, "libcapi ");
    capi_append_output(res, LIBCAPI_VERSION_STRING);
    capi_append_output(res, "\n");
    return 1;
}

/* ── capi_handle_info ──────────────────────────────────────────────── */

int capi_handle_info(const char *line, CapiResult *res)
{
    if (strcmp(line, "capi.info;") != 0)
        return 0;

    char info[CAPI_MAX_OUTPUT];
    snprintf(info, sizeof(info),
             "libcapi %s\n"
             "features: calib, conditionals, loops, math, strings, "
             "arrays, timers, env, file-io, try-catch\n",
             LIBCAPI_VERSION_STRING);

    capi_append_output(res, info);
    return 1;
}

/* ── capi_handle_env ───────────────────────────────────────────────── */

int capi_handle_env(const char *line, CapiResult *res)
{
    if (strncmp(line, "env.", 4) != 0)
        return 0;

    /* ---- env.get VARNAME; ---- */
    if (strncmp(line, "env.get ", 8) == 0) {
        char name[CAPI_NAME_LEN];
        strncpy(name, line + 8, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
        size_t nlen = strlen(name);
        if (nlen > 0 && name[nlen - 1] == ';')
            name[nlen - 1] = '\0';

        const char *val = getenv(name);
        if (val)
            capi_append_output(res, val);
        else
            capi_append_output(res, "[undefined]");
        capi_append_output(res, "\n");
        return 1;
    }

    /* ---- env.set VARNAME value; ---- */
    if (strncmp(line, "env.set ", 8) == 0) {
        char buf[CAPI_LINE_BUF];
        strncpy(buf, line + 8, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        size_t blen = strlen(buf);
        if (blen > 0 && buf[blen - 1] == ';')
            buf[blen - 1] = '\0';

        char *saveptr = NULL;
        char *name  = strtok_r(buf, " ", &saveptr);
        char *value = saveptr;

        if (name == NULL || value == NULL) {
            capi_append_output(res, "[error] env.set: invalid syntax\n");
            return 1;
        }

        /* Trim leading whitespace from value */
        while (*value == ' ')
            value++;

        setenv(name, value, 1);
        return 1;
    }

    return 0;
}

/* ── capi_handle_file_io ───────────────────────────────────────────── */

int capi_handle_file_io(const char *line, CapiResult *res)
{
    if (strncmp(line, "file.", 5) != 0)
        return 0;

    /* ---- file.read "path" into var; ---- */
    if (strncmp(line, "file.read ", 10) == 0) {
        char buf[CAPI_LINE_BUF];
        strncpy(buf, line + 10, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        size_t blen = strlen(buf);
        if (blen > 0 && buf[blen - 1] == ';')
            buf[blen - 1] = '\0';

        /* Parse: "path" into var */
        char path[CAPI_PATH_LEN] = {0};
        char var[CAPI_NAME_LEN]  = {0};

        const char *p = buf;
        while (*p == ' ') p++;
        if (*p == '"') {
            p++;
            const char *start = p;
            while (*p && *p != '"') p++;
            size_t plen = (size_t)(p - start);
            if (plen >= sizeof(path)) plen = sizeof(path) - 1;
            memcpy(path, start, plen);
            path[plen] = '\0';
            if (*p == '"') p++;
        }

        /* Skip " into " */
        while (*p == ' ') p++;
        if (strncmp(p, "into ", 5) == 0)
            p += 5;
        while (*p == ' ') p++;

        snprintf(var, sizeof(var), "%s", p);
        /* Trim trailing whitespace */
        size_t vlen = strlen(var);
        while (vlen > 0 && var[vlen - 1] == ' ')
            var[--vlen] = '\0';

        FILE *fp = fopen(path, "r");
        if (fp == NULL) {
            capi_append_output(res, "[error] file.read: cannot open file\n");
            if (g_in_try_block) g_try_error = 1;
            return 1;
        }

        char content[CAPI_VALUE_LEN];
        size_t nread = fread(content, 1, sizeof(content) - 1, fp);
        content[nread] = '\0';
        fclose(fp);

        capi_set_variable(var, content);
        return 1;
    }

    /* ---- file.write "path" var; ---- */
    if (strncmp(line, "file.write ", 11) == 0) {
        char buf[CAPI_LINE_BUF];
        strncpy(buf, line + 11, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        size_t blen = strlen(buf);
        if (blen > 0 && buf[blen - 1] == ';')
            buf[blen - 1] = '\0';

        char path[CAPI_PATH_LEN] = {0};
        const char *p = buf;
        while (*p == ' ') p++;
        if (*p == '"') {
            p++;
            const char *start = p;
            while (*p && *p != '"') p++;
            size_t plen = (size_t)(p - start);
            if (plen >= sizeof(path)) plen = sizeof(path) - 1;
            memcpy(path, start, plen);
            path[plen] = '\0';
            if (*p == '"') p++;
        }

        while (*p == ' ') p++;
        char var[CAPI_NAME_LEN];
        snprintf(var, sizeof(var), "%s", p);
        size_t vlen = strlen(var);
        while (vlen > 0 && var[vlen - 1] == ' ')
            var[--vlen] = '\0';

        const char *val = capi_get_variable(var);
        if (val == NULL)
            val = "";

        FILE *fp = fopen(path, "w");
        if (fp == NULL) {
            capi_append_output(res, "[error] file.write: cannot open file\n");
            if (g_in_try_block) g_try_error = 1;
            return 1;
        }
        fputs(val, fp);
        fclose(fp);
        return 1;
    }

    /* ---- file.append "path" "text"; ---- */
    if (strncmp(line, "file.append ", 12) == 0) {
        char buf[CAPI_LINE_BUF];
        strncpy(buf, line + 12, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        size_t blen = strlen(buf);
        if (blen > 0 && buf[blen - 1] == ';')
            buf[blen - 1] = '\0';

        /* Parse first quoted string (path) */
        char path[CAPI_PATH_LEN] = {0};
        const char *p = buf;
        while (*p == ' ') p++;
        if (*p == '"') {
            p++;
            const char *start = p;
            while (*p && *p != '"') p++;
            size_t plen = (size_t)(p - start);
            if (plen >= sizeof(path)) plen = sizeof(path) - 1;
            memcpy(path, start, plen);
            path[plen] = '\0';
            if (*p == '"') p++;
        }

        while (*p == ' ') p++;

        /* Second argument: quoted text or variable name */
        char text[CAPI_VALUE_LEN] = {0};
        size_t tlen = strlen(p);
        if (tlen >= 2 && p[0] == '"' && p[tlen - 1] == '"') {
            /* Quoted literal text */
            if (tlen - 2 >= sizeof(text)) tlen = sizeof(text) + 1;
            memcpy(text, p + 1, tlen - 2);
            text[tlen - 2] = '\0';
        } else {
            /* Variable name — resolve it */
            char vname[CAPI_NAME_LEN];
            snprintf(vname, sizeof(vname), "%s", p);
            size_t vnlen = strlen(vname);
            while (vnlen > 0 && vname[vnlen - 1] == ' ')
                vname[--vnlen] = '\0';

            const char *val = capi_get_variable(vname);
            if (val) {
                strncpy(text, val, sizeof(text) - 1);
                text[sizeof(text) - 1] = '\0';
            }
        }

        FILE *fp = fopen(path, "a");
        if (fp == NULL) {
            capi_append_output(res, "[error] file.append: cannot open file\n");
            if (g_in_try_block) g_try_error = 1;
            return 1;
        }
        fputs(text, fp);
        fclose(fp);
        return 1;
    }

    return 0;
}

/* ── capi_handle_try ───────────────────────────────────────────────── */

static int skip_catch_body = 0;   /* 1 = skip lines until endtry */

int capi_handle_try(const char *line, CapiResult *res)
{
    (void)res;

    if (strcmp(line, "try:") == 0) {
        g_in_try_block = 1;
        g_try_error = 0;
        skip_catch_body = 0;
        return 1;
    }

    if (strcmp(line, "catch:") == 0) {
        if (g_try_error == 0) {
            /* No error occurred — skip the catch body */
            skip_catch_body = 1;
        } else {
            /* Error occurred — execute the catch body */
            skip_catch_body = 0;
        }
        return 1;
    }

    if (strcmp(line, "endtry;") == 0) {
        g_in_try_block = 0;
        g_try_error = 0;
        skip_catch_body = 0;
        return 1;
    }

    /* If we are inside a catch body that should be skipped, consume lines */
    if (skip_catch_body)
        return 1;

    return 0;
}

/* ── capi_handle_calib ─────────────────────────────────────────────── */

int capi_handle_calib(const char *line, CapiResult *res)
{
    if (strncmp(line, "calib.", 6) != 0)
        return 0;

    /* ---- calib.load "path"; ---- */
    if (strncmp(line, "calib.load ", 11) == 0) {
        char buf[CAPI_PATH_LEN];
        strncpy(buf, line + 11, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        size_t blen = strlen(buf);
        if (blen > 0 && buf[blen - 1] == ';')
            buf[blen - 1] = '\0';

        /* Strip quotes */
        char *path = buf;
        size_t plen = strlen(path);
        if (plen >= 2 && path[0] == '"' && path[plen - 1] == '"') {
            path[plen - 1] = '\0';
            path++;
        }

        if (calib_load(&g_calib, path) == 0) {
            g_calib_loaded = 1;
        } else {
            capi_append_output(res, "[error] calib.load: failed to load file\n");
            if (g_in_try_block) g_try_error = 1;
        }
        return 1;
    }

    /* ---- calib.get section.key; ---- */
    if (strncmp(line, "calib.get ", 10) == 0) {
        char buf[CAPI_VALUE_LEN];
        strncpy(buf, line + 10, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        size_t blen = strlen(buf);
        if (blen > 0 && buf[blen - 1] == ';')
            buf[blen - 1] = '\0';

        if (!g_calib_loaded) {
            capi_append_output(res, "[error] calib.get: no calib file loaded\n");
            return 1;
        }

        /* Split on first '.' into section and key */
        char *dot = strchr(buf, '.');
        if (dot == NULL) {
            capi_append_output(res, "[error] calib.get: expected section.key\n");
            return 1;
        }
        *dot = '\0';
        const char *section = buf;
        const char *key = dot + 1;

        const char *val = calib_get_string(&g_calib, section, key);
        capi_append_output(res, val);
        capi_append_output(res, "\n");
        return 1;
    }

    return 0;
}

/* ── Import feature ────────────────────────────────────────────────── */

int capi_handle_import(const char *line, CapiResult *res)
{
    /* ---- import "path"; ---- */
    if (strncmp(line, "import ", 7) == 0) {
        char buf[CAPI_LINE_BUF];
        snprintf(buf, sizeof(buf), "%s", line + 7);
        
        size_t blen = strlen(buf);
        if (blen > 0 && buf[blen - 1] == ';')
            buf[blen - 1] = '\0';

        char *path = buf;
        while (*path == ' ') path++;
        capi_trim(path);

        int use_sys = 0;
        int use_pwd = 0;
        size_t plen = strlen(path);
        if (plen >= 2 && path[0] == '<' && path[plen - 1] == '>') {
            path[plen - 1] = '\0';
            path++;
            use_sys = 1;
        } else if (plen >= 2 && path[0] == '"' && path[plen - 1] == '"') {
            path[plen - 1] = '\0';
            path++;
            use_pwd = 1;
        }
        capi_trim(path);

        char final_path[CAPI_PATH_LEN];
        if (use_sys) {
            snprintf(final_path, sizeof(final_path), "/usr/capi/libs/%s", path);
        } else if (use_pwd) {
            if (path[0] != '/' && strncmp(path, "./", 2) != 0) {
                snprintf(final_path, sizeof(final_path), "./%s", path);
            } else {
                snprintf(final_path, sizeof(final_path), "%s", path);
            }
        } else {
            snprintf(final_path, sizeof(final_path), "%s", path);
        }

        if (strlen(final_path) == 0) {
            capi_append_output(res, "[error] import: empty path\n");
            return 1;
        }

        /* Check if already imported */
        for (int i = 0; i < g_import_count; i++) {
            if (strcmp(g_imports[i], final_path) == 0) {
                /* already imported, silently return */
                return 1;
            }
        }

        /* Add to imports list */
        if (g_import_count < CAPI_MAX_IMPORTS) {
            snprintf(g_imports[g_import_count], CAPI_PATH_LEN, "%s", final_path);
            g_import_count++;
        } else {
            capi_append_output(res, "[error] import: max imports exceeded\n");
            return 1;
        }

        /* Include the file or load .so */
        size_t final_len = strlen(final_path);
        if (final_len >= 3 && strcmp(final_path + final_len - 3, ".so") == 0) {
            capi_load_so(final_path, res);
        } else {
            capi_include_file(final_path, res);
        }
        return 1;
    }

    return 0;
}
