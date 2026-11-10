/*
 * tests for the error helpers and the refresh loop, using fake (mock)
 * callbacks instead of the real monitor. run with: make test
 */
#define _POSIX_C_SOURCE 200809L
#include "collector.h"
#include "error_handler.h"
#include "refresh.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

static int passed = 0, failed = 0;

#define CHECK(cond, name) do { \
    if (cond) { printf("PASS: %s\n", name); passed++; } \
    else      { printf("FAIL: %s\n", name); failed++; } } while (0)

/* ---- fake callbacks ---- */

typedef struct {
    int calls;
    int stop_at;    /* return 1 on this call (0 = never) */
    int signal_at;  /* send ourselves SIGTERM on this call */
    long long start;
} Mock;

static int mock_tick(void *ctx) {
    Mock *m = ctx;
    if (m->calls == 0) m->start = refresh_now_ms();
    m->calls++;
    printf("    mock tick %d at +%lld ms\n", m->calls, refresh_now_ms() - m->start);
    if (m->signal_at == m->calls) raise(SIGTERM);
    return m->stop_at == m->calls;
}

static int mock_wait_stop(void *ctx, int ms) {
    (void)ctx; (void)ms;
    return 1;   /* act like the user pressed q */
}




int main(void) {
    int ms = 0;

    printf("--- Error helpers ---\n");
    CHECK(err_from_errno(ENOENT) == ERR_NOT_FOUND, "ENOENT -> NOT_FOUND");
    CHECK(err_from_errno(EACCES) == ERR_PERMISSION, "EACCES -> PERMISSION");
    CHECK(err_parse_interval("2", &ms) == ERR_OK && ms == 2000, "\"2\" -> 2000 ms");
    CHECK(err_parse_interval("0.5", &ms) == ERR_OK && ms == 500, "\"0.5\" -> 500 ms");
    CHECK(err_parse_interval("abc", &ms) == ERR_INVALID_INPUT, "\"abc\" rejected");
    CHECK(err_parse_interval("2x", &ms) == ERR_INVALID_INPUT, "\"2x\" rejected");
    CHECK(err_parse_interval("0", &ms) == ERR_INVALID_INPUT, "\"0\" rejected");
    err_report(ERR_PERMISSION, "example message");   /* just to see what it looks like */

    /* reading a process that already ended should give NOT_FOUND, not crash */
    pid_t child = fork();
    if (child == 0) _exit(0);
    waitpid(child, NULL, 0);
    Process p;
    errno = 0;
    int r = collect_process(child, &p);
    CHECK(r == -1 && err_from_errno(errno) == ERR_NOT_FOUND,
          "reading an exited process -> NOT_FOUND");

    printf("\n--- Refresh loop (mock callbacks) ---\n");
    Mock m1 = {0};
    int n = refresh_run(200, 5, mock_tick, NULL, &m1);
    CHECK(n == 5 && m1.calls == 5, "5 refreshes at 200 ms");

    Mock m2 = {0};
    m2.stop_at = 3;
    n = refresh_run(100, 0, mock_tick, NULL, &m2);
    CHECK(n == 2 && m2.calls == 3, "tick returning 1 stops the loop (failed tick not counted)");

    Mock m3 = {0};
    CHECK(refresh_run(100, 0, mock_tick, mock_wait_stop, &m3) == 1, "wait returning 1 (key q) stops the loop");

    CHECK(refresh_run(50, 1, mock_tick, NULL, &m3) == -1, "interval below 100 ms rejected");

    refresh_install_signal_handlers();
    Mock m4 = {0};
    m4.signal_at = 2;
    n = refresh_run(100, 0, mock_tick, NULL, &m4);
    CHECK(n == 2 && refresh_should_stop(), "SIGTERM stops the loop cleanly (program still running)");

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}