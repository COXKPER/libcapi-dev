/*
 * calib — Calibration / Configuration File Parser for libcapi
 *
 * .calib file format:
 *   # comment
 *   [section]
 *   key = value
 *
 * Supports string, integer, float, and boolean values.
 *
 * Copyright (C) 2020 Neon Corporation
 * Licensed under GPL-3.0
 */

#ifndef CALIB_H
#define CALIB_H

#include <stddef.h>

/* ── Limits ─────────────────────────────────────────────────────────── */
#define CALIB_MAX_SECTIONS   32
#define CALIB_MAX_ENTRIES    128
#define CALIB_KEY_LEN        64
#define CALIB_VALUE_LEN      256
#define CALIB_SECTION_LEN    64

/* ── Types ──────────────────────────────────────────────────────────── */
typedef enum {
    CALIB_TYPE_STRING,
    CALIB_TYPE_INT,
    CALIB_TYPE_FLOAT,
    CALIB_TYPE_BOOL
} CalibType;

typedef struct {
    char      key[CALIB_KEY_LEN];
    char      value[CALIB_VALUE_LEN];
    CalibType type;
} CalibEntry;

typedef struct {
    char        name[CALIB_SECTION_LEN];
    CalibEntry  entries[CALIB_MAX_ENTRIES];
    int         entry_count;
} CalibSection;

typedef struct {
    CalibSection sections[CALIB_MAX_SECTIONS];
    int          section_count;
    char         filepath[512];
} CalibFile;

/* ── API ────────────────────────────────────────────────────────────── */

/*
 * Load and parse a .calib file. Returns 0 on success, -1 on error.
 */
int calib_load(CalibFile *cf, const char *filepath);

/*
 * Free / reset a CalibFile structure.
 */
void calib_free(CalibFile *cf);

/*
 * Get a value as a string.  Returns "" if not found.
 * Query format: "section.key"
 */
const char *calib_get_string(const CalibFile *cf, const char *section, const char *key);

/*
 * Get a value as an integer.  Returns `default_val` if not found.
 */
int calib_get_int(const CalibFile *cf, const char *section, const char *key, int default_val);

/*
 * Get a value as a float.  Returns `default_val` if not found.
 */
double calib_get_float(const CalibFile *cf, const char *section, const char *key, double default_val);

/*
 * Get a value as a boolean.  Returns `default_val` if not found.
 * Recognized true values: "true", "yes", "1", "on"
 * Recognized false values: "false", "no", "0", "off"
 */
int calib_get_bool(const CalibFile *cf, const char *section, const char *key, int default_val);

/*
 * Set a key-value pair in the given section. Creates section if needed.
 * Returns 0 on success, -1 if limits exceeded.
 */
int calib_set(CalibFile *cf, const char *section, const char *key, const char *value);

/*
 * Write the CalibFile contents back to a file.
 * Returns 0 on success, -1 on error.
 */
int calib_save(const CalibFile *cf, const char *filepath);

#endif /* CALIB_H */
