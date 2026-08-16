#define _POSIX_C_SOURCE 200809L

#include "av_sha256.h"
#include "common.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif
#ifndef O_DIRECTORY
#define O_DIRECTORY 0
#endif

#define AV_MAX_SIGNATURES 4096U
#define AV_LABEL_SIZE 96U
#define AV_PATH_SIZE 4096U
#define AV_MAX_DEPTH 64U
#define AV_HARD_MAX_FILES 1000000ULL
#define AV_DEFAULT_FILES 100000ULL
#define AV_HARD_FILE_BYTES (64ULL * 1024ULL * 1024ULL)
#define AV_DEFAULT_FILE_BYTES (32ULL * 1024ULL * 1024ULL)
#define AV_TOTAL_BYTES (1024ULL * 1024ULL * 1024ULL)
#define AV_MAX_FINDINGS 10000ULL

struct signature { char hash[65]; char label[AV_LABEL_SIZE]; };
struct scanner {
    struct signature *signatures;
    size_t signature_count;
    unsigned long long files;
    unsigned long long directories;
    unsigned long long bytes;
    unsigned long long findings;
    unsigned long long skipped;
    unsigned long long errors;
    unsigned long long max_files;
    unsigned long long max_file_bytes;
    int limit_hit;
};

enum content_flags {
    FLAG_STRATUM = 1U << 0, FLAG_XMRIG = 1U << 1, FLAG_DONATE = 1U << 2,
    FLAG_CURL = 1U << 3, FLAG_WGET = 1U << 4, FLAG_PIPE_SH = 1U << 5,
    FLAG_CHMOD = 1U << 6, FLAG_NOHUP = 1U << 7
};

static void print_safe(const char *text) {
    while (*text) {
        unsigned char byte = (unsigned char)*text++;
        putchar(byte >= 32U && byte <= 126U ? (int)byte : '?');
    }
}

static void finding(struct scanner *scanner, const char *severity, const char *rule,
                    const char *path, const char *evidence) {
    if (scanner->findings >= AV_MAX_FINDINGS) { scanner->limit_hit = 1; return; }
    printf("[%s] %s path=", severity, rule); print_safe(path);
    fputs(" evidence=", stdout); print_safe(evidence); putchar('\n');
    ++scanner->findings;
}

static int valid_hash(const char *text) {
    size_t index;
    if (strlen(text) != 64U) return 0;
    for (index = 0; index < 64U; ++index) if (!isxdigit((unsigned char)text[index])) return 0;
    return 1;
}

static int compare_signatures(const void *left, const void *right) {
    const struct signature *a = left, *b = right;
    return strcmp(a->hash, b->hash);
}

static int read_database(const char *path, char **output, size_t *length) {
    int descriptor = open(path, O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC);
    struct stat before, after;
    char *data;
    size_t used = 0, wanted;
    if (descriptor < 0 || fstat(descriptor, &before) != 0 || !S_ISREG(before.st_mode) ||
        before.st_size < 0 || (unsigned long long)before.st_size > MAX_INPUT_SIZE) {
        if (descriptor >= 0) (void)close(descriptor);
        return -1;
    }
    wanted = (size_t)before.st_size;
    data = malloc(wanted + 1U);
    if (!data) { (void)close(descriptor); return -1; }
    while (used < wanted) {
        ssize_t amount = read(descriptor, data + used, wanted - used);
        if (amount < 0 && errno == EINTR) continue;
        if (amount <= 0) { free(data); (void)close(descriptor); return -1; }
        used += (size_t)amount;
    }
    {
        unsigned char extra;
        ssize_t amount;
        int stat_result, close_result;
        do amount = read(descriptor, &extra, 1U); while (amount < 0 && errno == EINTR);
        stat_result = fstat(descriptor, &after);
        close_result = close(descriptor);
        if (amount != 0 || stat_result != 0 || before.st_dev != after.st_dev ||
            before.st_ino != after.st_ino || before.st_size != after.st_size || close_result != 0) {
            free(data); return -1;
        }
    }
    data[used] = '\0'; *output = data; *length = used;
    return 0;
}

