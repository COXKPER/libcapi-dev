/*
 * features.c — Conditional, loop, math, string, and array handlers
 *
 * Part of libcapi — Create Application Programmable Interface
 * Copyright (C) 2020 Neon Corporation
 * Licensed under GPL-3.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../include/internal.h"

/* ── If/Else State Machine ─────────────────────────────────────────── */

#define CAPI_IF_TRUE   0   /* Executing: condition was true               */
#define CAPI_IF_FALSE  1   /* Skipping:  condition was false               */
#define CAPI_IF_DONE   2   /* True branch already ran; skip else too       */

#define IF_MAX_DEPTH   16

static int if_stack[IF_MAX_DEPTH];
static int if_depth = 0;

/*
 * Return non-zero when we are inside a FALSE/DONE branch at any depth.
 * Used by the if-handler to suppress nested content.
 */
static int if_suppressing(void)
{
    for (int i = 0; i < if_depth; i++) {
        if (if_stack[i] != CAPI_IF_TRUE)
            return 1;
    }
    return 0;
}

/*
 * Try to parse a number from a string.
 * Returns 1 on success and stores the value in *out.
 */
static int try_parse_int(const char *s, long *out)
{
    char *end = NULL;
    if (s == NULL || *s == '\0')
        return 0;
    long val = strtol(s, &end, 10);
    if (*end != '\0')
        return 0;
    *out = val;
    return 1;
}

/* ── capi_handle_if ────────────────────────────────────────────────── */

