#include "common.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_SYSCALL_NAME 127U
#define DESCRIPTION_SIZE 256U

static const char *const x86_64_core[] = {
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

static const char *architecture_name(void) {
#if defined(__linux__) && defined(__x86_64__)
    return "linux-x86_64";
#else
    return "linux-x86_64-reference";
#endif
}

static const char *curated_description(long number) {
    switch (number) {
        case 0: return "Read bytes from a file descriptor";
        case 1: return "Write bytes to a file descriptor";
        case 2: return "Open a file";
        case 3: return "Close a file descriptor";
        case 4: return "Get file information by path";
        case 5: return "Get file information by descriptor";
        case 7: return "Wait for activity on file descriptors";
        case 8: return "Move a file descriptor's read or write position";
        case 9: return "Map files or memory into a process";
        case 10: return "Change memory access protections";
        case 11: return "Remove a memory mapping";
        case 12: return "Change the process data-segment boundary";
        case 16: return "Control a device through a file descriptor";
        case 21: return "Check access permissions for a path";
        case 22: return "Create a one-way communication pipe";
        case 24: return "Yield the processor to another task";
        case 32: return "Duplicate a file descriptor";
        case 35: return "Pause execution for a specified time";
        case 39: return "Get the current process identifier";
        case 41: return "Create a network socket";
        case 42: return "Connect a socket to a remote address";
        case 43: return "Accept an incoming socket connection";
        case 49: return "Bind a socket to a local address";
        case 50: return "Listen for incoming socket connections";
        case 56: return "Create a process or thread";
        case 57: return "Create a child process";
        case 59: return "Replace the current process with a program";
        case 60: return "Terminate the current process";
        case 61: return "Wait for a process to change state";
        case 62: return "Send a signal to a process";
        case 63: return "Get operating-system and machine information";
        case 72: return "Get or change file-descriptor properties";
        case 74: return "Flush file data and metadata to storage";
        case 78: return "Read directory entries";
        case 79: return "Get the current working directory";
        case 80: return "Change the current working directory";
        case 82: return "Rename a filesystem entry";
        case 83: return "Create a directory";
        case 84: return "Remove an empty directory";
        case 87: return "Remove a filesystem name";
        case 89: return "Read the target of a symbolic link";
        case 90: return "Change file permission bits";
        case 92: return "Change file ownership";
        case 95: return "Set the process file-creation permission mask";
        default: return NULL;
    }
}

static void generic_description(const char *name, char output[DESCRIPTION_SIZE]) {
    size_t source = 0;
    size_t target = 0;
    const char prefix[] = "Linux kernel operation: ";

    memcpy(output, prefix, sizeof prefix - 1U);
    target = sizeof prefix - 1U;
    while (name[source] != '\0' && target + 1U < DESCRIPTION_SIZE) {
        unsigned char byte = (unsigned char)name[source++];
        if (byte == '_') byte = ' ';
        if (target == sizeof prefix - 1U) byte = (unsigned char)toupper(byte);
        output[target++] = (char)byte;
    }
    output[target] = '\0';
}

static int header_lookup(const char *table, long wanted, char name[MAX_SYSCALL_NAME + 1U]) {
    const char *line = table;

    while (*line != '\0') {
        char macro[MAX_SYSCALL_NAME + 6U];
        long number;
        if (sscanf(line, "#define __NR_%132s %ld", macro, &number) == 2 &&
            number == wanted) {
            size_t length = strcspn(macro, " \t\r\n");
            if (length > MAX_SYSCALL_NAME) return -1;
            memcpy(name, macro, length);
            name[length] = '\0';
            return 1;
        }
        line = strchr(line, '\n');
        if (!line) break;
        ++line;
    }
    return 0;
}

static const char *find_default_table(void) {
    static const char *const paths[] = {
        "/usr/include/x86_64-linux-gnu/asm/unistd_64.h",
        "/usr/include/asm/unistd_64.h"
    };
    size_t index;
    for (index = 0; index < sizeof paths / sizeof paths[0]; ++index)
        if (access(paths[index], R_OK) == 0) return paths[index];
    return NULL;
}

static int print_lookup(long number, const char *table) {
    char name[MAX_SYSCALL_NAME + 1U];
    char generated[DESCRIPTION_SIZE];
    const char *description;
    int found = table ? header_lookup(table, number, name) : 0;

    if (found < 0) {
        fputs("syscallx: syscall name exceeds internal limit\n", stderr);
        return 1;
    }
    if (!found && number >= 0 && (size_t)number < sizeof x86_64_core / sizeof x86_64_core[0]) {
        snprintf(name, sizeof name, "%s", x86_64_core[number]);
        found = 1;
    }
    if (!found) {
        fprintf(stderr, "syscallx: syscall %ld is unknown for %s\n", number, architecture_name());
        return 1;
    }
    description = curated_description(number);
    if (!description) {
        generic_description(name, generated);
        description = generated;
    }
    printf("%ld [%s]\n  name: %s\n  meaning: %s\n", number, architecture_name(), name, description);
    return 0;
}

int main(int argc, char **argv) {
    const char *table_path = NULL;
    char *table = NULL;
    size_t table_length = 0;
    int first_number = 1;
    int index;
    int status = 0;

    if (argc >= 3 && strcmp(argv[1], "--table") == 0) {
        table_path = argv[2];
        first_number = 3;
    } else {
        table_path = find_default_table();
    }
    if (first_number >= argc) {
        fprintf(stderr, "usage: %s [--table LINUX_UNISTD_HEADER] NUMBER...\n", argv[0]);
        return 2;
    }
    if (table_path && read_file_bounded(table_path, &table, &table_length) != 0) return 1;
    (void)table_length;

    for (index = first_number; index < argc; ++index) {
        long number;
        if (parse_long(argv[index], 0, 1048575, &number) != 0) {
            fprintf(stderr, "syscallx: invalid syscall number: %s\n", argv[index]);
            status = 2;
            continue;
        }
        if (print_lookup(number, table) != 0 && status == 0) status = 1;
    }
    free(table);
    return status;
}
