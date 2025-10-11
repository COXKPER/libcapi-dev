#ifndef LIBCAPI_H
#define LIBCAPI_H

#define MAX_OUTPUT 8192

typedef struct {
    char output[MAX_OUTPUT];
    int force_close;
} CapiResult;

CapiResult process_capi_file(const char *filepath);
void process_line(const char *line, CapiResult *res);
void report_error(const char *msg, int line, const char *file);
void report_warning(const char *msg);

#endif
