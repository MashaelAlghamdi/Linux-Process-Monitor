#ifndef PROCESS_H
#define PROCESS_H
#include <stddef.h>
#include <sys/types.h>
#define PROCESS_NAME_MAX 256
#define PROCESS_CMDLINE_MAX 4096

typedef struct {
    pid_t pid;
    pid_t ppid;
    char name[PROCESS_NAME_MAX];
    char state;
    uid_t uid;
    unsigned long long utime_ticks;
    unsigned long long stime_ticks;
    unsigned long long starttime_ticks;
    unsigned long long cpu_time_ticks;
    long rss_kb; /* -1 if unavailable */
    char cmdline[PROCESS_CMDLINE_MAX];
} Process;

typedef struct {
    unsigned long long total_cpu_ticks;
    unsigned long long idle_cpu_ticks;
    unsigned long long memory_total_kb;
    unsigned long long memory_available_kb;
    double uptime_seconds;
    double load_average[3];
} SystemInfo;
#endif
