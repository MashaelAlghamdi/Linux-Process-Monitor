#include "parser.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int parse_process_stat(const char *line, Process *out) {
    if (!line || !out) return -1;
    const char *open = strchr(line, '(');
    const char *close = strrchr(line, ')');
    if (!open || !close || close <= open || close[1] != ' ' || close[2] == '\0') return -1;
    char *end = NULL;
    errno = 0;
    long pid = strtol(line, &end, 10);
    if (errno || end != open - 1 || pid <= 0 || pid > INT_MAX) return -1;
    size_t name_len = (size_t)(close - open - 1);
    if (name_len == 0 || name_len >= sizeof(out->name)) return -1;
    memcpy(out->name, open + 1, name_len);
    out->name[name_len] = '\0';
    out->pid = (pid_t)pid;
    const char *p = close + 2;
    if (!*p || p[1] != ' ') return -1;
    out->state = *p;
    p += 2;
    for (int field = 4; field <= 22; ++field) {
        while (*p == ' ') ++p;
        if (!*p) return -1;
        errno = 0;
        char *next;
        if (field == 4) {
            long val = strtol(p, &next, 10);
            if (errno || next == p || val < 0 || val > INT_MAX) return -1;
            out->ppid = (pid_t)val;
        } else {
            /* Other skipped fields can be signed (only parse target fields) */
            if (field == 14 || field == 15 || field == 22) {
                unsigned long long val = strtoull(p, &next, 10);
                if (errno || next == p || *p == '-') return -1;
                if (field == 14) out->utime_ticks = val;
                if (field == 15) out->stime_ticks = val;
                if (field == 22) out->starttime_ticks = val;
            } else {
                next = (char *)p;
                while (*next && !isspace((unsigned char)*next)) ++next;
                if (next == p) return -1;
            }
        }
        if (*next && !isspace((unsigned char)*next)) return -1;
        p = next;
    }
    if (ULLONG_MAX - out->utime_ticks < out->stime_ticks) return -1;
    out->cpu_time_ticks = out->utime_ticks + out->stime_ticks;
    return 0;
}

int parse_process_status(const char *text, Process *out) {
    if (!text || !out) return -1;
    int got_uid = 0;
    out->rss_kb = -1;
    const char *p = text;
    while (*p) {
        const char *end = strchr(p, '\n');
        size_t len = end ? (size_t)(end - p) : strlen(p);
        if (len >= 4 && strncmp(p, "Uid:", 4) == 0) {
            unsigned int uid;
            if (sscanf(p + 4, "%u", &uid) != 1) return -1;
            out->uid = (uid_t)uid;
            got_uid = 1;
        } else if (len >= 6 && strncmp(p, "VmRSS:", 6) == 0) {
            long rss;
            if (sscanf(p + 6, "%ld", &rss) != 1 || rss < 0) return -1;
            out->rss_kb = rss;
        }
        if (!end) break;
        p = end + 1;
    }
    return got_uid ? 0 : -1;
}

int parse_system_stat(const char *text, SystemInfo *out) {
    if (!text || !out || strncmp(text, "cpu ", 4) != 0) return -1;
    unsigned long long v[10] = {0};
    int count = sscanf(text + 4, "%llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
        &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7], &v[8], &v[9]);
    if (count < 4) return -1;
    unsigned long long total = 0;
    for (int i = 0; i < count; ++i) {
        if (ULLONG_MAX - total < v[i]) return -1;
        total += v[i];
    }
    out->total_cpu_ticks = total;
    out->idle_cpu_ticks = v[3] + (count >= 5 ? v[4] : 0);
    return 0;
}

int parse_meminfo(const char *text, SystemInfo *out) {
    if (!text || !out) return -1;
    int got_total = 0, got_available = 0;
    const char *p = text;
    while (*p) {
        if (strncmp(p, "MemTotal:", 9) == 0) {
            if (sscanf(p + 9, "%llu", &out->memory_total_kb) != 1) return -1;
            got_total = 1;
        } else if (strncmp(p, "MemAvailable:", 13) == 0) {
            if (sscanf(p + 13, "%llu", &out->memory_available_kb) != 1) return -1;
            got_available = 1;
        }
        const char *next = strchr(p, '\n');
        if (!next) break;
        p = next + 1;
    }
    return got_total && got_available ? 0 : -1;
}

int parse_uptime(const char *text, SystemInfo *out) {
    if (!text || !out || sscanf(text, "%lf", &out->uptime_seconds) != 1) return -1;
    return out->uptime_seconds >= 0 ? 0 : -1;
}

int parse_loadavg(const char *text, SystemInfo *out) {
    if (!text || !out || sscanf(text, "%lf %lf %lf", &out->load_average[0],
        &out->load_average[1], &out->load_average[2]) != 3) return -1;
    return 0;
}

void parse_cmdline(const char *data, size_t len, char *out, size_t capacity) {
    if (!out || !capacity) return;
    size_t used = 0;
    for (size_t i = 0; data && i < len && used + 1 < capacity; ++i) {
        char c = data[i];
        if (c == '\0') {
            if (i + 1 < len && used && out[used - 1] != ' ') out[used++] = ' ';
        } else if (isprint((unsigned char)c)) {
            out[used++] = c;
        } else {
            out[used++] = '?';
        }
    }
    while (used && out[used - 1] == ' ') --used;
    out[used] = '\0';
}
