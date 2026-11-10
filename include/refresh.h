#ifndef REFRESH_H
#define REFRESH_H
/* the refresh loop + making Ctrl+C / kill exit cleanly */

/* does one refresh. return 0 to keep going, anything else to stop */
typedef int (*RefreshTick)(void *ctx);

/* runs between refreshes and waits ms milliseconds (main.c reads keys here).
 * return 0 to keep going, anything else to stop */
typedef int (*RefreshWait)(void *ctx, int ms);

/* so Ctrl+C and kill stop the loop nicely instead of killing us on the spot */
void refresh_install_signal_handlers(void);

/* becomes 1 after Ctrl+C or kill */
int refresh_should_stop(void);

/* current time in ms (this clock never goes backwards) */
long long refresh_now_ms(void);

/* calls tick every interval_ms. stops when tick or wait returns non-zero,
 * after max_ticks refreshes (0 = forever), or when Ctrl+C / kill arrives.
 * if wait is NULL it just sleeps in between.
 * returns how many refreshes happened (-1 if the arguments are bad) */
int refresh_run(int interval_ms, int max_ticks,
                RefreshTick tick, RefreshWait wait, void *ctx);

#endif