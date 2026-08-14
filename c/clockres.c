#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static int show_clock(const char *name, clockid_t identifier) {
    struct timespec resolution;
    if (clock_getres(identifier, &resolution) != 0) {
        printf("%-12s unavailable (%s)\n", name, strerror(errno));
        return 1;
    }
    printf("%-12s %lld.%09ld seconds\n", name, (long long)resolution.tv_sec,
           resolution.tv_nsec);
    return 0;
}

int main(void) {
    int failures = 0;
    failures += show_clock("realtime", CLOCK_REALTIME);
    failures += show_clock("monotonic", CLOCK_MONOTONIC);
#ifdef CLOCK_PROCESS_CPUTIME_ID
    failures += show_clock("process-cpu", CLOCK_PROCESS_CPUTIME_ID);
#endif
#ifdef CLOCK_THREAD_CPUTIME_ID
    failures += show_clock("thread-cpu", CLOCK_THREAD_CPUTIME_ID);
#endif
    return failures == 0 ? 0 : 1;
}
