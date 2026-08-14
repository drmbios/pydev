#define _POSIX_C_SOURCE 200809L

#include <stdio.h>

#if defined(__linux__)
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_PROCESSES 131072U

static int numeric_name(const char *text) {
    if (!*text) return 0;
    while (*text) if (!isdigit((unsigned char)*text++)) return 0;
    return 1;
}

static int read_status(pid_t pid, char *name, size_t name_size, char *state,
                       long *ppid, unsigned long long *vm_kb, unsigned *threads,
                       uid_t *uid) {
    char path[64], line[512];
    FILE *file;
    int found = 0;
    if (snprintf(path, sizeof path, "/proc/%ld/status", (long)pid) >= (int)sizeof path) return -1;
    file = fopen(path, "r");
    if (!file) return -1;
    name[0] = '\0'; *state = '?'; *ppid = -1; *vm_kb = 0; *threads = 0; *uid = (uid_t)-1;
    while (fgets(line, sizeof line, file)) {
        if (sscanf(line, "Name: %255s", name) == 1) found |= 1;
        else if (sscanf(line, "State: %c", state) == 1) found |= 2;
        else if (sscanf(line, "PPid: %ld", ppid) == 1) found |= 4;
        else if (sscanf(line, "VmRSS: %llu kB", vm_kb) == 1) found |= 8;
        else if (sscanf(line, "Threads: %u", threads) == 1) found |= 16;
        else {
            unsigned long value;
            if (sscanf(line, "Uid: %lu", &value) == 1) { *uid = (uid_t)value; found |= 32; }
        }
    }
    (void)fclose(file);
    (void)name_size;
    return (found & 39) == 39 ? 0 : -1;
}

static unsigned count_fds(pid_t pid) {
    char path[64];
    DIR *directory;
    struct dirent *entry;
    unsigned count = 0;
    if (snprintf(path, sizeof path, "/proc/%ld/fd", (long)pid) >= (int)sizeof path) return 0;
    directory = opendir(path);
    if (!directory) return 0;
    while ((entry = readdir(directory)) != NULL)
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) ++count;
    (void)closedir(directory);
    return count;
}

static int print_process(pid_t pid, int detailed) {
    char name[256], state, user[64], path[64], executable[4096];
    long ppid;
    unsigned long long memory;
    unsigned threads;
    uid_t uid;
    struct passwd *account;
    ssize_t length;
    if (read_status(pid, name, sizeof name, &state, &ppid, &memory, &threads, &uid) != 0) return -1;
    account = getpwuid(uid);
    if (account) snprintf(user, sizeof user, "%s", account->pw_name);
    else snprintf(user, sizeof user, "%lu", (unsigned long)uid);
    printf("%-7ld %-7ld %-12s %c %10llu KB %5u %s\n",
           (long)pid, ppid, user, state, memory, threads, name);
    if (!detailed) return 0;
    if (snprintf(path, sizeof path, "/proc/%ld/exe", (long)pid) < (int)sizeof path) {
        length = readlink(path, executable, sizeof executable - 1U);
        if (length >= 0) { executable[(size_t)length] = '\0'; printf("  executable: %s\n", executable); }
    }
    printf("  open descriptors: %u\n", count_fds(pid));
    printf("  note: environment values are intentionally hidden\n");
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 2) {
        char *end;
        long value;
        errno = 0; value = strtol(argv[1], &end, 10);
        if (errno || *end || value < 1L || value > 2147483647L) { fputs("procexp: invalid PID\n", stderr); return 2; }
        puts("PID     PPID    USER         S     RSS       THR NAME");
        if (print_process((pid_t)value, 1) != 0) {
            fprintf(stderr, "procexp: PID %ld is unavailable\n", value);
            return 1;
        }
        return 0;
    }
    if (argc != 1) { fprintf(stderr, "usage: %s [PID]\n", argv[0]); return 2; }
    {
        DIR *directory = opendir("/proc");
        struct dirent *entry;
        unsigned seen = 0;
        if (!directory) { perror("procexp: /proc"); return 1; }
        puts("PID     PPID    USER         S     RSS       THR NAME");
        while ((entry = readdir(directory)) != NULL && seen < MAX_PROCESSES) {
            if (numeric_name(entry->d_name)) { (void)print_process((pid_t)strtol(entry->d_name, NULL, 10), 0); ++seen; }
        }
        (void)closedir(directory);
    }
    return 0;
}

#else
int main(void) {
    fputs("procexp: supported on Linux only\n", stderr);
    return 1;
}
#endif
