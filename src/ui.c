/* ui.c */
#include "ui.h"
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

#define HEADER_LINES 4
#define FOOTER_LINES 1
#define FIXED_COLS   43

/* state */

void ui_state_init(UiState *s) {
    if (!s) return;
    memset(s, 0, sizeof(*s));
    s->sort_key = SORT_BY_CPU;
    s->descending = 1;
    filter_init(&s->filter);
}

static void reset_view(UiState *s) { s->scroll = 0; }

static void apply_prompt(UiState *s) {
    s->prompt_buf[s->prompt_len] = '\0';
    switch (s->prompt) {
    case UI_PROMPT_NAME:
        snprintf(s->filter.name, sizeof(s->filter.name), "%s", s->prompt_buf);
        break;
    case UI_PROMPT_USER:
        snprintf(s->filter.user, sizeof(s->filter.user), "%s", s->prompt_buf);
        break;
    case UI_PROMPT_STATE:
        s->filter.state = s->prompt_len
            ? (char)toupper((unsigned char)s->prompt_buf[0]) : 0;
        break;
    default: break;
    }
    s->prompt = UI_PROMPT_NONE;
    s->prompt_len = 0;
    s->prompt_buf[0] = '\0';
    reset_view(s);
}

static void open_prompt(UiState *s, UiPrompt p) {
    s->prompt = p;
    s->prompt_len = 0;
    s->prompt_buf[0] = '\0';
}

int ui_handle_key(UiState *s, int key) {
    if (!s) return 0;

    if (s->prompt != UI_PROMPT_NONE) {           /* enter the prompt mode */
        if (key == UI_KEY_ENTER) { apply_prompt(s); return 1; }
        if (key == UI_KEY_ESC) {
            s->prompt = UI_PROMPT_NONE; s->prompt_len = 0; s->prompt_buf[0] = '\0';
            return 1;
        }
        if (key == UI_KEY_BACKSPACE) {
            if (s->prompt_len) s->prompt_buf[--s->prompt_len] = '\0';
            return 1;
        }
        if (key == 3) { s->quit = 1; return 1; } /* Ctrl-C to quit */
        if (key >= 32 && key < 127) {
            size_t cap = s->prompt == UI_PROMPT_STATE ? 1 : sizeof(s->prompt_buf) - 1;
            if (s->prompt_len < cap) {
                s->prompt_buf[s->prompt_len++] = (char)key;
                s->prompt_buf[s->prompt_len] = '\0';
            }
            return 1;
        }
        return 0;
    }

    switch (key) {                               /* enter the normal mode */
    case 'q': case 'Q': case 3:
        s->quit = 1; return 1;
    case 'p': s->sort_key = SORT_BY_PID; s->descending = 0; reset_view(s); return 1;
    case 'c': s->sort_key = SORT_BY_CPU; s->descending = 1; reset_view(s); return 1;
    case 'm': s->sort_key = SORT_BY_MEM; s->descending = 1; reset_view(s); return 1;
    case 'r': s->descending = !s->descending; reset_view(s); return 1;
    case 'n': open_prompt(s, UI_PROMPT_NAME);  return 1;
    case 'u': open_prompt(s, UI_PROMPT_USER);  return 1;
    case 's': open_prompt(s, UI_PROMPT_STATE); return 1;
    case 'x': filter_init(&s->filter); reset_view(s); return 1;
    case UI_KEY_UP:   if (s->scroll) --s->scroll; return 1;
    case UI_KEY_DOWN: ++s->scroll; return 1;     /* clamped at render time */
    case UI_KEY_PGUP: s->scroll = s->scroll > 10 ? s->scroll - 10 : 0; return 1;
    case UI_KEY_PGDN: s->scroll += 10; return 1;
    case UI_KEY_HOME: s->scroll = 0; return 1;
    case UI_KEY_END:  s->scroll = (size_t)-1 / 2; return 1;
    default: return 0;
    }
}

/* rendering */

static void format_uptime(double secs, char *buf, size_t cap) {
    if (secs < 0) { snprintf(buf, cap, "N/A"); return; }
    unsigned long long t = (unsigned long long)secs;
    unsigned long long d = t / 86400, h = t % 86400 / 3600,
                       m = t % 3600 / 60, s = t % 60;
    if (d) snprintf(buf, cap, "%llud %02llu:%02llu:%02llu", d, h, m, s);
    else   snprintf(buf, cap, "%02llu:%02llu:%02llu", h, m, s);
}

static void format_pct(int valid, double v, char *buf, size_t cap) {
    if (valid) snprintf(buf, cap, "%.1f", v);
    else       snprintf(buf, cap, "N/A");
}

/* Prints a maximum of `width` chars */
static void put_clean(FILE *out, const char *s, int width) {
    for (int i = 0; s[i] && i < width; ++i) {
        unsigned char c = (unsigned char)s[i];
        fputc((c >= 32 && c < 127) || c >= 128 ? c : '?', out);
    }
}

