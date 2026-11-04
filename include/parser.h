#ifndef PARSER_H
#define PARSER_H
#include <stddef.h>
#include "process.h"
/* Return 0 on success and -1 for malformed/missing required data. */
int parse_process_stat(const char *line, Process *out);
int parse_process_status(const char *text, Process *out);
int parse_system_stat(const char *text, SystemInfo *out);
int parse_meminfo(const char *text, SystemInfo *out);
int parse_uptime(const char *text, SystemInfo *out);
int parse_loadavg(const char *text, SystemInfo *out);
void parse_cmdline(const char *data, size_t len, char *out, size_t capacity);
#endif
