#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H
/* error codes and small helpers that every part of the program can use */

typedef enum {
    ERR_OK = 0,
    ERR_NOT_FOUND,      /* the file or process is gone */
    ERR_PERMISSION,     /* we're not allowed to read it */
    ERR_INVALID_INPUT,  /* the user typed something wrong */
    ERR_IO              /* anything else */
} ErrorCode;

/* turns errno (e.g. after fopen fails) into one of our codes */
ErrorCode err_from_errno(int errnum);

/* text version of a code, for printing */
const char *err_to_string(ErrorCode code);

/* prints something like "procmon: reading /proc: not found" to stderr */
void err_report(ErrorCode code, const char *context);

/* reads the -d value (in seconds) and gives it back in milliseconds.
 * only 0.1 to 60 is accepted, so "abc", "0" or "2x" give ERR_INVALID_INPUT */
ErrorCode err_parse_interval(const char *text, int *out_ms);

#endif