static int load_database(struct scanner *scanner, const char *path) {
    char *data = NULL, *line, *save = NULL;
    size_t length = 0;
    if (read_database(path, &data, &length) != 0) {
        fputs("antivermis: signature database must be a stable regular non-symlink file up to 16 MiB\n", stderr);
        return -1;
    }
    (void)length;
    scanner->signatures = calloc(AV_MAX_SIGNATURES, sizeof *scanner->signatures);
    if (!scanner->signatures) { free(data); return -1; }
    for (line = strtok_r(data, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        char *label, *end;
        size_t index, label_length;
        line[strcspn(line, "\r")] = '\0';
        while (*line == ' ' || *line == '\t') ++line;
        if (*line == '\0' || *line == '#') continue;
        end = line + strcspn(line, " \t");
        if (*end == '\0') { fprintf(stderr, "antivermis: malformed signature line\n"); free(data); return -1; }
        *end++ = '\0';
        while (*end == ' ' || *end == '\t') ++end;
        label = end;
        label_length = strlen(label);
        if (!valid_hash(line) || label_length == 0U || label_length >= AV_LABEL_SIZE ||
            scanner->signature_count >= AV_MAX_SIGNATURES) {
            fprintf(stderr, "antivermis: invalid or excessive signature database\n");
            free(data); return -1;
        }
        for (index = 0; index < 64U; ++index) line[index] = (char)tolower((unsigned char)line[index]);
        memcpy(scanner->signatures[scanner->signature_count].hash, line, 65U);
        memcpy(scanner->signatures[scanner->signature_count].label, label, label_length + 1U);
        ++scanner->signature_count;
    }
    free(data);
    qsort(scanner->signatures, scanner->signature_count, sizeof *scanner->signatures, compare_signatures);
    {
        size_t index;
        for (index = 1; index < scanner->signature_count; ++index) {
            if (strcmp(scanner->signatures[index - 1U].hash, scanner->signatures[index].hash) == 0) {
                fputs("antivermis: duplicate signature hash\n", stderr);
                return -1;
            }
        }
    }
    return 0;
}

static const char *signature_label(const struct scanner *scanner, const char hash[65]) {
    struct signature key;
    const struct signature *match;
    memset(&key, 0, sizeof key); memcpy(key.hash, hash, 65U);
    match = bsearch(&key, scanner->signatures, scanner->signature_count,
                    sizeof *scanner->signatures, compare_signatures);
    return match ? match->label : NULL;
}

static int contains_path(const char *path, const char *part) { return strstr(path, part) != NULL; }

static int ends_with(const char *text, const char *suffix) {
    size_t text_length = strlen(text), suffix_length = strlen(suffix);
    return text_length >= suffix_length && strcmp(text + text_length - suffix_length, suffix) == 0;
}

static int persistence_path(const char *path) {
    return contains_path(path, "/LaunchAgents/") || contains_path(path, "/LaunchDaemons/") ||
           contains_path(path, "/systemd/system/") || contains_path(path, "/cron.d/") ||
           contains_path(path, "/.config/autostart/") || contains_path(path, "/.config/systemd/user/");
}

static int suspicious_double_extension(const char *path) {
    static const char *const decoys[] = {".pdf.", ".jpg.", ".jpeg.", ".png.", ".doc.", ".docx.", ".txt."};
    char lowered[AV_PATH_SIZE];
    size_t index, length = strlen(path);
    if (length >= sizeof lowered) return 0;
    for (index = 0; index <= length; ++index) lowered[index] = (char)tolower((unsigned char)path[index]);
    for (index = 0; index < sizeof decoys / sizeof decoys[0]; ++index)
        if (strstr(lowered, decoys[index])) return 1;
    return 0;
}

static unsigned inspect_text(const unsigned char *data, size_t length, unsigned flags) {
    char normalized[8449];
    size_t index;
    if (length > sizeof normalized - 1U) length = sizeof normalized - 1U;
    for (index = 0; index < length; ++index) {
        unsigned char byte = data[index];
        normalized[index] = byte >= 32U && byte <= 126U ? (char)tolower(byte) : ' ';
    }
    normalized[length] = '\0';
    if (strstr(normalized, "stratum+tcp://") || strstr(normalized, "stratum+ssl://")) flags |= FLAG_STRATUM;
    if (strstr(normalized, "xmrig")) flags |= FLAG_XMRIG;
    if (strstr(normalized, "--donate-level")) flags |= FLAG_DONATE;
    if (strstr(normalized, "curl ")) flags |= FLAG_CURL;
    if (strstr(normalized, "wget ")) flags |= FLAG_WGET;
    if (strstr(normalized, "| sh") || strstr(normalized, "|sh") || strstr(normalized, "| bash")) flags |= FLAG_PIPE_SH;
    if (strstr(normalized, "chmod +x")) flags |= FLAG_CHMOD;
    if (strstr(normalized, "nohup ")) flags |= FLAG_NOHUP;
    return flags;
}

static int scan_file(struct scanner *scanner, const char *path, const struct stat *expected) {
    int descriptor;
    struct stat before, after;
    struct av_sha256 sha;
    unsigned char digest[32], buffer[8192 + 256];
    char hash[65];
    size_t carry = 0;
    unsigned flags = 0;
    unsigned long long read_total = 0;
    ssize_t amount;
    int executable = (expected->st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0;
    const char *label;
    if ((unsigned long long)expected->st_size > scanner->max_file_bytes ||
        scanner->bytes > AV_TOTAL_BYTES - (unsigned long long)expected->st_size) {
        ++scanner->skipped; return 0;
    }
    descriptor = open(path, O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC);
    if (descriptor < 0) { ++scanner->errors; return 0; }
    if (fstat(descriptor, &before) != 0 || !S_ISREG(before.st_mode) || before.st_dev != expected->st_dev ||
        before.st_ino != expected->st_ino || before.st_size != expected->st_size) {
        (void)close(descriptor); ++scanner->errors; return 0;
    }
    av_sha256_init(&sha);
    while ((amount = read(descriptor, buffer + carry, 8192U)) != 0) {
        size_t combined, keep;
        if (amount < 0) { if (errno == EINTR) continue; (void)close(descriptor); ++scanner->errors; return 0; }
        av_sha256_update(&sha, buffer + carry, (size_t)amount);
        read_total += (unsigned long long)amount;
        combined = carry + (size_t)amount;
        flags = inspect_text(buffer, combined, flags);
        keep = combined < 256U ? combined : 256U;
        memmove(buffer, buffer + combined - keep, keep);
        carry = keep;
    }
    if (fstat(descriptor, &after) != 0) {
        (void)close(descriptor); ++scanner->errors; return 0;
    }
    if (close(descriptor) != 0 || before.st_dev != after.st_dev || before.st_ino != after.st_ino ||
        before.st_size != after.st_size || read_total != (unsigned long long)before.st_size) {
        ++scanner->errors; return 0;
    }
    scanner->bytes += read_total;
    av_sha256_final(&sha, digest); av_sha256_hex(digest, hash);
    label = scanner->signatures ? signature_label(scanner, hash) : NULL;
    if (label) finding(scanner, "HIGH", "AV-SIG-001", path, label);
    if (executable && (expected->st_mode & S_IWOTH))
        finding(scanner, "HIGH", "AV-FILE-001", path, "world-writable executable");
    if (executable && (expected->st_mode & (S_ISUID | S_ISGID)) &&
        !contains_path(path, "/usr/bin/") && !contains_path(path, "/usr/sbin/") && !contains_path(path, "/bin/"))
        finding(scanner, "HIGH", "AV-FILE-002", path, "set-id executable outside standard binary paths");
    if (executable && (contains_path(path, "/tmp/") || contains_path(path, "/var/tmp/") || contains_path(path, "/dev/shm/")))
        finding(scanner, "MEDIUM", "AV-FILE-003", path, "executable in globally writable temporary storage");
    if (executable && suspicious_double_extension(path))
        finding(scanner, "MEDIUM", "AV-FILE-004", path, "executable uses a document-style double extension");
    if (persistence_path(path) && (expected->st_mode & S_IWOTH))
        finding(scanner, "MEDIUM", "AV-PERSIST-001", path, "world-writable persistence entry");
    if (persistence_path(path) && executable && path[strlen(path) - 1U] != '/')
        finding(scanner, "MEDIUM", "AV-PERSIST-002", path, "executable stored directly in a persistence directory");
    if (ends_with(path, "/etc/ld.so.preload") && before.st_size > 0)
        finding(scanner, "MEDIUM", "AV-ROOTKIT-001", path, "dynamic-loader preload configuration is non-empty; verify every library");
    if ((flags & FLAG_STRATUM) && (flags & (FLAG_XMRIG | FLAG_DONATE | FLAG_NOHUP)))
        finding(scanner, "HIGH", "AV-MINER-001", path, "compound Stratum and miner execution indicators");
    else if (flags & FLAG_STRATUM)
        finding(scanner, "LOW", "AV-MINER-002", path, "Stratum endpoint string; verify context manually");
    if ((flags & (FLAG_CURL | FLAG_WGET)) && (flags & FLAG_PIPE_SH) && (flags & (FLAG_CHMOD | FLAG_NOHUP)))
        finding(scanner, "HIGH", "AV-DROPPER-001", path, "download, shell-pipe, and execution persistence indicators");
    return 0;
}

static int scan_path(struct scanner *scanner, const char *path, unsigned depth) {
    struct stat info;
    if (scanner->files >= scanner->max_files) { scanner->limit_hit = 1; return 0; }
    if (lstat(path, &info) != 0) { ++scanner->errors; return 0; }
    if (S_ISLNK(info.st_mode)) { ++scanner->skipped; return 0; }
    if (S_ISREG(info.st_mode)) { ++scanner->files; return scan_file(scanner, path, &info); }
    if (!S_ISDIR(info.st_mode)) { ++scanner->skipped; return 0; }
    if (depth >= AV_MAX_DEPTH) { scanner->limit_hit = 1; return 0; }
    {
        int directory_descriptor = open(path, O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC | O_DIRECTORY);
        DIR *directory;
        struct dirent *entry;
        struct stat opened_info;
        if (directory_descriptor < 0 || fstat(directory_descriptor, &opened_info) != 0 ||
            !S_ISDIR(opened_info.st_mode) || opened_info.st_dev != info.st_dev || opened_info.st_ino != info.st_ino) {
            if (directory_descriptor >= 0) (void)close(directory_descriptor);
            ++scanner->errors; return 0;
        }
        directory = fdopendir(directory_descriptor);
        if (!directory) { (void)close(directory_descriptor); ++scanner->errors; return 0; }
        ++scanner->directories;
        while (!scanner->limit_hit && (entry = readdir(directory)) != NULL) {
            char child[AV_PATH_SIZE];
            int length;
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
            length = snprintf(child, sizeof child, "%s/%s", path, entry->d_name);
            if (length < 0 || (size_t)length >= sizeof child) { ++scanner->skipped; continue; }
            (void)scan_path(scanner, child, depth + 1U);
        }
        if (closedir(directory) != 0) ++scanner->errors;
    }
    return 0;
}

static void scan_system(struct scanner *scanner) {
#if defined(__linux__)
    static const char *const paths[] = {"/etc/ld.so.preload", "/etc/systemd/system", "/etc/cron.d", "/etc/rc.local", "/tmp", "/var/tmp", "/dev/shm"};
#elif defined(__APPLE__)
    static const char *const paths[] = {"/Library/LaunchAgents", "/Library/LaunchDaemons", "/tmp", "/private/tmp", "/private/var/tmp"};
#else
    static const char *const paths[] = {"/tmp"};
#endif
    size_t index;
    for (index = 0; index < sizeof paths / sizeof paths[0] && !scanner->limit_hit; ++index)
        if (access(paths[index], F_OK) == 0) (void)scan_path(scanner, paths[index], 0);
}

int main(int argc, char **argv) {
    struct scanner scanner;
    const char *database = NULL;
    int system_mode = 0, first_path = 1, index;
    memset(&scanner, 0, sizeof scanner);
    scanner.max_files = AV_DEFAULT_FILES; scanner.max_file_bytes = AV_DEFAULT_FILE_BYTES;
    while (first_path < argc) {
        if (strcmp(argv[first_path], "--db") == 0 && first_path + 1 < argc) database = argv[++first_path];
        else if (strcmp(argv[first_path], "--max-files") == 0 && first_path + 1 < argc) {
            long value; if (parse_long(argv[++first_path], 1, (long)AV_HARD_MAX_FILES, &value) != 0) return 2;
            scanner.max_files = (unsigned long long)value;
        } else if (strcmp(argv[first_path], "--max-bytes") == 0 && first_path + 1 < argc) {
            long value; if (parse_long(argv[++first_path], 1, 64, &value) != 0) return 2;
            scanner.max_file_bytes = (unsigned long long)value * 1024ULL * 1024ULL;
        } else if (strcmp(argv[first_path], "--system") == 0) system_mode = 1;
        else break;
        ++first_path;
    }
    if (!system_mode && first_path >= argc) {
        fprintf(stderr, "usage: %s [--db FILE] [--max-files N] [--max-bytes MiB] [--system] PATH...\n", argv[0]);
        return 2;
    }
    if (database && load_database(&scanner, database) != 0) { free(scanner.signatures); return 2; }
    if (system_mode) scan_system(&scanner);
    for (index = first_path; index < argc && !scanner.limit_hit; ++index) (void)scan_path(&scanner, argv[index], 0);
    printf("SUMMARY files=%llu directories=%llu bytes=%llu findings=%llu skipped=%llu errors=%llu limited=%s\n",
           scanner.files, scanner.directories, scanner.bytes, scanner.findings,
           scanner.skipped, scanner.errors, scanner.limit_hit ? "yes" : "no");
    free(scanner.signatures);
    if (scanner.errors || scanner.limit_hit) return 2;
    return scanner.findings ? 1 : 0;
}
