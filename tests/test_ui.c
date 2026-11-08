/*
 * test_ui.c
 *   ./test_ui               run asserts, then print a sample frame
 *   ./test_ui --snapshot    print one plain-text frame for screenshots
 *   ./test_ui --interactive run the live UI on the mock list (press q to quit)
 */
#define _GNU_SOURCE
#include "ui.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define N 8

static Process procs[N];
static ProcessRow rows[N];
static SystemInfo sys_info;

static void set(int i, pid_t pid, pid_t ppid, const char *name, char state,
                uid_t uid, double cpu, int cpu_ok, double mem, int mem_ok) {
    memset(&procs[i], 0, sizeof(procs[i]));
    procs[i].pid = pid; procs[i].ppid = ppid; procs[i].state = state;
    procs[i].uid = uid;
    snprintf(procs[i].name, sizeof(procs[i].name), "%s", name);
    rows[i].process = &procs[i];
    rows[i].cpu_percent = cpu; rows[i].cpu_valid = cpu_ok;
    rows[i].mem_percent = mem; rows[i].mem_valid = mem_ok;
}

static void build_mock(void) {
    /*  idx pid  ppid name                  st uid   cpu   ok  mem   ok */
    set(0,   1,    0, "systemd",             'S',    0,  0.0, 1,  0.3, 1);
    set(1, 412,    1, "sshd",                'S',    0,  0.1, 1,  0.2, 1);
    set(2, 980,    1, "bash",                'S', 1000,  0.0, 1,  0.1, 1);
    set(3, 1234, 980, "my (test) process",   'R', 1000, 42.5, 1,  1.5, 1);
    set(4, 2001, 980, "firefox",             'S', 1000, 17.3, 1, 12.8, 1);
    set(5, 2002,   2, "kworker/0:1",         'I',    0,  0.0, 0,  0.0, 0);
    set(6, 3100, 980, "Firefox-Web",         'D', 1000, 17.3, 1,  9.9, 1);
    set(7, 4000,   1, "zombie\nproc",        'Z', 1000,  0.0, 0,  0.0, 0);
    memset(&sys_info, 0, sizeof(sys_info));
    sys_info.memory_total_kb = 8u * 1024 * 1024;
    sys_info.memory_available_kb = 3u * 1024 * 1024;
    sys_info.uptime_seconds = 93784.0;
    sys_info.load_average[0] = 0.52;
    sys_info.load_average[1] = 0.40;
    sys_info.load_average[2] = 0.31;
}

static void pids_of(const ProcessRow *r, int n, long *out) {
    for (int i = 0; i < n; ++i) out[i] = (long)r[i].process->pid;
}

static void sort_tests(void) {
    ProcessRow r[N];
    long p[N];

    memcpy(r, rows, sizeof(rows));
    assert(sort_rows(r, N, SORT_BY_PID, 0) == 0);
    pids_of(r, N, p);
    long by_pid[N] = {1, 412, 980, 1234, 2001, 2002, 3100, 4000};
    assert(memcmp(p, by_pid, sizeof(by_pid)) == 0);

    assert(sort_rows(r, N, SORT_BY_PID, 1) == 0);
    assert(r[0].process->pid == 4000 && r[N - 1].process->pid == 1);

    memcpy(r, rows, sizeof(rows));
    assert(sort_rows(r, N, SORT_BY_CPU, 1) == 0);
    pids_of(r, N, p);
    long by_cpu[N] = {1234, 2001, 3100, 412, 1, 980, 2002, 4000};
    assert(memcmp(p, by_cpu, sizeof(by_cpu)) == 0);

    assert(sort_rows(r, N, SORT_BY_CPU, 0) == 0);
    assert(r[N - 2].process->pid == 2002 && r[N - 1].process->pid == 4000);
    assert(r[0].cpu_valid && r[N - 3].cpu_valid);

    memcpy(r, rows, sizeof(rows));
    assert(sort_rows(r, N, SORT_BY_MEM, 1) == 0);
    assert(r[0].process->pid == 2001 && r[1].process->pid == 3100 &&
           r[2].process->pid == 1234);

    assert(rows[0].process->pid == 1 && rows[7].process->pid == 4000);
    assert(sort_rows(NULL, 3, SORT_BY_PID, 0) == -1);
    assert(sort_rows(r, 0, SORT_BY_PID, 0) == 0);
    puts("Sorting tests passed.");
}