static void end_line(FILE *out, int ansi, int last) {
    if (ansi) fputs("\033[K", out);
    if (!last) fputc('\n', out);
}

static const char *sort_label(SortKey k) {
    return k == SORT_BY_PID ? "PID" : k == SORT_BY_CPU ? "CPU%" : "MEM%";
}

int ui_render(FILE *out, UiState *s, const SystemSummary *sys,
              const ProcessRow *rows, size_t n,
              int term_rows, int term_cols, int use_ansi) {
    if (!out || !s || (!rows && n) || term_rows < 1 || term_cols < 1) return -1;

    /* Filter and sort a copy and keep the original untouched */
    ProcessRow *view = malloc((n ? n : 1) * sizeof(*view));
    if (!view) return -1;
    int shown = n ? filter_rows(rows, n, &s->filter, view) : 0;
    if (shown < 0 || sort_rows(view, (size_t)shown, s->sort_key, s->descending)) {
        free(view);
        return -1;
    }

    int visible = term_rows - HEADER_LINES - FOOTER_LINES;
    if (visible < 1) visible = 1;
    size_t max_scroll = (size_t)shown > (size_t)visible ? (size_t)shown - visible : 0;
    if (s->scroll > max_scroll) s->scroll = max_scroll;

    int ansi = use_ansi != 0;
    int w = term_cols;
    char a[32], b[32], c[64], up[48];

    if (ansi) fputs("\033[H", out);

    /* print the title */
    fprintf(out, "%s", ansi ? "\033[1m" : "");
    fputs("Linux Process Monitor", out);
    fprintf(out, "%s", ansi ? "\033[0m" : "");
    end_line(out, ansi, 0);

    /* print the summary */
    format_pct(sys && sys->cpu_valid, sys ? sys->cpu_percent : 0, a, sizeof(a));
    format_pct(sys && sys->mem_valid, sys ? sys->mem_percent : 0, b, sizeof(b));
    if (sys && sys->info) {
        format_uptime(sys->info->uptime_seconds, up, sizeof(up));
        unsigned long long tot = sys->info->memory_total_kb / 1024;
        unsigned long long avail = sys->info->memory_available_kb / 1024;
        unsigned long long used = tot > avail ? tot - avail : 0;
        snprintf(c, sizeof(c), "%llu/%llu MiB", used, tot);
        fprintf(out, "CPU: %s%%  Mem: %s%% (%s)  Uptime: %s  Load: %.2f %.2f %.2f",
                a, b, sys->mem_valid ? c : "N/A", up,
                sys->info->load_average[0], sys->info->load_average[1],
                sys->info->load_average[2]);
    } else {
        fprintf(out, "CPU: %s%%  Mem: %s%%  Uptime: N/A  Load: N/A", a, b);
    }
    end_line(out, ansi, 0);

    /* print the status of sort/filter */
    fprintf(out, "Sort: %s %s | Filter: name=%s user=%s state=%c | Showing %d of %zu",
            sort_label(s->sort_key), s->descending ? "desc" : "asc",
            s->filter.name[0] ? s->filter.name : "-",
            s->filter.user[0] ? s->filter.user : "-",
            s->filter.state ? s->filter.state : '-', shown, n);
    end_line(out, ansi, 0);

    /* add the column header */
    char ind_pid = s->sort_key == SORT_BY_PID ? (s->descending ? 'v' : '^') : ' ';
    char ind_cpu = s->sort_key == SORT_BY_CPU ? (s->descending ? 'v' : '^') : ' ';
    char ind_mem = s->sort_key == SORT_BY_MEM ? (s->descending ? 'v' : '^') : ' ';
    char hp[16], hc[16], hm[16];
    snprintf(hp, sizeof(hp), "PID%c", ind_pid);
    snprintf(hc, sizeof(hc), "CPU%%%c", ind_cpu);
    snprintf(hm, sizeof(hm), "MEM%%%c", ind_mem);
    if (ansi) fputs("\033[7m", out);
    fprintf(out, "%7s %7s %-10s %c %6s %6s %s", hp, "PPID", "USER", 'S', hc, hm, "NAME");
    if (ansi) fputs("\033[0m", out);
    end_line(out, ansi, 0);

    /* add the process rows */
    int name_w = w - FIXED_COLS;
    if (name_w < 8) name_w = 8;
    for (int i = 0; i < visible; ++i) {
        size_t idx = s->scroll + (size_t)i;
        if (idx < (size_t)shown) {
            const Process *p = view[idx].process;
            char user[32], cpu[16], mem[16];
            lookup_username(p->uid, user, sizeof(user));
            format_pct(view[idx].cpu_valid, view[idx].cpu_percent, cpu, sizeof(cpu));
            format_pct(view[idx].mem_valid, view[idx].mem_percent, mem, sizeof(mem));
            fprintf(out, "%7ld %7ld %-10.10s %c %6s %6s ",
                    (long)p->pid, (long)p->ppid, user,
                    p->state >= 32 && p->state < 127 ? p->state : '?', cpu, mem);
            put_clean(out, p->name, name_w);
            end_line(out, ansi, 0);
        } else if (shown == 0 && i == 0) {
            fputs("(no matching processes)", out);
            end_line(out, ansi, 0);
        } else if (ansi) {
            end_line(out, 1, 0);          /* blanking out stale rows */
        }
    }

    /* add a footer that includs a prompt or key help */
    if (s->prompt != UI_PROMPT_NONE) {
        const char *what = s->prompt == UI_PROMPT_NAME ? "name"
                         : s->prompt == UI_PROMPT_USER ? "user/UID" : "state (R,S,D,T,Z,I)";
        fprintf(out, "Filter %s: %s_  [Enter=apply Esc=cancel]", what, s->prompt_buf);
    } else {
        fprintf(out, "%.*s", w > 1 ? w - 1 : 1,
                "q quit  p/c/m sort PID/CPU/MEM  r reverse  n/u/s filter name/user/state  x clear  Up/Down scroll");
    }
    end_line(out, ansi, 1);
    if (ansi) fputs("\033[J", out);
    fflush(out);
    free(view);
    return 0;
}

