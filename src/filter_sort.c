/* filter_sort.c */
#include "filter_sort.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <pwd.h>

int lookup_username(uid_t uid, char *buf, size_t capacity) {
    if (!buf || capacity == 0) return -1;
    struct passwd *pw = getpwuid(uid);
    if (pw && pw->pw_name && pw->pw_name[0]) {
        snprintf(buf, capacity, "%s", pw->pw_name);
        return 0;
    }
    snprintf(buf, capacity, "%lu", (unsigned long)uid);
    return -1;
}

void filter_init(FilterOptions *f) {
    if (f) memset(f, 0, sizeof(*f));
}

int filter_is_active(const FilterOptions *f) {
    return f && (f->name[0] || f->user[0] || f->state);
}

/* Case-insensitive substring search */
static int contains_icase(const char *hay, const char *needle) {
    size_t nlen = strlen(needle);
    if (nlen == 0) return 1;
    for (; *hay; ++hay) {
        size_t i = 0;
        while (i < nlen && hay[i] &&
               tolower((unsigned char)hay[i]) == tolower((unsigned char)needle[i]))
            ++i;
        if (i == nlen) return 1;
    }
    return 0;
}

static int all_digits(const char *s) {
    if (!*s) return 0;
    for (; *s; ++s) if (!isdigit((unsigned char)*s)) return 0;
    return 1;
}

int filter_matches(const ProcessRow *row, const FilterOptions *f) {
    if (!row || !row->process || !f) return 0;
    const Process *p = row->process;

    if (f->name[0] && !contains_icase(p->name, f->name)) return 0;

    if (f->state &&
        toupper((unsigned char)p->state) != toupper((unsigned char)f->state))
        return 0;

    if (f->user[0]) {
        if (all_digits(f->user)) {
            errno = 0;
            char *end;
            unsigned long want = strtoul(f->user, &end, 10);
            if (errno || *end || (unsigned long)p->uid != want) return 0;
        } else {
            char user[64];
            lookup_username(p->uid, user, sizeof(user));
            if (!contains_icase(user, f->user)) return 0;
        }
    }
    return 1;
}

int filter_rows(const ProcessRow *in, size_t n, const FilterOptions *f,
                ProcessRow *out) {
    if (!f || !out || (!in && n)) return -1;
    size_t count = 0;
    for (size_t i = 0; i < n; ++i)
        if (filter_matches(&in[i], f)) out[count++] = in[i];
    return (int)count;
}

/* sorting */

static SortKey g_key;
static int g_desc;

static int cmp_pid(const ProcessRow *a, const ProcessRow *b) {
    if (a->process->pid < b->process->pid) return -1;
    if (a->process->pid > b->process->pid) return 1;
    return 0;
}

static int row_cmp(const void *pa, const void *pb) {
    const ProcessRow *a = pa, *b = pb;
    int c = 0;

    if (g_key == SORT_BY_PID) {
        c = cmp_pid(a, b);
        return g_desc ? -c : c;
    }

    int av = g_key == SORT_BY_CPU ? a->cpu_valid : a->mem_valid;
    int bv = g_key == SORT_BY_CPU ? b->cpu_valid : b->mem_valid;
    if (!av || !bv) {
        if (av != bv) return av ? -1 : 1;
        return cmp_pid(a, b);
    }
    double x = g_key == SORT_BY_CPU ? a->cpu_percent : a->mem_percent;
    double y = g_key == SORT_BY_CPU ? b->cpu_percent : b->mem_percent;
    if (x < y) c = -1; else if (x > y) c = 1;
    if (c) return g_desc ? -c : c;
    return cmp_pid(a, b);
}

int sort_rows(ProcessRow *rows, size_t n, SortKey key, int descending) {
    if (!rows && n) return -1;
    if (key != SORT_BY_PID && key != SORT_BY_CPU && key != SORT_BY_MEM) return -1;
    for (size_t i = 0; i < n; ++i) if (!rows[i].process) return -1;
    g_key = key;
    g_desc = descending != 0;
    if (n > 1) qsort(rows, n, sizeof(*rows), row_cmp);
    return 0;
}
