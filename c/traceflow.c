#define _POSIX_C_SOURCE 200809L

#include "syscall_names.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__linux__) && defined(__x86_64__)
#include <signal.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_TRACED 4096U

struct traced_process {
    pid_t pid;
    pid_t parent;
    unsigned depth;
    int entering;
    long syscall_number;
};

static struct traced_process processes[MAX_TRACED];
static size_t process_count;

static struct traced_process *find_process(pid_t pid) {
    size_t index;
    for (index = 0; index < process_count; ++index)
        if (processes[index].pid == pid) return &processes[index];
    return NULL;
}

static struct traced_process *add_process(pid_t pid, pid_t parent) {
    struct traced_process *parent_process = find_process(parent);
    struct traced_process *result;
    result = find_process(pid);
    if (result) return result;
    if (process_count >= MAX_TRACED) return NULL;
    result = &processes[process_count++];
    result->pid = pid;
    result->parent = parent;
    result->depth = parent_process ? parent_process->depth + 1U : 0U;
    result->entering = 1;
    result->syscall_number = -1;
    return result;
}

static void remove_process(pid_t pid) {
    size_t index;
    for (index = 0; index < process_count; ++index) {
        if (processes[index].pid == pid) {
            processes[index] = processes[process_count - 1U];
            --process_count;
            return;
        }
    }
}

static void indent(unsigned depth) {
    unsigned index;
    for (index = 0; index < depth && index < 64U; ++index) fputs("  ", stdout);
}

static int resume(pid_t pid, int signal_number) {
    if (ptrace(PTRACE_SYSCALL, pid, NULL, (void *)(long)signal_number) == -1) {
        if (errno == ESRCH) return 0;
        fprintf(stderr, "traceflow: cannot resume %ld: %s\n", (long)pid, strerror(errno));
        return -1;
    }
    return 0;
}

static void report_syscall(pid_t pid, struct traced_process *process) {
    struct user_regs_struct registers;
    if (ptrace(PTRACE_GETREGS, pid, NULL, &registers) == -1) return;
    if (process->entering) {
        process->syscall_number = (long)registers.orig_rax;
        indent(process->depth);
        printf("|- pid=%ld %s(%ld)\n", (long)pid,
               pydev_syscall_name(process->syscall_number), process->syscall_number);
    } else {
        indent(process->depth);
        printf("|  -> %ld\n", (long)registers.rax);
    }
    process->entering = !process->entering;
}

static int trace_loop(pid_t root) {
    int root_status = 1;
    while (process_count > 0U) {
        int status;
        pid_t pid = waitpid(-1, &status, __WALL);
        struct traced_process *process;
        int deliver = 0;
        if (pid == -1) {
            if (errno == EINTR) continue;
            if (errno == ECHILD) break;
            fprintf(stderr, "traceflow: waitpid: %s\n", strerror(errno));
            return 1;
        }
        process = find_process(pid);
        if (!process) process = add_process(pid, root);
        if (!process) {
            fputs("traceflow: process limit reached\n", stderr);
            (void)ptrace(PTRACE_KILL, pid, NULL, NULL);
            continue;
        }
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            indent(process->depth);
            if (WIFEXITED(status)) {
                printf("`- pid=%ld exited=%d\n", (long)pid, WEXITSTATUS(status));
                if (pid == root) root_status = WEXITSTATUS(status);
            } else {
                printf("`- pid=%ld signal=%d\n", (long)pid, WTERMSIG(status));
                if (pid == root) root_status = 128 + WTERMSIG(status);
            }
            remove_process(pid);
            continue;
        }
        if (WIFSTOPPED(status)) {
            int stop_signal = WSTOPSIG(status);
            unsigned event = (unsigned)status >> 16;
            if (stop_signal == (SIGTRAP | 0x80)) {
                report_syscall(pid, process);
            } else if (stop_signal == SIGTRAP && event != 0U) {
                if (event == PTRACE_EVENT_FORK || event == PTRACE_EVENT_VFORK ||
                    event == PTRACE_EVENT_CLONE) {
                    unsigned long child_value = 0;
                    if (ptrace(PTRACE_GETEVENTMSG, pid, NULL, &child_value) == 0) {
                        pid_t child = (pid_t)child_value;
                        struct traced_process *added = add_process(child, pid);
                        indent(process->depth);
                        printf("+- pid=%ld created child=%ld%s\n", (long)pid, (long)child,
                               added ? "" : " (not tracked: limit)");
                    }
                } else if (event == PTRACE_EVENT_EXEC) {
                    indent(process->depth);
                    printf("+- pid=%ld exec\n", (long)pid);
                }
            } else if (stop_signal != SIGSTOP && stop_signal != SIGTRAP) {
                deliver = stop_signal;
            }
        }
        if (resume(pid, deliver) != 0) return 1;
    }
    return root_status;
}

int main(int argc, char **argv) {
    pid_t child;
    int status;
    long options = PTRACE_O_TRACESYSGOOD | PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK |
                   PTRACE_O_TRACECLONE | PTRACE_O_TRACEEXEC | PTRACE_O_TRACEEXIT;
    if (argc < 2) {
        fprintf(stderr, "usage: %s COMMAND [ARG...]\n", argv[0]);
        return 2;
    }
    child = fork();
    if (child == -1) {
        fprintf(stderr, "traceflow: fork: %s\n", strerror(errno));
        return 1;
    }
    if (child == 0) {
        if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) == -1) _exit(126);
        if (raise(SIGSTOP) != 0) _exit(126);
        execvp(argv[1], &argv[1]);
        _exit(errno == ENOENT ? 127 : 126);
    }
    if (waitpid(child, &status, 0) == -1 || !WIFSTOPPED(status)) {
        fputs("traceflow: child did not enter trace stop\n", stderr);
        return 1;
    }
    if (ptrace(PTRACE_SETOPTIONS, child, NULL, (void *)options) == -1) {
        fprintf(stderr, "traceflow: ptrace options: %s\n", strerror(errno));
        (void)kill(child, SIGKILL);
        (void)waitpid(child, NULL, 0);
        return 1;
    }
    (void)add_process(child, 0);
    printf("pid=%ld %s\n", (long)child, argv[1]);
    if (resume(child, 0) != 0) return 1;
    return trace_loop(child);
}

#else
int main(void) {
    fputs("traceflow: supported on Linux x86-64 only\n", stderr);
    return 1;
}
#endif
