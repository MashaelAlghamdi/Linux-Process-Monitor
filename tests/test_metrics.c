#include <stdio.h>
#include <assert.h>
#include <math.h>
#include "metrics.h"

#define EPSILON 0.0001

/* Check whether two floating-point values are approximately equal */
void check_result(double actual, double expected) {
    assert(fabs(actual - expected) < EPSILON);
}

int main(void) {
    double result = -1.0;

    /* Sample system information */
    SystemInfo prev_sys = {0};
    SystemInfo curr_sys = {0};

    prev_sys.total_cpu_ticks = 1000;
    prev_sys.idle_cpu_ticks = 300;

    curr_sys.total_cpu_ticks = 2000;
    curr_sys.idle_cpu_ticks = 600;
    curr_sys.memory_total_kb = 8000;
    curr_sys.memory_available_kb = 2000;

    /* Sample process information */
    Process prev_process = {0};
    Process curr_process = {0};

    prev_process.pid = 123;
    prev_process.starttime_ticks = 50;
    prev_process.cpu_time_ticks = 100;

    curr_process.pid = 123;
    curr_process.starttime_ticks = 50;
    curr_process.cpu_time_ticks = 150;
    curr_process.rss_kb = 500;

    /* Test 1: Process CPU = 5% */
    assert(calculate_process_cpu(
        &prev_process, &curr_process,
        &prev_sys, &curr_sys, &result) == 0);

    check_result(result, 5.0);
    printf("PASS: Process CPU = %.2f%%\n", result);

    /* Test 2: Process Memory = 6.25% */
    assert(calculate_process_memory(
        &curr_process, &curr_sys, &result) == 0);

    check_result(result, 6.25);
    printf("PASS: Process Memory = %.2f%%\n", result);

    /* Test 3: System CPU = 70% */
    assert(calculate_system_cpu(
        &prev_sys, &curr_sys, &result) == 0);

    check_result(result, 70.0);
    printf("PASS: System CPU = %.2f%%\n", result);

    /* Test 4: System Memory = 75% */
    assert(calculate_system_memory(
        &curr_sys, &result) == 0);

    check_result(result, 75.0);
    printf("PASS: System Memory = %.2f%%\n", result);

    /* Test 5: First CPU sample */
    assert(calculate_process_cpu(
        NULL, &curr_process,
        NULL, &curr_sys, &result) == -1);

    printf("PASS: First sample handled correctly\n");

    /* Test 6: Zero CPU delta */
    assert(calculate_system_cpu(
        &curr_sys, &curr_sys, &result) == -1);

    printf("PASS: Zero CPU delta handled correctly\n");

    /* Test 7: Process counter reset */
    Process reset_process = curr_process;
    reset_process.cpu_time_ticks = 50;

    assert(calculate_process_cpu(
        &prev_process, &reset_process,
        &prev_sys, &curr_sys, &result) == -1);

    printf("PASS: Counter reset handled correctly\n");

    /* Test 8: Reused PID */
    Process reused_process = curr_process;
    reused_process.starttime_ticks = 999;

    assert(calculate_process_cpu(
        &prev_process, &reused_process,
        &prev_sys, &curr_sys, &result) == -1);

    printf("PASS: Reused PID handled correctly\n");

    /* Test 9: Unavailable memory */
    Process invalid_process = curr_process;
    invalid_process.rss_kb = -1;

    assert(calculate_process_memory(
        &invalid_process, &curr_sys, &result) == -1);

    printf("PASS: Unavailable memory handled correctly\n");

    /* Test 10: Zero total memory */
    SystemInfo invalid_sys = curr_sys;
    invalid_sys.memory_total_kb = 0;

    assert(calculate_system_memory(
        &invalid_sys, &result) == -1);

    printf("PASS: Zero total memory handled correctly\n");

    printf("\nAll 10 tests passed successfully!\n");

    return 0;
}
