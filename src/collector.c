#include "collector.h"
#include "parser.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int numeric_pid(const char *s, pid_t *pid) {
    if (!s || !*s) return 0;
    for (const char *p = s; *p; ++p) if (!isdigit((unsigned char)*p)) return 0;
    errno = 0;
    char *end;
    long n = strtol(s, &end, 10);
    if (errno || *end || n <= 0 || n > INT_MAX) return 0;
    *pid = (pid_t)n;
    return 1;
}

int get_process_ids(pid_t *pids, size_t capacity) {
    if (!pids && capacity) return -1;
    DIR *dir = opendir("/proc");
    if (!dir) return -1;
    struct dirent *entry;
    size_t count = 0;
    while ((entry = readdir(dir)) != NULL) {
        pid_t pid;
        if (numeric_pid(entry->d_name, &pid) && count < capacity) pids[count++] = pid;
    }
    closedir(dir);
    return (int)count;
}

static int read_text(const char *path, char *buffer, size_t capacity, size_t *bytes) {
    if (!buffer || capacity < 2) return -1;
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    size_t n = fread(buffer, 1, capacity - 1, f);
    int error = ferror(f);
    int too_long = n == capacity - 1 && fgetc(f) != EOF;
    fclose(f);
    if (error || too_long) return -1;
    buffer[n] = '\0';
    if (bytes) *bytes = n;
    return 0;
}

int collect_process(pid_t pid, Process *out) {
    if (!out || pid <= 0) return -1;
    memset(out, 0, sizeof(*out));
    char path[128];
    char text[16384];
    snprintf(path, sizeof(path), "/proc/%ld/stat", (long)pid);
    if (read_text(path, text, sizeof(text), NULL) || parse_process_stat(text, out)) return -1;
    if (out->pid != pid) return -1;
    snprintf(path, sizeof(path), "/proc/%ld/status", (long)pid);
    if (read_text(path, text, sizeof(text), NULL) || parse_process_status(text, out)) return -1;
    /* cmdline is optional: kernel threads and restricted processes can have empty data. */
    snprintf(path, sizeof(path), "/proc/%ld/cmdline", (long)pid);
    size_t n = 0;
    if (read_text(path, text, sizeof(text), &n) == 0)
        parse_cmdline(text, n, out->cmdline, sizeof(out->cmdline));
    return 0;
}

int collect_processes(Process *out, size_t capacity) {
    if (!out && capacity) return -1;
    DIR *dir = opendir("/proc");
    if (!dir) return -1;
    struct dirent *entry;
    size_t count = 0;
    while ((entry = readdir(dir)) != NULL && count < capacity) {
        pid_t pid;
        if (!numeric_pid(entry->d_name, &pid)) continue;
        Process item;
        if (collect_process(pid, &item) == 0) out[count++] = item;
    }
    closedir(dir);
    return (int)count;
}

int collect_system_info(SystemInfo *out) {
    if (!out) return -1;
    memset(out, 0, sizeof(*out));
    char text[16384];
    if (read_text("/proc/stat", text, sizeof(text), NULL) || parse_system_stat(text, out)) return -1;
    if (read_text("/proc/meminfo", text, sizeof(text), NULL) || parse_meminfo(text, out)) return -1;
    if (read_text("/proc/uptime", text, sizeof(text), NULL) || parse_uptime(text, out)) return -1;
    if (read_text("/proc/loadavg", text, sizeof(text), NULL) || parse_loadavg(text, out)) return -1;
    return 0;
}