int capi_handle_if(const char *line, CapiResult *res)
{
    /* ---- endif; ---- */
    if (strcmp(line, "endif;") == 0) {
        if (if_depth > 0)
            if_depth--;
        return 1;
    }

    /* ---- else: ---- */
    if (strcmp(line, "else:") == 0) {
        if (if_depth > 0) {
            int top = if_depth - 1;
            if (if_stack[top] == CAPI_IF_TRUE)
                if_stack[top] = CAPI_IF_DONE;
            else if (if_stack[top] == CAPI_IF_FALSE)
                if_stack[top] = CAPI_IF_TRUE;
            /* CAPI_IF_DONE stays DONE */
        }
        return 1;
    }

    /* ---- if ... : ---- */
    if (strncmp(line, "if ", 3) == 0) {
        /* If we are already suppressing, just push FALSE to keep depth right */
        if (if_suppressing()) {
            if (if_depth < IF_MAX_DEPTH)
                if_stack[if_depth++] = CAPI_IF_FALSE;
            return 1;
        }

        /* Parse the condition.
         * Supported forms:
         *   if var == "value":
         *   if var != "value":
         *   if var <op> num:       (numeric: <, >, ==, !=, <=, >=)
         */
        char var[CAPI_NAME_LEN] = {0};
        char op[4]  = {0};
        char val[CAPI_VALUE_LEN] = {0};

        /* Work on a copy without the trailing ':' */
        char buf[CAPI_LINE_BUF];
        strncpy(buf, line + 3, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';

        /* Strip trailing colon */
        size_t blen = strlen(buf);
        if (blen > 0 && buf[blen - 1] == ':')
            buf[blen - 1] = '\0';

        /* Tokenise: var op value */
        char *saveptr = NULL;
        char *tok_var = strtok_r(buf, " ", &saveptr);
        char *tok_op  = strtok_r(NULL, " ", &saveptr);
        /* The rest is the value (may contain spaces inside quotes) */
        char *tok_val = saveptr;  /* remainder */

        if (tok_var == NULL || tok_op == NULL || tok_val == NULL) {
            if (if_depth < IF_MAX_DEPTH)
                if_stack[if_depth++] = CAPI_IF_FALSE;
            return 1;
        }

        strncpy(var, tok_var, sizeof(var) - 1);
        var[sizeof(var) - 1] = '\0';
        strncpy(op, tok_op, sizeof(op) - 1);
        op[sizeof(op) - 1] = '\0';

        /* Trim leading/trailing whitespace and optional quotes from value */
        while (*tok_val == ' ')
            tok_val++;
        size_t vlen = strlen(tok_val);
        while (vlen > 0 && tok_val[vlen - 1] == ' ')
            tok_val[--vlen] = '\0';

        int quoted = 0;
        if (vlen >= 2 && tok_val[0] == '"' && tok_val[vlen - 1] == '"') {
            tok_val[vlen - 1] = '\0';
            tok_val++;
            quoted = 1;
        }

        strncpy(val, tok_val, sizeof(val) - 1);
        val[sizeof(val) - 1] = '\0';

        /* Resolve the variable */
        const char *var_value = capi_get_variable(var);
        const char *lhs = var_value ? var_value : "";

        int cond = 0;

        /* Attempt numeric comparison when not quoted */
        long lhs_num = 0, rhs_num = 0;
        int numeric = 0;
        if (!quoted) {
            numeric = try_parse_int(lhs, &lhs_num) && try_parse_int(val, &rhs_num);
        }

        if (strcmp(op, "==") == 0) {
            cond = numeric ? (lhs_num == rhs_num) : (strcmp(lhs, val) == 0);
        } else if (strcmp(op, "!=") == 0) {
            cond = numeric ? (lhs_num != rhs_num) : (strcmp(lhs, val) != 0);
        } else if (strcmp(op, "<") == 0) {
            cond = numeric ? (lhs_num < rhs_num) : (strcmp(lhs, val) < 0);
        } else if (strcmp(op, ">") == 0) {
            cond = numeric ? (lhs_num > rhs_num) : (strcmp(lhs, val) > 0);
        } else if (strcmp(op, "<=") == 0) {
            cond = numeric ? (lhs_num <= rhs_num) : (strcmp(lhs, val) <= 0);
        } else if (strcmp(op, ">=") == 0) {
            cond = numeric ? (lhs_num >= rhs_num) : (strcmp(lhs, val) >= 0);
        }

        if (if_depth < IF_MAX_DEPTH)
            if_stack[if_depth++] = cond ? CAPI_IF_TRUE : CAPI_IF_FALSE;

        return 1;
    }

    /* If we are currently suppressing, consume any line */
    if (if_suppressing())
        return 1;

    return 0;
}

/* ── While Loop ────────────────────────────────────────────────────── */

#define WHILE_MAX_DEPTH  8
#define WHILE_MAX_ITER   10000

typedef struct {
    char var[CAPI_NAME_LEN];
    char op[4];
    char rhs[CAPI_VALUE_LEN];
    char lines[CAPI_MAX_LINES][CAPI_LINE_BUF];
    int  line_count;
    int  collecting;   /* 1 = currently gathering lines */
} WhileState;

static WhileState while_stack[WHILE_MAX_DEPTH];
static int        while_depth = 0;

/*
 * Evaluate a while-condition:  var <op> rhs
 * Numeric when both sides parse as integers, otherwise string compare.
 */
static int while_cond_met(const WhileState *ws)
{
    const char *lhs_str = capi_get_variable(ws->var);
    if (lhs_str == NULL)
        lhs_str = "";

    long lhs_num = 0, rhs_num = 0;
    int numeric = try_parse_int(lhs_str, &lhs_num) &&
                  try_parse_int(ws->rhs, &rhs_num);

    if (strcmp(ws->op, "<") == 0)
        return numeric ? (lhs_num <  rhs_num) : (strcmp(lhs_str, ws->rhs) <  0);
    if (strcmp(ws->op, ">") == 0)
        return numeric ? (lhs_num >  rhs_num) : (strcmp(lhs_str, ws->rhs) >  0);
    if (strcmp(ws->op, "==") == 0)
        return numeric ? (lhs_num == rhs_num) : (strcmp(lhs_str, ws->rhs) == 0);
    if (strcmp(ws->op, "!=") == 0)
        return numeric ? (lhs_num != rhs_num) : (strcmp(lhs_str, ws->rhs) != 0);
    if (strcmp(ws->op, "<=") == 0)
        return numeric ? (lhs_num <= rhs_num) : (strcmp(lhs_str, ws->rhs) <= 0);
    if (strcmp(ws->op, ">=") == 0)
        return numeric ? (lhs_num >= rhs_num) : (strcmp(lhs_str, ws->rhs) >= 0);

    return 0;
}

int capi_handle_while(const char *line, CapiResult *res)
{
    /* Are we inside a collecting while? Capture lines. */
    if (while_depth > 0 && while_stack[while_depth - 1].collecting) {
        WhileState *ws = &while_stack[while_depth - 1];

        if (strcmp(line, "endwhile;") == 0) {
            ws->collecting = 0;

            /* Execute the collected body while condition holds */
            int iter = 0;
            while (while_cond_met(ws) && iter < WHILE_MAX_ITER) {
                for (int i = 0; i < ws->line_count; i++)
                    capi_process_line(ws->lines[i], res);
                iter++;
            }

            while_depth--;
            return 1;
        }

        /* Accumulate the line */
        if (ws->line_count < CAPI_MAX_LINES) {
            strncpy(ws->lines[ws->line_count], line, CAPI_LINE_BUF - 1);
            ws->lines[ws->line_count][CAPI_LINE_BUF - 1] = '\0';
            ws->line_count++;
        }
        return 1;
    }

    /* ---- while var <op> rhs: ---- */
    if (strncmp(line, "while ", 6) == 0) {
        if (while_depth >= WHILE_MAX_DEPTH) {
            capi_append_output(res, "[error] while: max nesting depth exceeded\n");
            return 1;
        }

        char buf[CAPI_LINE_BUF];
        strncpy(buf, line + 6, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';

        /* Strip trailing colon */
        size_t blen = strlen(buf);
        if (blen > 0 && buf[blen - 1] == ':')
            buf[blen - 1] = '\0';

        char *saveptr = NULL;
        char *tok_var = strtok_r(buf, " ", &saveptr);
        char *tok_op  = strtok_r(NULL, " ", &saveptr);
        char *tok_rhs = strtok_r(NULL, " ", &saveptr);

        if (tok_var == NULL || tok_op == NULL || tok_rhs == NULL) {
            capi_append_output(res, "[error] while: invalid syntax\n");
            return 1;
        }

        WhileState *ws = &while_stack[while_depth];
        memset(ws, 0, sizeof(*ws));

        strncpy(ws->var, tok_var, sizeof(ws->var) - 1);
        ws->var[sizeof(ws->var) - 1] = '\0';
        strncpy(ws->op, tok_op, sizeof(ws->op) - 1);
        ws->op[sizeof(ws->op) - 1] = '\0';
        strncpy(ws->rhs, tok_rhs, sizeof(ws->rhs) - 1);
        ws->rhs[sizeof(ws->rhs) - 1] = '\0';

        ws->collecting = 1;
        ws->line_count = 0;
        while_depth++;

        return 1;
    }

    return 0;
}

/* ── Math ──────────────────────────────────────────────────────────── */

int capi_handle_math(const char *line, CapiResult *res)
{
    if (strncmp(line, "math ", 5) != 0)
        return 0;

    /* Syntax: math <varname> = <op1> <operator> <op2>; */
    char buf[CAPI_LINE_BUF];
    strncpy(buf, line + 5, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    /* Strip trailing semicolon */
    size_t blen = strlen(buf);
    if (blen > 0 && buf[blen - 1] == ';')
        buf[blen - 1] = '\0';

    char *saveptr = NULL;
    char *tok_dest = strtok_r(buf, " ", &saveptr);
    char *tok_eq   = strtok_r(NULL, " ", &saveptr);
    char *tok_a    = strtok_r(NULL, " ", &saveptr);
    char *tok_op   = strtok_r(NULL, " ", &saveptr);
    char *tok_b    = strtok_r(NULL, " ", &saveptr);

    if (tok_dest == NULL || tok_eq == NULL || tok_a == NULL ||
        tok_op  == NULL  || tok_b == NULL  || strcmp(tok_eq, "=") != 0) {
        capi_append_output(res, "[error] math: invalid syntax\n");
        return 1;
    }

    /* Resolve operands — could be variable names or literals */
    const char *val_a_str = capi_get_variable(tok_a);
    if (val_a_str == NULL)
        val_a_str = tok_a;

    const char *val_b_str = capi_get_variable(tok_b);
    if (val_b_str == NULL)
        val_b_str = tok_b;

    long a = strtol(val_a_str, NULL, 10);
    long b = strtol(val_b_str, NULL, 10);
    long result = 0;

    if (strcmp(tok_op, "+") == 0)      result = a + b;
    else if (strcmp(tok_op, "-") == 0) result = a - b;
    else if (strcmp(tok_op, "*") == 0) result = a * b;
    else if (strcmp(tok_op, "/") == 0) {
        if (b == 0) {
            capi_append_output(res, "[error] math: division by zero\n");
            return 1;
        }
        result = a / b;
    } else if (strcmp(tok_op, "%") == 0) {
        if (b == 0) {
            capi_append_output(res, "[error] math: division by zero\n");
            return 1;
        }
        result = a % b;
    } else {
        capi_append_output(res, "[error] math: unknown operator\n");
        return 1;
    }

    char result_str[CAPI_VALUE_LEN];
    snprintf(result_str, sizeof(result_str), "%ld", result);
    capi_set_variable(tok_dest, result_str);

    return 1;
}

/* ── String Operations ─────────────────────────────────────────────── */

int capi_handle_string_op(const char *line, CapiResult *res)
{
    if (strncmp(line, "str.", 4) != 0)
        return 0;

    /* ---- str.len var; ---- */
    if (strncmp(line, "str.len ", 8) == 0) {
        char var[CAPI_NAME_LEN];
        strncpy(var, line + 8, sizeof(var) - 1);
        var[sizeof(var) - 1] = '\0';
        /* Strip trailing semicolon */
        size_t vlen = strlen(var);
        if (vlen > 0 && var[vlen - 1] == ';')
            var[vlen - 1] = '\0';

        const char *val = capi_get_variable(var);
        char out[CAPI_VALUE_LEN];
        snprintf(out, sizeof(out), "%zu\n", val ? strlen(val) : (size_t)0);
        capi_append_output(res, out);
        return 1;
    }

    /* ---- str.upper var; ---- */
    if (strncmp(line, "str.upper ", 10) == 0) {
        char var[CAPI_NAME_LEN];
        strncpy(var, line + 10, sizeof(var) - 1);
        var[sizeof(var) - 1] = '\0';
        size_t vlen = strlen(var);
        if (vlen > 0 && var[vlen - 1] == ';')
            var[vlen - 1] = '\0';

        const char *val = capi_get_variable(var);
        if (val) {
            char upper[CAPI_VALUE_LEN];
            strncpy(upper, val, sizeof(upper) - 1);
            upper[sizeof(upper) - 1] = '\0';
            for (size_t i = 0; upper[i]; i++)
                upper[i] = (char)toupper((unsigned char)upper[i]);
            capi_set_variable(var, upper);
        }
        return 1;
    }

    /* ---- str.lower var; ---- */
    if (strncmp(line, "str.lower ", 10) == 0) {
        char var[CAPI_NAME_LEN];
        strncpy(var, line + 10, sizeof(var) - 1);
        var[sizeof(var) - 1] = '\0';
        size_t vlen = strlen(var);
        if (vlen > 0 && var[vlen - 1] == ';')
            var[vlen - 1] = '\0';

        const char *val = capi_get_variable(var);
        if (val) {
            char lower[CAPI_VALUE_LEN];
            strncpy(lower, val, sizeof(lower) - 1);
            lower[sizeof(lower) - 1] = '\0';
            for (size_t i = 0; lower[i]; i++)
                lower[i] = (char)tolower((unsigned char)lower[i]);
            capi_set_variable(var, lower);
        }
        return 1;
    }

    /* ---- str.concat dest a b; ---- */
    if (strncmp(line, "str.concat ", 11) == 0) {
        char buf[CAPI_LINE_BUF];
        strncpy(buf, line + 11, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        size_t blen = strlen(buf);
        if (blen > 0 && buf[blen - 1] == ';')
            buf[blen - 1] = '\0';

        char *saveptr = NULL;
        char *dest = strtok_r(buf, " ", &saveptr);
        char *src_a = strtok_r(NULL, " ", &saveptr);
        char *src_b = strtok_r(NULL, " ", &saveptr);

        if (dest == NULL || src_a == NULL || src_b == NULL) {
            capi_append_output(res, "[error] str.concat: invalid syntax\n");
            return 1;
        }

        const char *va = capi_get_variable(src_a);
        const char *vb = capi_get_variable(src_b);
        if (va == NULL) va = "";
        if (vb == NULL) vb = "";

        char combined[CAPI_VALUE_LEN];
        snprintf(combined, sizeof(combined), "%s%s", va, vb);
        capi_set_variable(dest, combined);
        return 1;
    }

    return 0;
}

/* ── Array Operations ──────────────────────────────────────────────── */

/*
 * Find an array by name. Returns the index, or -1 if not found.
 */
static int find_array(const char *name)
{
    for (int i = 0; i < g_array_count; i++) {
        if (strcmp(g_arrays[i].name, name) == 0)
            return i;
    }
    return -1;
}

int capi_handle_array(const char *line, CapiResult *res)
{
    /* ---- array.get name idx; ---- */
    if (strncmp(line, "array.get ", 10) == 0) {
        char buf[CAPI_LINE_BUF];
        strncpy(buf, line + 10, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        size_t blen = strlen(buf);
        if (blen > 0 && buf[blen - 1] == ';')
            buf[blen - 1] = '\0';

        char *saveptr = NULL;
        char *name = strtok_r(buf, " ", &saveptr);
        char *idx_str = strtok_r(NULL, " ", &saveptr);

        if (name == NULL || idx_str == NULL) {
            capi_append_output(res, "[error] array.get: invalid syntax\n");
            return 1;
        }

        /* Resolve idx — could be a variable */
        const char *idx_val = capi_get_variable(idx_str);
        int idx = atoi(idx_val ? idx_val : idx_str);

        int ai = find_array(name);
        if (ai < 0) {
            capi_append_output(res, "[error] array.get: array not found\n");
            return 1;
        }
        if (idx < 0 || idx >= g_arrays[ai].count) {
            capi_append_output(res, "[error] array.get: index out of bounds\n");
            return 1;
        }

        capi_append_output(res, g_arrays[ai].items[idx]);
        capi_append_output(res, "\n");
        return 1;
    }

    /* ---- array.push name "val"; ---- */
    if (strncmp(line, "array.push ", 11) == 0) {
        char buf[CAPI_LINE_BUF];
        strncpy(buf, line + 11, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        size_t blen = strlen(buf);
        if (blen > 0 && buf[blen - 1] == ';')
            buf[blen - 1] = '\0';

        char *saveptr = NULL;
        char *name = strtok_r(buf, " ", &saveptr);
        char *val  = saveptr;

        if (name == NULL || val == NULL) {
            capi_append_output(res, "[error] array.push: invalid syntax\n");
            return 1;
        }

        /* Trim whitespace and optional quotes */
        while (*val == ' ') val++;
        size_t vlen = strlen(val);
        if (vlen >= 2 && val[0] == '"' && val[vlen - 1] == '"') {
            val[vlen - 1] = '\0';
            val++;
        }

        int ai = find_array(name);
        if (ai < 0) {
            capi_append_output(res, "[error] array.push: array not found\n");
            return 1;
        }
        if (g_arrays[ai].count >= CAPI_MAX_ARRAY_ITEMS) {
            capi_append_output(res, "[error] array.push: array full\n");
            return 1;
        }

        strncpy(g_arrays[ai].items[g_arrays[ai].count], val, CAPI_VALUE_LEN - 1);
        g_arrays[ai].items[g_arrays[ai].count][CAPI_VALUE_LEN - 1] = '\0';
        g_arrays[ai].count++;
        return 1;
    }

    /* ---- array.len name; ---- */
    if (strncmp(line, "array.len ", 10) == 0) {
        char name[CAPI_NAME_LEN];
        strncpy(name, line + 10, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
        size_t nlen = strlen(name);
        if (nlen > 0 && name[nlen - 1] == ';')
            name[nlen - 1] = '\0';

        int ai = find_array(name);
        if (ai < 0) {
            capi_append_output(res, "[error] array.len: array not found\n");
            return 1;
        }

        char out[CAPI_VALUE_LEN];
        snprintf(out, sizeof(out), "%d\n", g_arrays[ai].count);
        capi_append_output(res, out);
        return 1;
    }

    /* ---- array name = "v1" "v2" ...; ---- */
    if (strncmp(line, "array ", 6) == 0) {
        char buf[CAPI_LINE_BUF];
        strncpy(buf, line + 6, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        size_t blen = strlen(buf);
        if (blen > 0 && buf[blen - 1] == ';')
            buf[blen - 1] = '\0';

        /* Extract name */
        char *saveptr = NULL;
        char *name = strtok_r(buf, " ", &saveptr);
        char *eq   = strtok_r(NULL, " ", &saveptr);
        char *rest = saveptr;

        if (name == NULL || eq == NULL || strcmp(eq, "=") != 0 || rest == NULL) {
            capi_append_output(res, "[error] array: invalid syntax\n");
            return 1;
        }

        /* Find or create the array */
        int ai = find_array(name);
        if (ai < 0) {
            if (g_array_count >= CAPI_MAX_ARRAYS) {
                capi_append_output(res, "[error] array: max arrays exceeded\n");
                return 1;
            }
            ai = g_array_count++;
            strncpy(g_arrays[ai].name, name, CAPI_NAME_LEN - 1);
            g_arrays[ai].name[CAPI_NAME_LEN - 1] = '\0';
        }
        g_arrays[ai].count = 0;

        /* Parse quoted items: "val1" "val2" ... */
        const char *p = rest;
        while (*p) {
            while (*p == ' ') p++;
            if (*p == '"') {
                p++;  /* skip opening quote */
                const char *start = p;
                while (*p && *p != '"') p++;
                size_t ilen = (size_t)(p - start);
                if (ilen >= CAPI_VALUE_LEN)
                    ilen = CAPI_VALUE_LEN - 1;

                if (g_arrays[ai].count < CAPI_MAX_ARRAY_ITEMS) {
                    memcpy(g_arrays[ai].items[g_arrays[ai].count], start, ilen);
                    g_arrays[ai].items[g_arrays[ai].count][ilen] = '\0';
                    g_arrays[ai].count++;
                }
                if (*p == '"') p++;  /* skip closing quote */
            } else {
                break;
            }
        }
        return 1;
    }

    return 0;
}
