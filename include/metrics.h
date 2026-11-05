#ifndef METRICS_H
#define METRICS_H

#include "process.h"

/*
 * Return values:
 *  0 = Calculation successful
 * -1 = Invalid input or unavailable metric
 */

/* Calculate CPU usage percentage for a process */
int calculate_process_cpu(
    const Process *previous,
    const Process *current,
    const SystemInfo *prev_sys,
    const SystemInfo *curr_sys,
    double *cpu_percent
);

/* Calculate memory usage percentage for a process */
int calculate_process_memory(
    const Process *process,
    const SystemInfo *system,
    double *memory_percent
);

/* Calculate total system CPU usage percentage */
int calculate_system_cpu(
    const SystemInfo *previous,
    const SystemInfo *current,
    double *cpu_percent
);

/* Calculate total system memory usage percentage */
int calculate_system_memory(
    const SystemInfo *system,
    double *memory_percent
);

#endif
