#ifndef FILTER_SORT_H
#define FILTER_SORT_H
/*
 * filter_sort.h
 * Each ProcessRow links to a Process object and stores the calculated percentages.
 * A ProcessRow references the Process and no rows are ever deleted or changed.
 * Filtering and sorting don't alter the Process objects.
 */
#include <stddef.h>
#include <sys/types.h>
#include "process.h"

#define FILTER_TEXT_MAX 64

typedef struct {
    const Process *process;  /* must outlive the row */
    double cpu_percent;      /* meaningful only if cpu_valid  */
    int    cpu_valid;        /* if 0, it is unavailable */
    double mem_percent;      /* meaningful only if mem_valid  */
    int    mem_valid;
} ProcessRow;

typedef enum { SORT_BY_PID, SORT_BY_CPU, SORT_BY_MEM } SortKey;

typedef struct {
    char name[FILTER_TEXT_MAX]; /* case-insensitive */
    char user[FILTER_TEXT_MAX]; /* case-insensitive */
    char state;                 /* process state */
} FilterOptions;

/* Remove filters */
void filter_init(FilterOptions *f);

/* returns 1 if any filter is set, and 0 otherwise */
int filter_is_active(const FilterOptions *f);

/* returns 1 if the row passes every active filter, and 0 otherwise or if null arguments */
int filter_matches(const ProcessRow *row, const FilterOptions *f);

/*
 * Copies matching elements from in[0..n) to out (size at least n).
 * Returns the number of elements copied, or -1 on bad parameters.
 */
int filter_rows(const ProcessRow *in, size_t n, const FilterOptions *f,
                ProcessRow *out);

/*
 * Rows are sorted in-place. Ties are broken in ascending order of PID
 * to ensure determinism. In case of CPU/MEM, rows with an unavailable metric 
 * are always put at the end in any direction. 
 * Returns 0 or -1 for bad input. 
 * Note that this is not thread-safe since the file-local state is used in qsort.
 */
int sort_rows(ProcessRow *rows, size_t n, SortKey key, int descending);

/*
 * Convert a UID to a username. This routine returns 0 on success, otherwise stores
 * the numerical UID in buf and returns -1 (the buffer is guaranteed to be valid).
 */
int lookup_username(uid_t uid, char *buf, size_t capacity);

#endif
