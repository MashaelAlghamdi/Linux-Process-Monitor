/* the refresh loop + clean exit on Ctrl+C / kill */
#define _POSIX_C_SOURCE 200809L   /* needed for sigaction, nanosleep, clock_gettime with -std=c11 */
#include "refresh.h"
#include <signal.h>
#include <string.h>
#include <time.h>

/* the signal handler sets this and the loop checks it.
 * volatile sig_atomic_t is the safe type to use inside a handler */
static volatile sig_atomic_t stop_requested = 0;

static void on_signal(int sig) {
    (void)sig;
    stop_requested = 1;   /* just set the flag, the loop handles the rest */
}

void refresh_install_signal_handlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);    /* Ctrl+C */
    sigaction(SIGTERM, &sa, NULL);   /* kill <pid> */
}

int refresh_should_stop(void) {
    return stop_requested;
}




long long refresh_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void sleep_ms(int ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);   /* Ctrl+C wakes this up early */
}

int refresh_run(int interval_ms, int max_ticks,
                RefreshTick tick, RefreshWait wait, void *ctx) {
    if (tick == NULL || interval_ms < 100) return -1;

    int ticks = 0;
    while (!stop_requested) {
        if (tick(ctx) != 0) break;            /* do one refresh */
        ticks++;
        if (max_ticks > 0 && ticks >= max_ticks) break;
        if (stop_requested) break;

        if (wait != NULL) {                   /* wait for the next one */
            if (wait(ctx, interval_ms) != 0) break;
        } else {
            sleep_ms(interval_ms);
        }
    }
    return ticks;
}