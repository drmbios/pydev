#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_SCAN_DEPTH 64U
#define MAX_SCAN_ENTRIES 1000000U
#define MAX_LIST_ENTRIES 100000U

struct scan_state {
    size_t entries;
    int incomplete;
};

struct entry {
    char *name;
    struct stat info;
    uint64_t size;
    int incomplete;
};

static int sort_by_size = 0;
static int reverse_sort = 0;

static char *join_path(const char *directory, const char *name) {
    size_t directory_length = strlen(directory);
    size_t name_length = strlen(name);
    int separator = directory_length > 0 && directory[directory_length - 1] != '/';
    char *path;

    if (directory_length > SIZE_MAX - name_length - (size_t)separator - 1U) {
        errno = ENAMETOOLONG;
        return NULL;
    }
    path = malloc(directory_length + (size_t)separator + name_length + 1U);
    if (!path) return NULL;
    memcpy(path, directory, directory_length);
    if (separator) path[directory_length++] = '/';
    memcpy(path + directory_length, name, name_length + 1U);
    return path;
}

static uint64_t add_saturating(uint64_t left, uint64_t right, int *incomplete) {
    if (UINT64_MAX - left < right) {
        *incomplete = 1;
        return UINT64_MAX;
    }
    return left + right;
}

static uint64_t apparent_size(const char *path, const struct stat *info,
                              unsigned depth, struct scan_state *state) {
    DIR *directory;
    struct dirent *item;
    uint64_t total = 0;

    if (!S_ISDIR(info->st_mode)) return info->st_size > 0 ? (uint64_t)info->st_size : 0;
    if (depth >= MAX_SCAN_DEPTH || state->entries >= MAX_SCAN_ENTRIES) {
        state->incomplete = 1;
        return 0;
    }
    directory = opendir(path);
    if (!directory) {
        state->incomplete = 1;
        return 0;
    }
    for (;;) {
        char *child_path;
        struct stat child_info;
        uint64_t child_size;

        errno = 0;
        item = readdir(directory);
        if (!item) {
            if (errno != 0) state->incomplete = 1;
            break;
        }
        if (!strcmp(item->d_name, ".") || !strcmp(item->d_name, "..")) continue;
        if (++state->entries > MAX_SCAN_ENTRIES) {
            state->incomplete = 1;
            break;
        }
        child_path = join_path(path, item->d_name);
        if (!child_path) {
            state->incomplete = 1;
            break;
        }
        if (lstat(child_path, &child_info) != 0) {
            state->incomplete = 1;
            free(child_path);
            continue;
        }
        child_size = apparent_size(child_path, &child_info, depth + 1U, state);
        total = add_saturating(total, child_size, &state->incomplete);
        free(child_path);
    }
    if (closedir(directory) != 0) state->incomplete = 1;
    return total;
}

static const char *type_name(mode_t mode) {
    if (S_ISDIR(mode)) return "DIR";
    if (S_ISREG(mode)) return "FILE";
    if (S_ISLNK(mode)) return "LINK";
    if (S_ISCHR(mode)) return "CHAR";
    if (S_ISBLK(mode)) return "BLOCK";
    if (S_ISFIFO(mode)) return "FIFO";
    if (S_ISSOCK(mode)) return "SOCKET";
    return "OTHER";
}

static void human_size(uint64_t bytes, char output[32]) {
    static const char *const units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    double value = (double)bytes;
    size_t unit = 0;

    while (value >= 1024.0 && unit + 1U < sizeof units / sizeof units[0]) {
        value /= 1024.0;
        ++unit;
    }
    if (unit == 0) snprintf(output, 32, "%llu B", (unsigned long long)bytes);
    else snprintf(output, 32, "%.2f %s", value, units[unit]);
}

static void print_name_safely(const char *name) {
    const unsigned char *cursor = (const unsigned char *)name;
    while (*cursor) {
        if (isprint(*cursor) && *cursor != '\\') putchar(*cursor);
        else printf("\\x%02x", *cursor);
        ++cursor;
    }
}

static void print_entry(const struct entry *item) {
    char size[32];
    unsigned permissions = (unsigned)(item->info.st_mode & 07777U);
    human_size(item->size, size);
    if (permissions > 0777U) printf("%04o", permissions);
    else printf("%03o ", permissions);
    printf("  %-6s %12s%s  ", type_name(item->info.st_mode), size,
           item->incomplete ? "*" : " ");
    print_name_safely(item->name);
    putchar('\n');
}

