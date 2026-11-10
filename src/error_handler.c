/* error helpers shared by the whole program */
#include "error_handler.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

ErrorCode err_from_errno(int errnum) {
    switch (errnum) {
    case ENOENT:
    case ESRCH:  return ERR_NOT_FOUND;   /* process ended while we were reading it */
    case EACCES:
    case EPERM:  return ERR_PERMISSION;
    case EINVAL: return ERR_INVALID_INPUT;
    default:     return ERR_IO;
    }
}

const char *err_to_string(ErrorCode code) {
    switch (code) {
    case ERR_OK:            return "no error";
    case ERR_NOT_FOUND:     return "not found";
    case ERR_PERMISSION:    return "permission denied";
    case ERR_INVALID_INPUT: return "invalid input";
    case ERR_IO:            return "input/output error";
    }
    return "unknown error";
}




void err_report(ErrorCode code, const char *context) {
    fprintf(stderr, "procmon: %s: %s\n", context, err_to_string(code));
}

ErrorCode err_parse_interval(const char *text, int *out_ms) {
    if (text == NULL || out_ms == NULL) return ERR_INVALID_INPUT;

    char *end = NULL;
    errno = 0;
    double seconds = strtod(text, &end);

    /* if end didn't move, there was no number at all ("abc").
     * if anything is left after the number, it's junk ("2x") */
    if (errno != 0 || end == text || *end != '\0') return ERR_INVALID_INPUT;
    if (!(seconds >= 0.1 && seconds <= 60.0)) return ERR_INVALID_INPUT;

    *out_ms = (int)(seconds * 1000.0 + 0.5);
    return ERR_OK;
}











