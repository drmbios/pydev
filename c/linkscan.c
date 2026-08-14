#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAX_DEPTH 128U
#define MAX_ENTRIES 1000000ULL
#define PATH_CAPACITY 4096U

struct target { dev_t device; ino_t inode; unsigned long long visited; unsigned long long matches; };

static void print_path(const char *path) {
    while (*path) {
        unsigned char byte = (unsigned char)*path++;
        putchar(byte >= 32U && byte <= 126U ? (int)byte : '?');
    }
    putchar('\n');
}

static int scan(const char *path, struct target *target, unsigned depth) {
    struct stat info;
    DIR *directory;
    struct dirent *entry;
    if (target->visited++ >= MAX_ENTRIES) {
        fputs("linkscan: entry limit reached\n", stderr);
        errno = EOVERFLOW;
        return -1;
    }
    if (lstat(path, &info) != 0) return errno == EACCES || errno == ENOENT ? 0 : -1;
    if (info.st_dev == target->device && info.st_ino == target->inode) {
        print_path(path);
        ++target->matches;
    }
    if (!S_ISDIR(info.st_mode) || depth >= MAX_DEPTH) return 0;
    directory = opendir(path);
    if (!directory) return errno == EACCES || errno == ENOENT ? 0 : -1;
    while ((entry = readdir(directory)) != NULL) {
        char child[PATH_CAPACITY];
        int length;
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        length = snprintf(child, sizeof child, "%s/%s", path, entry->d_name);
        if (length < 0 || (size_t)length >= sizeof child) { fputs("linkscan: path too long\n", stderr); continue; }
        if (scan(child, target, depth + 1U) != 0) {
            int saved_error = errno;
            (void)closedir(directory);
            errno = saved_error;
            return -1;
        }
    }
    return closedir(directory) == 0 ? 0 : -1;
}

int main(int argc, char **argv) {
    struct stat info;
    struct target target;
    const char *root;
    if (argc < 2 || argc > 3) { fprintf(stderr, "usage: %s FILE [SEARCH_ROOT]\n", argv[0]); return 2; }
    root = argc == 3 ? argv[2] : ".";
    if (lstat(argv[1], &info) != 0) { fprintf(stderr, "linkscan: %s: %s\n", argv[1], strerror(errno)); return 1; }
    target.device = info.st_dev; target.inode = info.st_ino; target.visited = 0; target.matches = 0;
    if (scan(root, &target, 0) != 0) { fprintf(stderr, "linkscan: scan failed: %s\n", strerror(errno)); return 1; }
    fprintf(stderr, "linkscan: %llu match(es), %llu entries inspected\n", target.matches, target.visited);
    return target.matches ? 0 : 1;
}