static int compare_entries(const void *left_pointer, const void *right_pointer) {
    const struct entry *left = left_pointer;
    const struct entry *right = right_pointer;
    int result;

    if (sort_by_size && left->size != right->size) result = left->size < right->size ? 1 : -1;
    else result = strcmp(left->name, right->name);
    return reverse_sort ? -result : result;
}

static void free_entries(struct entry *entries, size_t count) {
    size_t index;
    for (index = 0; index < count; ++index) free(entries[index].name);
    free(entries);
}

static int list_directory(const char *path, int show_hidden) {
    DIR *directory = opendir(path);
    struct dirent *item;
    struct entry *entries = NULL;
    size_t count = 0;
    size_t capacity = 0;
    int status = 0;

    if (!directory) {
        perror(path);
        return 1;
    }
    for (;;) {
        char *full_path;
        struct entry next;
        struct scan_state state = {0, 0};

        errno = 0;
        item = readdir(directory);
        if (!item) {
            if (errno != 0) {
                perror(path);
                status = 1;
            }
            break;
        }
        if (!strcmp(item->d_name, ".") || !strcmp(item->d_name, "..")) continue;
        if (!show_hidden && item->d_name[0] == '.') continue;
        if (count == MAX_LIST_ENTRIES) {
            fprintf(stderr, "lsx: directory exceeds %u-entry listing limit\n", MAX_LIST_ENTRIES);
            status = 1;
            break;
        }
        full_path = join_path(path, item->d_name);
        if (!full_path) {
            perror("lsx: path");
            status = 1;
            break;
        }
        if (lstat(full_path, &next.info) != 0) {
            perror(full_path);
            free(full_path);
            status = 1;
            continue;
        }
        next.size = apparent_size(full_path, &next.info, 0, &state);
        next.incomplete = state.incomplete;
        next.name = strdup(item->d_name);
        free(full_path);
        if (!next.name) {
            perror("lsx: strdup");
            status = 1;
            break;
        }
        if (count == capacity) {
            size_t new_capacity = capacity ? capacity * 2U : 32U;
            struct entry *grown;
            if (new_capacity > MAX_LIST_ENTRIES) new_capacity = MAX_LIST_ENTRIES;
            grown = realloc(entries, new_capacity * sizeof *entries);
            if (!grown) {
                perror("lsx: realloc");
                free(next.name);
                status = 1;
                break;
            }
            entries = grown;
            capacity = new_capacity;
        }
        entries[count++] = next;
    }
    if (closedir(directory) != 0) {
        perror(path);
        status = 1;
    }
    qsort(entries, count, sizeof *entries, compare_entries);
    puts("PERM  TYPE           SIZE   NAME");
    puts("----  ------ ------------   ----");
    for (capacity = 0; capacity < count; ++capacity) print_entry(&entries[capacity]);
    if (status) fputs("* Some sizes may be incomplete due to limits or read errors.\n", stderr);
    free_entries(entries, count);
    return status;
}

static int list_path(const char *path) {
    struct entry item;
    struct scan_state state = {0, 0};
    const char *name = strrchr(path, '/');

    if (lstat(path, &item.info) != 0) {
        perror(path);
        return 1;
    }
    item.name = (char *)(name && name[1] ? name + 1 : path);
    item.size = apparent_size(path, &item.info, 0, &state);
    item.incomplete = state.incomplete;
    puts("PERM  TYPE           SIZE   NAME");
    puts("----  ------ ------------   ----");
    print_entry(&item);
    return state.incomplete ? 1 : 0;
}

static void usage(const char *program) {
    fprintf(stderr,
            "usage: %s [-aSr] [PATH]\n"
            "  -a  include hidden entries\n"
            "  -S  sort largest first\n"
            "  -r  reverse the selected order\n",
            program);
}

int main(int argc, char **argv) {
    int option;
    int show_hidden = 0;
    const char *path;
    struct stat info;

    while ((option = getopt(argc, argv, "aSrh")) != -1) {
        switch (option) {
            case 'a': show_hidden = 1; break;
            case 'S': sort_by_size = 1; break;
            case 'r': reverse_sort = 1; break;
            case 'h': usage(argv[0]); return 0;
            default: usage(argv[0]); return 2;
        }
    }
    if (optind + 1 < argc) {
        usage(argv[0]);
        return 2;
    }
    path = optind < argc ? argv[optind] : ".";
    if (lstat(path, &info) != 0) {
        perror(path);
        return 1;
    }
    return S_ISDIR(info.st_mode) ? list_directory(path, show_hidden) : list_path(path);
}