static void filter_tests(void) {
    ProcessRow out[N];
    FilterOptions f;
    filter_init(&f);
    assert(!filter_is_active(&f));
    assert(filter_rows(rows, N, &f, out) == N);

    strcpy(f.name, "FIREFOX");
    assert(filter_is_active(&f));
    assert(filter_rows(rows, N, &f, out) == 2);
    assert(out[0].process->pid == 2001 && out[1].process->pid == 3100);

    filter_init(&f); strcpy(f.name, "(test)");
    assert(filter_rows(rows, N, &f, out) == 1 && out[0].process->pid == 1234);

    filter_init(&f); f.state = 'r';
    assert(filter_rows(rows, N, &f, out) == 1 && out[0].process->pid == 1234);

    filter_init(&f); strcpy(f.user, "1000");
    assert(filter_rows(rows, N, &f, out) == 5);
    filter_init(&f); strcpy(f.user, "0");
    assert(filter_rows(rows, N, &f, out) == 3);
    filter_init(&f); strcpy(f.user, "root");
    assert(filter_rows(rows, N, &f, out) == 3);

    filter_init(&f); strcpy(f.name, "firefox"); strcpy(f.user, "1000"); f.state = 'D';
    assert(filter_rows(rows, N, &f, out) == 1 && out[0].process->pid == 3100);

    filter_init(&f); strcpy(f.name, "no-such-process");
    assert(filter_rows(rows, N, &f, out) == 0);
    filter_init(&f); strcpy(f.user, "99999999999999999999");
    assert(filter_rows(rows, N, &f, out) == 0);
    assert(filter_rows(NULL, 1, &f, out) == -1);
    puts("Filtering tests passed.");
}

static void key_tests(void) {
    UiState s;
    ui_state_init(&s);
    assert(s.sort_key == SORT_BY_CPU && s.descending);
    assert(ui_handle_key(&s, 'p') && s.sort_key == SORT_BY_PID && !s.descending);
    assert(ui_handle_key(&s, 'm') && s.sort_key == SORT_BY_MEM && s.descending);
    assert(ui_handle_key(&s, 'r') && !s.descending);
    assert(!ui_handle_key(&s, 'z'));

    ui_handle_key(&s, 'n');
    assert(s.prompt == UI_PROMPT_NAME);
    ui_handle_key(&s, 'b'); ui_handle_key(&s, 'x'); ui_handle_key(&s, 'a');
    ui_handle_key(&s, UI_KEY_BACKSPACE);
    ui_handle_key(&s, 'a'); ui_handle_key(&s, 's');
    assert(strcmp(s.prompt_buf, "bxas") == 0 && !s.quit);
    ui_handle_key(&s, UI_KEY_ESC);
    assert(s.prompt == UI_PROMPT_NONE && s.filter.name[0] == '\0');

    ui_handle_key(&s, 'n');
    ui_handle_key(&s, 'b'); ui_handle_key(&s, 'a');
    ui_handle_key(&s, UI_KEY_ENTER);
    assert(s.prompt == UI_PROMPT_NONE && strcmp(s.filter.name, "ba") == 0);

    ui_handle_key(&s, 's'); ui_handle_key(&s, 'r'); ui_handle_key(&s, UI_KEY_ENTER);
    assert(s.filter.state == 'R');
    ui_handle_key(&s, 'u'); ui_handle_key(&s, '1'); ui_handle_key(&s, UI_KEY_ENTER);
    assert(strcmp(s.filter.user, "1") == 0);
    ui_handle_key(&s, 'x');
    assert(!filter_is_active(&s.filter));

    ui_handle_key(&s, UI_KEY_DOWN); ui_handle_key(&s, UI_KEY_DOWN);
    assert(s.scroll == 2);
    ui_handle_key(&s, UI_KEY_HOME); assert(s.scroll == 0);
    ui_handle_key(&s, UI_KEY_UP);   assert(s.scroll == 0);

    assert(!s.quit);
    ui_handle_key(&s, 'q');
    assert(s.quit);
    puts("Keyboard handling tests passed.");
}

