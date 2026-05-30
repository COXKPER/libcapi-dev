/*
 * calib.c — .calib configuration file parser for libcapi
 *
 * Copyright (C) 2020 Neon Corporation
 * Licensed under GPL-3.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "../include/calib.h"

/* ── Static Helpers ────────────────────────────────────────────────── */

/*
 * trim_whitespace — trim leading and trailing whitespace in-place.
 */
static void trim_whitespace(char *str)
{
    char *start, *end;
    size_t len;

    if (!str) return;

    start = str;
    while (*start && isspace((unsigned char)*start))
        start++;

    if (*start == '\0') {
        str[0] = '\0';
        return;
    }

    end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end))
        end--;

    len = (size_t)(end - start + 1);
    if (start != str)
        memmove(str, start, len);
    str[len] = '\0';
}

/*
 * detect_type — auto-detect the CalibType of a value string.
 */
static CalibType detect_type(const char *value)
{
    char *endptr;
    const char *v;

    if (!value || *value == '\0')
        return CALIB_TYPE_STRING;

    v = value;

    /* Check for boolean keywords */
    if (strcasecmp(v, "true") == 0  || strcasecmp(v, "false") == 0 ||
        strcasecmp(v, "yes") == 0   || strcasecmp(v, "no") == 0    ||
        strcasecmp(v, "on") == 0    || strcasecmp(v, "off") == 0) {
        return CALIB_TYPE_BOOL;
    }

    /* Check for integer: strtol must consume the entire string */
    endptr = NULL;
    (void)strtol(v, &endptr, 10);
    if (endptr && *endptr == '\0' && endptr != v)
        return CALIB_TYPE_INT;

    /* Check for float: must contain '.' and strtod must consume the string */
    if (strchr(v, '.') != NULL) {
        endptr = NULL;
        (void)strtod(v, &endptr);
        if (endptr && *endptr == '\0' && endptr != v)
            return CALIB_TYPE_FLOAT;
    }

    return CALIB_TYPE_STRING;
}

/*
 * strip_quotes — remove surrounding double quotes from str in-place.
 */
static void strip_quotes(char *str)
{
    size_t len;

    if (!str) return;

    len = strlen(str);
    if (len >= 2 && str[0] == '"' && str[len - 1] == '"') {
        memmove(str, str + 1, len - 2);
        str[len - 2] = '\0';
    }
}

/* ── Find Helpers ──────────────────────────────────────────────────── */

/*
 * find_section — find a section by name. Returns index or -1.
 */
static int find_section(const CalibFile *cf, const char *section)
{
    int i;

    for (i = 0; i < cf->section_count; i++) {
        if (strcmp(cf->sections[i].name, section) == 0)
            return i;
    }
    return -1;
}

/*
 * find_entry — find an entry by key within a section. Returns pointer or NULL.
 */
static const CalibEntry *find_entry(const CalibFile *cf, const char *section, const char *key)
{
    int si, i;

    si = find_section(cf, section);
    if (si < 0) return NULL;

    for (i = 0; i < cf->sections[si].entry_count; i++) {
        if (strcmp(cf->sections[si].entries[i].key, key) == 0)
            return &cf->sections[si].entries[i];
    }
    return NULL;
}

/* ── Public API ────────────────────────────────────────────────────── */

int calib_load(CalibFile *cf, const char *filepath)
{
    FILE *fp;
    char line[1024];
    int cur_section = -1;

    if (!cf || !filepath)
        return -1;

    memset(cf, 0, sizeof(*cf));

    strncpy(cf->filepath, filepath, sizeof(cf->filepath) - 1);
    cf->filepath[sizeof(cf->filepath) - 1] = '\0';

    fp = fopen(filepath, "r");
    if (!fp)
        return -1;

    while (fgets(line, sizeof(line), fp) != NULL) {
        char *eq;

        trim_whitespace(line);

        /* skip blank lines and comments */
        if (line[0] == '\0' || line[0] == '#')
            continue;

        /* section header: [section] */
        if (line[0] == '[') {
            char *close = strchr(line, ']');
            if (close) {
                char sec_name[CALIB_SECTION_LEN];

                *close = '\0';
                strncpy(sec_name, line + 1, sizeof(sec_name) - 1);
                sec_name[sizeof(sec_name) - 1] = '\0';
                trim_whitespace(sec_name);

                /* find or create section */
                cur_section = find_section(cf, sec_name);
                if (cur_section < 0) {
                    if (cf->section_count >= CALIB_MAX_SECTIONS) {
                        fclose(fp);
                        return -1;
                    }
                    cur_section = cf->section_count;
                    size_t len = strlen(sec_name);
                    if (len >= CALIB_SECTION_LEN) len = CALIB_SECTION_LEN - 1;
                    memcpy(cf->sections[cur_section].name, sec_name, len);
                    cf->sections[cur_section].name[len] = '\0';
                    cf->sections[cur_section].entry_count = 0;
                    cf->section_count++;
                }
            }
            continue;
        }

        /* key = value pair */
        eq = strchr(line, '=');
        if (eq && cur_section >= 0) {
            CalibEntry *entry;
            char key_buf[CALIB_KEY_LEN];
            char val_buf[CALIB_VALUE_LEN];
            CalibSection *sec = &cf->sections[cur_section];

            /* split at '=' */
            *eq = '\0';
            {
                size_t len = strlen(line);
                if (len >= sizeof(key_buf)) len = sizeof(key_buf) - 1;
                memcpy(key_buf, line, len);
                key_buf[len] = '\0';
            }
            trim_whitespace(key_buf);

            {
                size_t len = strlen(eq + 1);
                if (len >= sizeof(val_buf)) len = sizeof(val_buf) - 1;
                memcpy(val_buf, eq + 1, len);
                val_buf[len] = '\0';
            }
            trim_whitespace(val_buf);
            strip_quotes(val_buf);

            /* store entry */
            if (sec->entry_count >= CALIB_MAX_ENTRIES) {
                fclose(fp);
                return -1;
            }

            entry = &sec->entries[sec->entry_count];
            {
                size_t klen = strlen(key_buf);
                size_t vlen = strlen(val_buf);
                if (klen >= CALIB_KEY_LEN) klen = CALIB_KEY_LEN - 1;
                if (vlen >= CALIB_VALUE_LEN) vlen = CALIB_VALUE_LEN - 1;
                memcpy(entry->key, key_buf, klen);
                entry->key[klen] = '\0';
                memcpy(entry->value, val_buf, vlen);
                entry->value[vlen] = '\0';
            }
            entry->type = detect_type(val_buf);
            sec->entry_count++;
        }
    }

    fclose(fp);
    return 0;
}

