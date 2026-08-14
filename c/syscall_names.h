#ifndef PYDEV_SYSCALL_NAMES_H
#define PYDEV_SYSCALL_NAMES_H

#include <stddef.h>

static inline const char *pydev_syscall_name(long number) {
    static const char *const names[] = {
        "read", "write", "open", "close", "stat", "fstat", "lstat", "poll",
        "lseek", "mmap", "mprotect", "munmap", "brk", "rt_sigaction",
        "rt_sigprocmask", "rt_sigreturn", "ioctl", "pread64", "pwrite64",
        "readv", "writev", "access", "pipe", "select", "sched_yield", "mremap",
        "msync", "mincore", "madvise", "shmget", "shmat", "shmctl", "dup",
        "dup2", "pause", "nanosleep", "getitimer", "alarm", "setitimer", "getpid",
        "sendfile", "socket", "connect", "accept", "sendto", "recvfrom", "sendmsg",
        "recvmsg", "shutdown", "bind", "listen", "getsockname", "getpeername",
        "socketpair", "setsockopt", "getsockopt", "clone", "fork", "vfork",
        "execve", "exit", "wait4", "kill", "uname", "semget", "semop", "semctl",
        "shmdt", "msgget", "msgsnd", "msgrcv", "msgctl", "fcntl", "flock",
        "fsync", "fdatasync", "truncate", "ftruncate", "getdents", "getcwd",
        "chdir", "fchdir", "rename", "mkdir", "rmdir", "creat", "link", "unlink",
        "symlink", "readlink", "chmod", "fchmod", "chown", "fchown", "lchown",
        "umask", "gettimeofday", "getrlimit", "getrusage", "sysinfo"
    };
    if (number < 0 || (size_t)number >= sizeof names / sizeof names[0]) return "unknown";
    return names[number];
}

#endif
