#include "metrics.h"

/*
 * Calculate CPU usage for a single process.
 * Uses two samples to calculate CPU percentage.
 */
int calculate_process_cpu(
    const Process *previous,
    const Process *current,
    const SystemInfo *prev_sys,
    const SystemInfo *curr_sys,
    double *cpu_percent
) {
    if (!previous || !current || !prev_sys ||
        !curr_sys || !cpu_percent) {
        return -1;
    }

    /* Check if this is the same process */
    if (previous->pid != current->pid ||
        previous->starttime_ticks != current->starttime_ticks) {
        return -1;
    }

    /* Check for counter resets */
    if (current->cpu_time_ticks < previous->cpu_time_ticks ||
        curr_sys->total_cpu_ticks <= prev_sys->total_cpu_ticks) {
        return -1;
    }

    unsigned long long process_delta =
        current->cpu_time_ticks - previous->cpu_time_ticks;

    unsigned long long total_delta =
        curr_sys->total_cpu_ticks - prev_sys->total_cpu_ticks;

    *cpu_percent =
        ((double)process_delta / (double)total_delta) * 100.0;

    return 0;
}

/*
 * Calculate memory usage percentage for a process.
 */
int calculate_process_memory(
    const Process *process,
    const SystemInfo *system,
    double *memory_percent
) {
    if (!process || !system || !memory_percent) {
        return -1;
    }

    if (process->rss_kb < 0 || system->memory_total_kb == 0) {
        return -1;
    }

    *memory_percent =
        ((double)process->rss_kb /
         (double)system->memory_total_kb) * 100.0;

    return 0;
}

/*
 * Calculate total system CPU usage.
 */
int calculate_system_cpu(
    const SystemInfo *previous,
    const SystemInfo *current,
    double *cpu_percent
) {
    if (!previous || !current || !cpu_percent) {
        return -1;
    }

    if (current->total_cpu_ticks <= previous->total_cpu_ticks ||
        current->idle_cpu_ticks < previous->idle_cpu_ticks) {
        return -1;
    }

    unsigned long long total_delta =
        current->total_cpu_ticks - previous->total_cpu_ticks;

    unsigned long long idle_delta =
        current->idle_cpu_ticks - previous->idle_cpu_ticks;

    if (idle_delta > total_delta) {
        return -1;
    }

    *cpu_percent =
        ((double)(total_delta - idle_delta) /
         (double)total_delta) * 100.0;

    return 0;
}

/*
 * Calculate total system memory usage.
 */
int calculate_system_memory(
    const SystemInfo *system,
    double *memory_percent
) {
    if (!system || !memory_percent) {
        return -1;
    }

    if (system->memory_total_kb == 0 ||
        system->memory_available_kb > system->memory_total_kb) {
        return -1;
    }

    *memory_percent =
        ((double)(system->memory_total_kb -
                  system->memory_available_kb) /
         (double)system->memory_total_kb) * 100.0;

    return 0;
}