static char *render_to_string(UiState *s, SystemSummary *sum, int rows_h, int cols_w) {
    char *buf = NULL; size_t len = 0;
    FILE *f = open_memstream(&buf, &len);
    assert(f);
    assert(ui_render(f, s, sum, rows, N, rows_h, cols_w, 0) == 0);
    fclose(f);
    return buf;
}

static void render_tests(void) {
    SystemSummary sum = { &sys_info, 12.34, 1, 62.5, 1 };
    UiState s;
    ui_state_init(&s);

    char *out = render_to_string(&s, &sum, 24, 100);
    assert(strstr(out, "CPU: 12.3%"));
    assert(strstr(out, "Mem: 62.5% (5120/8192 MiB)"));
    assert(strstr(out, "Uptime: 1d 02:03:04"));
    assert(strstr(out, "Load: 0.52 0.40 0.31"));
    assert(strstr(out, "Showing 8 of 8"));
    assert(strstr(out, "my (test) process"));
    assert(strstr(out, "zombie?proc"));
    assert(strstr(out, "N/A"));
    assert(strstr(out, "CPU%v"));
    assert(strstr(out, "42.5"));
    assert(strstr(out, "q quit"));
    assert(strstr(out, "\033") == NULL);
    assert(strstr(out, "my (test) process") < strstr(out, "sshd"));
    free(out);

    strcpy(s.filter.name, "fire");
    out = render_to_string(&s, &sum, 24, 100);
    assert(strstr(out, "Showing 2 of 8") && strstr(out, "firefox") && !strstr(out, "sshd"));
    free(out);

    strcpy(s.filter.name, "nothing-matches");
    out = render_to_string(&s, &sum, 24, 100);
    assert(strstr(out, "(no matching processes)"));
    free(out);

    filter_init(&s.filter);
    s.scroll = 9999;
    out = render_to_string(&s, &sum, 8, 40);
    assert(s.scroll <= 8);
    free(out);

    SystemSummary none = { NULL, 0, 0, 0, 0 };
    out = render_to_string(&s, &none, 24, 100);
    assert(strstr(out, "Uptime: N/A"));
    free(out);

    assert(rows[0].process->pid == 1 && rows[3].process->pid == 1234);
    assert(ui_render(NULL, &s, &sum, rows, N, 24, 80, 0) == -1);
    puts("Rendering tests passed.");
}

static void print_frame(int use_ansi, int h, int w) {
    SystemSummary sum = { &sys_info, 12.34, 1, 62.5, 1 };
    UiState s;
    ui_state_init(&s);
    ui_render(stdout, &s, &sum, rows, N, h, w, use_ansi);
    putchar('\n');
}

static void interactive(void) {
    SystemSummary sum = { &sys_info, 12.34, 1, 62.5, 1 };
    UiState s;
    ui_state_init(&s);
    if (ui_init() != 0) { fputs("stdin is not a terminal\n", stderr); return; }
    atexit(ui_shutdown);
    for (;;) {
        int h, w;
        ui_terminal_size(&h, &w);
        ui_render(stdout, &s, &sum, rows, N, h, w, 1);
        int key = ui_read_key(1000);
        if (key == UI_KEY_EOF) break;
        if (key != UI_KEY_NONE) ui_handle_key(&s, key);
        if (s.quit) break;
    }
    ui_shutdown();
}

int main(int argc, char **argv) {
    build_mock();
    if (argc > 1 && strcmp(argv[1], "--snapshot") == 0) { print_frame(0, 20, 100); return 0; }
    if (argc > 1 && strcmp(argv[1], "--interactive") == 0) { interactive(); return 0; }

    sort_tests();
    filter_tests();
    key_tests();
    render_tests();
    puts("\nSample frame (mock data, sorted by CPU% descending):\n");
    print_frame(0, 20, 100);
    puts("\nTask 3 tests passed.");
    return 0;
}
