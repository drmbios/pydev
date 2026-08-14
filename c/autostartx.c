#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define PATH_CAPACITY 4096U
#define MAX_ENTRIES 100000U

static unsigned total_entries;

static void print_text(const char *text) {
    while (*text) {
        unsigned char byte = (unsigned char)*text++;
        putchar(byte >= 32U && byte <= 126U ? (int)byte : '?');
    }
}

static void list_directory(const char *label, const char *path) {
    DIR *directory;
    struct dirent *entry;
    struct stat info;
    if (lstat(path, &info) != 0 || !S_ISDIR(info.st_mode)) return;
    directory = opendir(path);
    if (!directory) return;
    putchar('['); print_text(label); fputs("] ", stdout); print_text(path); putchar('\n');
    while ((entry = readdir(directory)) != NULL && total_entries < MAX_ENTRIES) {
        char full_path[PATH_CAPACITY];
        int length;
        if (entry->d_name[0] == '.') continue;
        length = snprintf(full_path, sizeof full_path, "%s/%s", path, entry->d_name);
        if (length < 0 || (size_t)length >= sizeof full_path) continue;
        if (lstat(full_path, &info) != 0) continue;
        fputs("  ", stdout);
        print_text(entry->d_name);
        if (S_ISLNK(info.st_mode)) fputs(" -> symlink", stdout);
        putchar('\n');
        ++total_entries;
    }
    (void)closedir(directory);
}

static void list_user_directory(const char *label, const char *suffix) {
    const char *home_directory = getenv("HOME");
    char path[PATH_CAPACITY];
    if (!home_directory || !*home_directory) return;
    if (snprintf(path, sizeof path, "%s/%s", home_directory, suffix) >= (int)sizeof path) return;
    list_directory(label, path);
}

int main(void) {
#if defined(__APPLE__)
    list_user_directory("user launch agents", "Library/LaunchAgents");
    list_directory("system launch agents", "/Library/LaunchAgents");
    list_directory("system launch daemons", "/Library/LaunchDaemons");
    list_directory("apple launch agents", "/System/Library/LaunchAgents");
    list_directory("apple launch daemons", "/System/Library/LaunchDaemons");
#elif defined(__linux__)
    list_user_directory("desktop autostart", ".config/autostart");
    list_user_directory("user systemd", ".config/systemd/user");
    list_directory("system autostart", "/etc/xdg/autostart");
    list_directory("systemd system", "/etc/systemd/system");
    list_directory("cron", "/etc/cron.d");
#else
    fputs("autostartx: startup locations are defined for Linux and macOS only\n", stderr);
    return 1;
#endif
    if (total_entries >= MAX_ENTRIES) fputs("autostartx: entry limit reached\n", stderr);
    if (total_entries == 0U) puts("No readable startup entries found.");
    return 0;
}
