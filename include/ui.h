#ifndef UI_H
#define UI_H
/*
 * ui.h
 * Integration (main loop of Task 4), each time the screen refreshes:
 *     ui_render(stdout, &state, &summary, rows, n, rows_h, cols_w, 1);
 *     key = ui_read_key(interval_ms);
 *     if (key != UI_KEY_NONE) ui_handle_key(&state, key);
 *     if (state.quit || key == UI_KEY_EOF) break;
 * The caller provides *unfiltered, unsorted* rows; ui_render filters/sorts its own
 * copy, hence the caller's data never gets modified.
 */
#include <stdio.h>
#include "filter_sort.h"
#include "process.h"

/* Keys returned by ui_read_key() */
#define UI_KEY_NONE      (-1)
#define UI_KEY_EOF       (-2)
#define UI_KEY_ESC       27
#define UI_KEY_ENTER     '\n'
#define UI_KEY_BACKSPACE 127
#define UI_KEY_UP        1000
#define UI_KEY_DOWN      1001
#define UI_KEY_PGUP      1002
#define UI_KEY_PGDN      1003
#define UI_KEY_HOME      1004
#define UI_KEY_END       1005

typedef enum {
    UI_PROMPT_NONE, UI_PROMPT_NAME, UI_PROMPT_USER, UI_PROMPT_STATE
} UiPrompt;

typedef struct {
    SortKey       sort_key;
    int           descending;
    FilterOptions filter;
    UiPrompt      prompt;
    char          prompt_buf[FILTER_TEXT_MAX];
    size_t        prompt_len;
    size_t        scroll;
    int           quit;
} UiState;

/* System-wide values for the summary */
typedef struct {
    const SystemInfo *info;
    double cpu_percent;  int cpu_valid;
    double mem_percent;  int mem_valid;
} SystemSummary;

void ui_state_init(UiState *s);

/*
 * Perform a single key press. Returns 1 if the screen needs to be redraw and 0.
 * Standard mode: q quits | p/c/m sorts by PID/CPU/MEM percent | r reverses |
 *   n/u/s enters name/user/state filtering prompt | x clears all filters |
 *   Up/Down/PgUp/PgDn/Home/End scrolls.
 * Prompt mode: text sets the filter; Enter commits, Escape cancels,
 *   Backspace erases (Enter with no text clears this filter).
 */
int ui_handle_key(UiState *s, int key);

/*
 * Render a complete frame into `out`. rows/n is the unfiltered process listing.
 * term_rows/term_cols provide the dimensions of the frame; use_ansi controls cursor
 * movement, highlighting, and line clearing (0 provides non-color text output for tests/logging).
 * Returns 0 if no error, or -1 for errors in arguments or memory allocation.
 */
int ui_render(FILE *out, UiState *s, const SystemSummary *sys,
              const ProcessRow *rows, size_t n,
              int term_rows, int term_cols, int use_ansi);

/* Put the terminal in raw mode. Returns 0 if no error, or -1 if the input is not a terminal.
 * Ctrl-C is treated as the quit key.
 */
int ui_init(void);

/* Restore the terminal */
void ui_shutdown(void);

/* Wait for timeout_ms for a key (note that a negative input can cause it to wait forever) */
int ui_read_key(int timeout_ms);

/* The terminal size */
void ui_terminal_size(int *rows, int *cols);

#endif
