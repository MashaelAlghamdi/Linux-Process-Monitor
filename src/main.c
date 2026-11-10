/*
 * main.c - ties everything together and runs the refresh loop.
 * every refresh: collect (Task 1) -> calculate (Task 2) -> show (Task 3)
 *
 * usage: procmon [-d SECONDS] [-n COUNT] [-b]
 *   -d  seconds between refreshes, 0.1 to 60 (default 2)
 *   -n  quit after this many refreshes (default: until q / Ctrl+C)
 *   -b  plain text instead of the full-screen view
 */
#define _POSIX_C_SOURCE 200809L
#include "collector.h"
#include "error_handler.h"
#include "metrics.h"
#include "refresh.h"
#include "ui.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_PROCS 2048

typedef struct {
    Process      *cur, *prev;        /* this refresh and the one before */
    int           cur_n, prev_n;
    SystemInfo    cur_sys, prev_sys;
    int           have_prev;         /* CPU% needs two samples to compare */
    ProcessRow   *rows;
    SystemSummary summary;
    UiState       ui;
    int           plain;             /* 1 if we're in -b mode */
} App;




/* look for the same PID in the last sample (NULL if it's a new process) */
static const Process *find_prev(const App *a, pid_t pid) {
    for (int i = 0; i < a->prev_n; i++)
        if (a->prev[i].pid == pid) return &a->prev[i];
    return NULL;
}

static void draw(App *a) {
    if (a->plain) {
        /* make it tall enough to print every process */
        ui_render(stdout, &a->ui, &a->summary, a->rows, (size_t)a->cur_n,
                  a->cur_n + 8, 120, 0);
        printf("\n");
    } else {
        int rows, cols;
        ui_terminal_size(&rows, &cols);
        ui_render(stdout, &a->ui, &a->summary, a->rows, (size_t)a->cur_n,
                  rows, cols, 1);
    }
    fflush(stdout);
}




/* one refresh: collect, calculate, draw. refresh_run calls this */
static int refresh_tick(void *ctx) {
    App *a = ctx;

    /* last time's data becomes the "previous" sample */
    Process *tmp = a->prev;
    a->prev = a->cur;
    a->cur = tmp;
    a->prev_n = a->cur_n;
    a->prev_sys = a->cur_sys;

    errno = 0;
    if (collect_system_info(&a->cur_sys) != 0) {
        err_report(err_from_errno(errno), "reading system information");
        return 1;   /* can't continue without data */
    }
    a->cur_n = collect_processes(a->cur, MAX_PROCS);  /* already skips processes that ended */
    if (a->cur_n < 0) {
        err_report(ERR_IO, "scanning /proc");
        return 1;
    }

    for (int i = 0; i < a->cur_n; i++) {
        ProcessRow *r = &a->rows[i];
        r->process = &a->cur[i];

        const Process *old = a->have_prev ? find_prev(a, a->cur[i].pid) : NULL;
        r->cpu_valid = old != NULL &&
            calculate_process_cpu(old, &a->cur[i], &a->prev_sys, &a->cur_sys,
                                  &r->cpu_percent) == 0;
        r->mem_valid =
            calculate_process_memory(&a->cur[i], &a->cur_sys, &r->mem_percent) == 0;
    }

    a->summary.info = &a->cur_sys;
    a->summary.cpu_valid = a->have_prev &&
        calculate_system_cpu(&a->prev_sys, &a->cur_sys, &a->summary.cpu_percent) == 0;
    a->summary.mem_valid =
        calculate_system_memory(&a->cur_sys, &a->summary.mem_percent) == 0;
    a->have_prev = 1;

    draw(a);
    return 0;
}




/* between refreshes: handle key presses until it's time to refresh again */
static int wait_for_keys(void *ctx, int ms) {
    App *a = ctx;
    long long end = refresh_now_ms() + ms;

    while (!refresh_should_stop()) {
        long long left = end - refresh_now_ms();
        if (left <= 0) return 0;                 /* time to refresh */

        int key = ui_read_key((int)left);
        if (key == UI_KEY_EOF) return 1;
        if (key != UI_KEY_NONE && ui_handle_key(&a->ui, key)) draw(a);
        if (a->ui.quit) return 1;                /* user pressed q or Ctrl+C */
    }
    return 1;
}




int main(int argc, char **argv) {
    int interval_ms = 2000, count = 0, opt;
    App app = {0};

    while ((opt = getopt(argc, argv, "d:n:b")) != -1) {
        if (opt == 'd') {
            if (err_parse_interval(optarg, &interval_ms) != ERR_OK) {
                err_report(ERR_INVALID_INPUT, "-d needs seconds between 0.1 and 60");
                return 2;
            }
        } else if (opt == 'n') {
            count = atoi(optarg);
            if (count <= 0) {
                err_report(ERR_INVALID_INPUT, "-n needs a positive number");
                return 2;
            }
        } else if (opt == 'b') {
            app.plain = 1;
        } else {
            fprintf(stderr, "Usage: %s [-d SECONDS] [-n COUNT] [-b]\n", argv[0]);
            return 2;
        }
    }

    app.cur  = malloc(MAX_PROCS * sizeof(Process));
    app.prev = malloc(MAX_PROCS * sizeof(Process));
    app.rows = malloc(MAX_PROCS * sizeof(ProcessRow));
    if (!app.cur || !app.prev || !app.rows) {
        err_report(ERR_IO, "out of memory");
        return 1;
    }

    ui_state_init(&app.ui);
    refresh_install_signal_handlers();
    if (!app.plain && ui_init() != 0) app.plain = 1;   /* no terminal, so fall back to plain text */

    int done = refresh_run(interval_ms, count, refresh_tick,
                           app.plain ? NULL : wait_for_keys, &app);

    if (!app.plain) ui_shutdown();   /* put the terminal back to normal */
    free(app.cur);
    free(app.prev);
    free(app.rows);

    printf("procmon: exited cleanly after %d refreshes\n", done);
    return 0;
}