void calib_free(CalibFile *cf)
{
    if (!cf) return;
    memset(cf, 0, sizeof(*cf));
}

const char *calib_get_string(const CalibFile *cf, const char *section, const char *key)
{
    const CalibEntry *e;

    if (!cf || !section || !key)
        return "";

    e = find_entry(cf, section, key);
    if (e)
        return e->value;
    return "";
}

int calib_get_int(const CalibFile *cf, const char *section, const char *key, int default_val)
{
    const CalibEntry *e;

    if (!cf || !section || !key)
        return default_val;

    e = find_entry(cf, section, key);
    if (e)
        return atoi(e->value);
    return default_val;
}

double calib_get_float(const CalibFile *cf, const char *section, const char *key, double default_val)
{
    const CalibEntry *e;

    if (!cf || !section || !key)
        return default_val;

    e = find_entry(cf, section, key);
    if (e)
        return atof(e->value);
    return default_val;
}

int calib_get_bool(const CalibFile *cf, const char *section, const char *key, int default_val)
{
    const CalibEntry *e;

    if (!cf || !section || !key)
        return default_val;

    e = find_entry(cf, section, key);
    if (!e)
        return default_val;

    if (strcasecmp(e->value, "true") == 0  || strcasecmp(e->value, "yes") == 0 ||
        strcasecmp(e->value, "1") == 0     || strcasecmp(e->value, "on") == 0) {
        return 1;
    }

    if (strcasecmp(e->value, "false") == 0 || strcasecmp(e->value, "no") == 0 ||
        strcasecmp(e->value, "0") == 0     || strcasecmp(e->value, "off") == 0) {
        return 0;
    }

    return default_val;
}

int calib_set(CalibFile *cf, const char *section, const char *key, const char *value)
{
    int si, i;
    CalibSection *sec;
    CalibEntry *entry;

    if (!cf || !section || !key || !value)
        return -1;

    /* find or create section */
    si = find_section(cf, section);
    if (si < 0) {
        if (cf->section_count >= CALIB_MAX_SECTIONS)
            return -1;
        si = cf->section_count;
        size_t len = strlen(section);
        if (len >= CALIB_SECTION_LEN) len = CALIB_SECTION_LEN - 1;
        memcpy(cf->sections[si].name, section, len);
        cf->sections[si].name[len] = '\0';
        cf->sections[si].entry_count = 0;
        cf->section_count++;
    }

    sec = &cf->sections[si];

    /* find existing entry */
    for (i = 0; i < sec->entry_count; i++) {
        if (strcmp(sec->entries[i].key, key) == 0) {
            size_t len = strlen(value);
            if (len >= CALIB_VALUE_LEN) len = CALIB_VALUE_LEN - 1;
            memcpy(sec->entries[i].value, value, len);
            sec->entries[i].value[len] = '\0';
            sec->entries[i].type = detect_type(value);
            return 0;
        }
    }

    /* create new entry */
    if (sec->entry_count >= CALIB_MAX_ENTRIES)
        return -1;

    entry = &sec->entries[sec->entry_count];
    {
        size_t klen = strlen(key);
        size_t vlen = strlen(value);
        if (klen >= CALIB_KEY_LEN) klen = CALIB_KEY_LEN - 1;
        if (vlen >= CALIB_VALUE_LEN) vlen = CALIB_VALUE_LEN - 1;
        memcpy(entry->key, key, klen);
        entry->key[klen] = '\0';
        memcpy(entry->value, value, vlen);
        entry->value[vlen] = '\0';
    }
    entry->type = detect_type(value);
    sec->entry_count++;

    return 0;
}

int calib_save(const CalibFile *cf, const char *filepath)
{
    FILE *fp;
    int si, ei;

    if (!cf || !filepath)
        return -1;

    fp = fopen(filepath, "w");
    if (!fp)
        return -1;

    for (si = 0; si < cf->section_count; si++) {
        const CalibSection *sec = &cf->sections[si];

        if (si > 0)
            fprintf(fp, "\n");

        fprintf(fp, "[%s]\n", sec->name);

        for (ei = 0; ei < sec->entry_count; ei++) {
            fprintf(fp, "%s = %s\n", sec->entries[ei].key, sec->entries[ei].value);
        }
    }

    fclose(fp);
    return 0;
}