/* the terminal */

static struct termios g_saved;
static int g_raw_active = 0;

int ui_init(void) {
    if (g_raw_active) return 0;
    if (!isatty(STDIN_FILENO) || tcgetattr(STDIN_FILENO, &g_saved) != 0) return -1;
    struct termios raw = g_saved;
    raw.c_lflag &= ~(tcflag_t)(ICANON | ECHO | ISIG | IEXTEN);
    raw.c_iflag &= ~(tcflag_t)(IXON | ICRNL);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) return -1;
    g_raw_active = 1;
    fputs("\033[?1049h\033[?25l\033[2J", stdout);   /* enter alt screen and hide the cursor */
    fflush(stdout);
    return 0;
}

void ui_shutdown(void) {
    if (!g_raw_active) return;
    fputs("\033[?25h\033[?1049l", stdout);          /* exit alt screen and show the cursor */
    fflush(stdout);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_saved);
    g_raw_active = 0;
}

void ui_terminal_size(int *rows, int *cols) {
    struct winsize ws;
    int r = 24, c = 80;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row && ws.ws_col) {
        r = ws.ws_row; c = ws.ws_col;
    }
    if (rows) *rows = r;
    if (cols) *cols = c;
}

/* read one byte at a time, return -1 on timeout/EINTR, return -2 on EOF or other errors. */
static int read_byte(int timeout_ms) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    struct timeval tv, *tvp = NULL;
    if (timeout_ms >= 0) {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        tvp = &tv;
    }
    int r = select(STDIN_FILENO + 1, &fds, NULL, NULL, tvp);
    if (r < 0) return errno == EINTR ? -1 : -2;
    if (r == 0) return -1;
    unsigned char ch;
    ssize_t n = read(STDIN_FILENO, &ch, 1);
    if (n == 1) return ch;
    if (n < 0 && (errno == EINTR || errno == EAGAIN)) return -1;
    return -2;
}

int ui_read_key(int timeout_ms) {
    int ch = read_byte(timeout_ms);
    if (ch == -1) return UI_KEY_NONE;
    if (ch == -2) return UI_KEY_EOF;
    if (ch == '\r') return UI_KEY_ENTER;
    if (ch == 8) return UI_KEY_BACKSPACE;
    if (ch != 27) return ch;

    /* handle Esc/arrows/Home/End/PgUp/PgDn */
    int b = read_byte(30);
    if (b < 0) return UI_KEY_ESC;
    if (b != '[' && b != 'O') return UI_KEY_NONE;
    int num = 0;
    for (int i = 0; i < 8; ++i) {
        int c = read_byte(30);
        if (c < 0) return UI_KEY_NONE;
        if (isdigit(c)) { num = num * 10 + (c - '0'); continue; }
        if (c == ';') { num = 0; continue; }
        switch (c) {
        case 'A': return UI_KEY_UP;
        case 'B': return UI_KEY_DOWN;
        case 'H': return UI_KEY_HOME;
        case 'F': return UI_KEY_END;
        case '~':
            if (num == 1 || num == 7) return UI_KEY_HOME;
            if (num == 4 || num == 8) return UI_KEY_END;
            if (num == 5) return UI_KEY_PGUP;
            if (num == 6) return UI_KEY_PGDN;
            return UI_KEY_NONE;
        default: return UI_KEY_NONE;
        }
    }
    return UI_KEY_NONE;
}
