#define _POSIX_C_SOURCE 200809L
#include "pqcommon.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

void pq_clear(void *buffer, size_t length) {
    volatile unsigned char *p = buffer;
    while (length--) *p++ = 0;
}
int pq_algorithm(const char *s) {
    return !strcmp(s,"ML-KEM-768") || !strcmp(s,"ML-KEM-1024") ||
        !strcmp(s,"ML-DSA-65") || !strcmp(s,"ML-DSA-87");
}
static double now(void) {
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC,&t)) return -1;
    return (double)t.tv_sec + (double)t.tv_nsec / 1000000000.0;
}
int pq_run(char *const args[], int input, unsigned char *output, size_t *length) {
    int pipes[2], status = 0, eof = 0, done = 0, result = -1;
    pid_t pid;
    double start = now();
    const char *exe = getenv("PYDEV_OPENSSL");
    *length = 0;
    if (!exe) exe = "openssl";
    else if (*exe != '/') { fputs("PYDEV_OPENSSL must be an absolute executable path\n",stderr); return -1; }
    if (start < 0 || pipe(pipes)) return -1;
    pid = fork();
    if (pid < 0) { close(pipes[0]); close(pipes[1]); return -1; }
    if (!pid) {
        int nullfd = open("/dev/null",O_RDWR);
        struct rlimit cpu = {5,5};
        if (setpgid(0,0) || nullfd < 0 || dup2(input < 0 ? nullfd : input,STDIN_FILENO) < 0 ||
            dup2(pipes[1],STDOUT_FILENO) < 0 || dup2(nullfd,STDERR_FILENO) < 0 ||
            setrlimit(RLIMIT_CPU,&cpu)) _exit(126);
        close(pipes[0]); close(pipes[1]);
        if (nullfd > 2) close(nullfd);
        if (setenv("OPENSSL_CONF","/dev/null",1) || unsetenv("OPENSSL_MODULES") ||
            unsetenv("OPENSSL_ENGINES")) _exit(126);
        execvp(exe,args); _exit(127);
    }
    close(pipes[1]);
    (void)setpgid(pid,pid);
    while (!eof || !done) {
        double current = now();
        if (current < 0 || current - start >= 10.0) goto cleanup;
        if (!eof) {
            struct pollfd p = {pipes[0],POLLIN,0};
            int polled = poll(&p,1,50);
            if (polled < 0) { if (errno == EINTR) continue; goto cleanup; }
            if (polled && (p.revents & (POLLIN|POLLHUP|POLLERR))) {
                unsigned char chunk[4096];
                ssize_t n = read(pipes[0],chunk,sizeof chunk);
                if (n < 0) { if (errno == EINTR) continue; goto cleanup; }
                if (!n) eof = 1;
                else {
                    if ((size_t)n > PQ_OUTPUT - *length) { pq_clear(chunk,sizeof chunk); goto cleanup; }
                    memcpy(output + *length,chunk,(size_t)n); *length += (size_t)n;
                    pq_clear(chunk,sizeof chunk);
                }
            }
        } else { struct timespec pause = {0,50000000}; nanosleep(&pause,NULL); }
        /* Keep an exited child unreaped until its pipe is drained. This also
           keeps its PID reserved if a descendant holds the pipe open. */
        if (eof && !done) {
            pid_t w = waitpid(pid,&status,WNOHANG);
            if (w == pid) done = 1;
            else if (w < 0 && errno != EINTR) goto cleanup;
        }
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) result = 0;
cleanup:
    if (!done || !eof) { (void)kill(-pid,SIGKILL); (void)kill(pid,SIGKILL); }
    if (!done) while (waitpid(pid,&status,0) < 0 && errno == EINTR) {}
    close(pipes[0]);
    if (result) fputs("OpenSSL operation failed, unavailable, timed out, or exceeded output budget\n",stderr);
    return result;
}
