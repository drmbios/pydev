#define _POSIX_C_SOURCE 200809L

#include <stdio.h>

#if defined(__linux__)
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utmpx.h>

#define MAX_PROCESSES 131072U

static int numeric_name(const char *text) {
    if (!*text) return 0;
    while (*text) if (!isdigit((unsigned char)*text++)) return 0;
    return 1;
}

static int process_uid(pid_t pid, uid_t *uid) {
    char path[64];
    struct stat info;
    if (snprintf(path, sizeof path, "/proc/%ld", (long)pid) >= (int)sizeof path) return -1;
    if (stat(path, &info) != 0) return -1;
    *uid = info.st_uid;
    return 0;
}

static void process_command(pid_t pid, char output[256]) {
    char path[64];
    FILE *file;
    size_t length;
    output[0] = '\0';
    if (snprintf(path, sizeof path, "/proc/%ld/comm", (long)pid) >= (int)sizeof path) return;
    file = fopen(path, "r");
    if (!file) return;
    if (fgets(output, 256, file)) {
        length = strcspn(output, "\r\n");
        output[length] = '\0';
    }
    (void)fclose(file);
}

static int walk_processes(uid_t wanted, int terminate, int confirmed) {
    DIR *directory = opendir("/proc");
    struct dirent *entry;
    unsigned seen = 0;
    int failures = 0;
    pid_t self = getpid();
    if (!directory) { perror("sessionx: /proc"); return 1; }
    while ((entry = readdir(directory)) != NULL && seen < MAX_PROCESSES) {
        long value;
        char *end;
        pid_t pid;
        uid_t actual;
        char command[256];
        if (!numeric_name(entry->d_name)) continue;
        errno = 0;
        value = strtol(entry->d_name, &end, 10);
        if (errno || *end || value < 1L || value > 2147483647L) continue;
        pid = (pid_t)value;
        ++seen;
        if (process_uid(pid, &actual) != 0 || actual != wanted) continue;
        process_command(pid, command);
        printf("  pid=%ld command=%s", (long)pid, command[0] ? command : "?");
        if (terminate) {
            if (pid == 1 || pid == self) {
                fputs(" protected\n", stdout);
                continue;
            }
            if (!confirmed) { fputs(" would-send=SIGTERM\n", stdout); continue; }
            /* Revalidate ownership immediately before signaling to resist PID reuse. */
            if (process_uid(pid, &actual) != 0 || actual != wanted) {
                fputs(" skipped=ownership-changed\n", stdout);
                ++failures;
            } else if (kill(pid, SIGTERM) != 0) {
                printf(" error=%s\n", strerror(errno));
                ++failures;
            } else fputs(" sent=SIGTERM\n", stdout);
        } else putchar('\n');
    }
    (void)closedir(directory);
    if (seen >= MAX_PROCESSES) fputs("sessionx: process scan limit reached\n", stderr);
    return failures ? 1 : 0;
}

static int list_sessions(void) {
    struct utmpx *record;
    unsigned count = 0;
    setutxent();
    while ((record = getutxent()) != NULL) {
        struct passwd *account;
        char user[sizeof record->ut_user + 1U];
        if (record->ut_type != USER_PROCESS) continue;
        memcpy(user, record->ut_user, sizeof record->ut_user);
        user[sizeof record->ut_user] = '\0';
        user[strnlen(user, sizeof record->ut_user)] = '\0';
        account = getpwnam(user);
        printf("user=%s tty=%.*s host=%.*s uid=%s\n", user,
               (int)sizeof record->ut_line, record->ut_line,
               (int)sizeof record->ut_host, record->ut_host,
               account ? "known" : "unknown");
        if (account) (void)walk_processes(account->pw_uid, 0, 0);
        ++count;
    }
    endutxent();
    if (count == 0U) puts("No interactive login sessions found.");
    return 0;
}

int main(int argc, char **argv) {
    struct passwd *account;
    int confirmed = 0;
    if (argc == 1) return list_sessions();
    if (argc < 3 || argc > 4 || strcmp(argv[1], "--terminate") != 0 ||
        (argc == 4 && strcmp(argv[3], "--confirm") != 0)) {
        fprintf(stderr, "usage: %s [--terminate USER [--confirm]]\n", argv[0]);
        return 2;
    }
    confirmed = argc == 4;
    account = getpwnam(argv[2]);
    if (!account) { fprintf(stderr, "sessionx: unknown user: %s\n", argv[2]); return 1; }
    if (account->pw_uid == 0) { fputs("sessionx: refusing to terminate root processes\n", stderr); return 1; }
    printf("user=%s uid=%lu mode=%s\n", account->pw_name, (unsigned long)account->pw_uid,
           confirmed ? "SIGTERM" : "dry-run");
    return walk_processes(account->pw_uid, 1, confirmed);
}

#else
int main(void) {
    fputs("sessionx: supported on Linux only\n", stderr);
    return 1;
}
#endif
