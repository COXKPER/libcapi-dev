#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <time.h>
#include "libcapi.h"

#define MAX_VARS 100
#define MAX_FUNCS 50
#define MAX_LINES 512

typedef struct {
    char name[64];
    char value[256];
} CapiVar;

typedef struct {
    char name[64];
    char lines[MAX_LINES][256];
    int line_count;
} CapiFunction;

static CapiVar vars[MAX_VARS];
static int var_count = 0;

static CapiFunction funcs[MAX_FUNCS];
static int func_count = 0;
static int defining_function = 0;

static void safe_append(char *dest, const char *src, size_t maxlen) {
    size_t len = strlen(dest);
    if (len + strlen(src) < maxlen)
        strcat(dest, src);
}

static void set_variable(const char *name, const char *value) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(vars[i].name, name) == 0) {
            strncpy(vars[i].value, value, sizeof(vars[i].value) - 1);
            return;
        }
    }
    if (var_count < MAX_VARS) {
        strncpy(vars[var_count].name, name, sizeof(vars[var_count].name) - 1);
        strncpy(vars[var_count].value, value, sizeof(vars[var_count].value) - 1);
        var_count++;
    }
}

static const char *get_variable(const char *name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(vars[i].name, name) == 0)
            return vars[i].value;
    }
    return "";
}

static void append_output(CapiResult *res, const char *data) {
    safe_append(res->output, data, MAX_OUTPUT);
}

static void execute_shell(const char *cmd, CapiResult *res) {
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        append_output(res, "[Error executing command]\n");
        return;
    }
    char out[256];
    while (fgets(out, sizeof(out), fp))
        append_output(res, out);
    pclose(fp);
}

static void include_file(const char *path, CapiResult *res);

void report_error(const char *msg, int line, const char *file) {
    fprintf(stderr, "Error at line %d in %s: %s\n", line, file, msg);
}

void report_warning(const char *msg) {
    fprintf(stderr, "Warning: %s\n", msg);
}

static void run_function(const char *name, CapiResult *res) {
    for (int i = 0; i < func_count; i++) {
        if (strcmp(funcs[i].name, name) == 0) {
            for (int j = 0; j < funcs[i].line_count; j++) {
                process_line(funcs[i].lines[j], res);
                if (res->force_close) return;
            }
            return;
        }
    }
    char msg[128];
    snprintf(msg, sizeof(msg), "Undefined function: %s\n", name);
    append_output(res, msg);
}

void process_line(const char *raw, CapiResult *res) {
    char line[512];
    strncpy(line, raw, sizeof(line) - 1);
    line[strcspn(line, "\r\n")] = '\0'; // strip newline

    if (line[0] == '#' || strncmp(line, "//", 2) == 0 || strlen(line) == 0)
        return;

    // function definition start
    if (strncmp(line, "function ", 9) == 0) {
        if (func_count >= MAX_FUNCS) {
            report_error("Too many functions defined", 0, "capi");
            return;
        }
        sscanf(line, "function %[^:]:", funcs[func_count].name);
        funcs[func_count].line_count = 0;
        defining_function = 1;
        return;
    }

    // function definition end
    if (strstr(line, "funclose;")) {
        defining_function = 0;
        func_count++;
        return;
    }

    // if defining a function, store its lines
    if (defining_function) {
        CapiFunction *f = &funcs[func_count];
        if (f->line_count < MAX_LINES)
            strncpy(f->lines[f->line_count++], line, 255);
        return;
    }

    // variable declaration
    if (strncmp(line, "int ", 4) == 0) {
        char name[64], val[256];
        if (sscanf(line, "int %63[^=]= \"%255[^\"]\"", name, val) == 2 ||
            sscanf(line, "int %63[^=]= %255s", name, val) == 2) {
            set_variable(name, val);
        } else {
            report_warning("Invalid variable declaration");
        }
        return;
    }

    // echo
    if (strncmp(line, "echo:", 5) == 0) {
        char arg[256];
        if (sscanf(line, "echo: %255[^;];", arg) == 1) {
            const char *val = get_variable(arg);
            if (strlen(val) == 0) {
                // fallback: print literal
                append_output(res, arg);
            } else {
                append_output(res, val);
            }
            append_output(res, "\n");
        }
        return;
    }

    // include
    if (strncmp(line, "incl <", 6) == 0) {
        char incl_path[256];
        if (sscanf(line, "incl <%255[^>]", incl_path) == 1)
            include_file(incl_path, res);
        return;
    }

    // exec shell
    if (strncmp(line, "capi.exec:", 10) == 0) {
        char cmd[256];
        if (sscanf(line, "capi.exec: \"%255[^\"]\"", cmd) == 1)
            execute_shell(cmd, res);
        return;
    }

    // deprecated feature
    if (strstr(line, "capi.forceclose()")) {
        report_warning("capi.forceclose() is deprecated");
        res->force_close = 1;
        return;
    }

    // function call
    if (strchr(line, '(') && strchr(line, ')')) {
        char fname[64];
        if (sscanf(line, "%63[^()]", fname) == 1)
            run_function(fname, res);
        return;
    }

    // fallback
    report_warning("Unknown or invalid syntax");
}

static void include_file(const char *path, CapiResult *res) {
    FILE *file = fopen(path, "r");
    if (!file) {
        char msg[128];
        snprintf(msg, sizeof(msg), "[include error: cannot open %s]\n", path);
        append_output(res, msg);
        return;
    }

    char line[512];
    while (fgets(line, sizeof(line), file)) {
        process_line(line, res);
        if (res->force_close) break;
    }
    fclose(file);
}

CapiResult process_capi_file(const char *filepath) {
    CapiResult res;
    memset(&res, 0, sizeof(res));

    FILE *file = fopen(filepath, "r");
    if (!file) {
        snprintf(res.output, sizeof(res.output),
                 "[error: cannot open %s]\n", filepath);
        return res;
    }

    char line[512];
    while (fgets(line, sizeof(line), file)) {
        process_line(line, &res);
        if (res.force_close) break;
    }

    fclose(file);
    return res;
}
