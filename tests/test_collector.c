#include "collector.h"
#include "parser.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void parser_tests(void) {
    Process p = {0};
    const char *stat = "1234 (my (test) process) S 1000 0 0 0 0 0 0 0 0 0 42 8 0 0 0 0 0 0 98765 0 0";
    assert(parse_process_stat(stat, &p) == 0);
    assert(p.pid == 1234 && p.ppid == 1000 && p.state == 'S');
    assert(strcmp(p.name, "my (test) process") == 0);
    assert(p.utime_ticks == 42 && p.stime_ticks == 8 && p.cpu_time_ticks == 50);
    assert(p.starttime_ticks == 98765);
    assert(parse_process_status("Name:\ttest\nUid:\t1000\t1000\t1000\t1000\nVmRSS:\t2048 kB\n", &p) == 0);
    assert(p.uid == 1000 && p.rss_kb == 2048);
    SystemInfo s = {0};
    assert(parse_system_stat("cpu  10 20 30 40 50 60 70 80 90 100\n", &s) == 0);
    assert(s.total_cpu_ticks == 550 && s.idle_cpu_ticks == 90);
    assert(parse_meminfo("MemTotal: 8000 kB\nMemAvailable: 3000 kB\n", &s) == 0);
    assert(s.memory_total_kb == 8000 && s.memory_available_kb == 3000);
    assert(parse_uptime("120.50 30.0", &s) == 0);
    assert(parse_loadavg("0.10 0.20 0.30 1/100 123", &s) == 0);
    char cmd[64];
    const char args[] = "bash\0-c\0echo hello\0";
    parse_cmdline(args, sizeof(args) - 1, cmd, sizeof(cmd));
    assert(strcmp(cmd, "bash -c echo hello") == 0);
    puts("Parser unit tests passed.");
}

int main(void) {
    parser_tests();
    SystemInfo sys;
    if (collect_system_info(&sys)) { perror("System collection failed"); return 1; }
    printf("\nSYSTEM INFORMATION\nCPU ticks: %llu (idle: %llu)\nMemory: %llu kB (available: %llu kB)\nUptime: %.1f s\nLoad: %.2f %.2f %.2f\n",
        sys.total_cpu_ticks, sys.idle_cpu_ticks, sys.memory_total_kb,
        sys.memory_available_kb, sys.uptime_seconds,
        sys.load_average[0], sys.load_average[1], sys.load_average[2]);
    pid_t pids[4096];
    int pid_count = get_process_ids(pids, 4096);
    assert(pid_count >= 0);
    printf("\nDiscovered %d PIDs.\n", pid_count);
    Process self;
    assert(collect_process(getpid(), &self) == 0);
    printf("Current process: PID=%ld Name=%s State=%c PPID=%ld UID=%lu CPU=%llu RSS=%ld kB\n",
        (long)self.pid, self.name, self.state, (long)self.ppid,
        (unsigned long)self.uid, self.cpu_time_ticks, self.rss_kb);
    Process *processes = calloc(4096, sizeof(*processes));
    if (!processes) return 1;
    int count = collect_processes(processes, 4096);
    assert(count >= 0);
    printf("\nCollected %d processes (first 10 shown):\n", count);
    puts("PID     PPID    UID     STATE CPU_TICKS RSS_KB NAME");
    for (int i = 0; i < count && i < 10; ++i) {
        Process *p = &processes[i];
        printf("%-7ld %-7ld %-7lu %-5c %-9llu %-6ld %s\n",
            (long)p->pid, (long)p->ppid, (unsigned long)p->uid,
            p->state, p->cpu_time_ticks, p->rss_kb, p->name);
    }
    free(processes);
    puts("\nTask 1 tests passed.");
    return 0;
}
