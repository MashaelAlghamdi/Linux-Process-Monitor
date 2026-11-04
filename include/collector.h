#ifndef COLLECTOR_H
#define COLLECTOR_H
#include <stddef.h>
#include "process.h"
/* Return number of PIDs, or -1 on failure. Results are capped at capacity */
int get_process_ids(pid_t *pids, size_t capacity);
/* Return 0 on success, -1 if process is gone, inaccessible, or malformed */
int collect_process(pid_t pid, Process *out);
/* Return number collected, or -1 if /proc cannot be scanned */
int collect_processes(Process *out, size_t capacity);
/* Return 0 on success, or -1 if required system information is unavailable */
int collect_system_info(SystemInfo *out);
#endif
