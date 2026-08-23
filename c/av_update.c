#define _POSIX_C_SOURCE 200809L

#include "av_update.h"
#include "av_sha256.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
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

#if PYDEV_HAVE_CURL
#include <curl/curl.h>

#define MANIFEST_LIMIT (64U * 1024U)
#define DATABASE_LIMIT (16U * 1024U * 1024U)
#define URL_LIMIT 2048U
#define VERSION_LIMIT 64U
#define PATH_LIMIT 4096U

struct download {
    unsigned char *data;
    size_t used;
    size_t limit;
    int exceeded;
};

struct manifest {
    char version[VERSION_LIMIT];
    char database_url[URL_LIMIT];
    char sha256[65];
};

static int supported_url(const char *url) {
    const unsigned char *cursor = (const unsigned char *)url;
    if (strncmp(url, "https://", 8U) != 0 && strncmp(url, "file://", 7U) != 0) return 0;
    for (; *cursor; ++cursor) if (*cursor <= 32U || *cursor == 127U) return 0;
    return 1;
}

static int valid_version(const char *version) {
    const unsigned char *cursor = (const unsigned char *)version;
    if (*cursor == '\0') return 0;
    for (; *cursor; ++cursor)
        if (!isalnum(*cursor) && *cursor != '.' && *cursor != '-' && *cursor != '_' && *cursor != '+') return 0;
    return 1;
}

static size_t receive_data(char *input, size_t size, size_t count, void *user_data) {
    struct download *download = user_data;
    size_t amount;
    unsigned char *grown;
    if (size != 0U && count > (size_t)-1 / size) { download->exceeded = 1; return 0U; }
    amount = size * count;
    if (amount > download->limit - download->used) { download->exceeded = 1; return 0U; }
    grown = realloc(download->data, download->used + amount + 1U);
    if (!grown) return 0U;
    download->data = grown;
    memcpy(download->data + download->used, input, amount);
    download->used += amount;
    download->data[download->used] = '\0';
    return amount;
}

static int fetch_url(const char *url, size_t limit, struct download *download) {
    CURL *curl;
    CURLcode result;
    long response = 0;
    if (!supported_url(url)) { fputs("antivermis: update URLs must use HTTPS or file://\n", stderr); return -1; }
    memset(download, 0, sizeof *download); download->limit = limit;
    curl = curl_easy_init();
    if (!curl) return -1;
    (void)curl_easy_setopt(curl, CURLOPT_URL, url);
    (void)curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, receive_data);
    (void)curl_easy_setopt(curl, CURLOPT_WRITEDATA, download);
    (void)curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    (void)curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3L);
    (void)curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    (void)curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
    (void)curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    (void)curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
#if LIBCURL_VERSION_NUM >= 0x075500
    (void)curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https,file");
    (void)curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "https,file");
#else
    (void)curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS | CURLPROTO_FILE);
    (void)curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS | CURLPROTO_FILE);
#endif
    (void)curl_easy_setopt(curl, CURLOPT_USERAGENT, "antivermis/1");
    (void)curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    result = curl_easy_perform(curl);
    (void)curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response);
    curl_easy_cleanup(curl);
    if (result != CURLE_OK || download->exceeded ||
        (strncmp(url, "https://", 8U) == 0 && (response < 200L || response >= 300L))) {
        free(download->data); memset(download, 0, sizeof *download);
        return -1;
    }
    if (!download->data) {
        download->data = calloc(1U, 1U);
        if (!download->data) return -1;
    }
    return 0;
}

static int valid_hex(const char *text) {
    size_t index;
    if (strlen(text) != 64U) return 0;
    for (index = 0; index < 64U; ++index) if (!isxdigit((unsigned char)text[index])) return 0;
    return 1;
}

static int parse_manifest(char *text, struct manifest *manifest) {
    char *line, *save = NULL;
    int header = 0, version = 0, database = 0, sha = 0;
    memset(manifest, 0, sizeof *manifest);
    for (line = strtok_r(text, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        char *value;
        line[strcspn(line, "\r")] = '\0';
        if (*line == '\0' || *line == '#') continue;
        if (!header) {
            if (strcmp(line, "ANTIVERMIS-MANIFEST 1") != 0) return -1;
            header = 1; continue;
        }
        value = strchr(line, ' ');
        if (!value) return -1;
        *value++ = '\0';
        while (*value == ' ') ++value;
        if (strcmp(line, "version") == 0 && !version) {
            if (!valid_version(value) || strlen(value) >= sizeof manifest->version) return -1;
            snprintf(manifest->version, sizeof manifest->version, "%s", value); version = 1;
        } else if (strcmp(line, "database") == 0 && !database) {
            if (!supported_url(value) || strlen(value) >= sizeof manifest->database_url) return -1;
            snprintf(manifest->database_url, sizeof manifest->database_url, "%s", value); database = 1;
        } else if (strcmp(line, "sha256") == 0 && !sha) {
            size_t index;
            if (!valid_hex(value)) return -1;
            for (index = 0; index < 64U; ++index) manifest->sha256[index] = (char)tolower((unsigned char)value[index]);
            manifest->sha256[64] = '\0'; sha = 1;
        } else return -1;
    }
    return header && version && database && sha ? 0 : -1;
}

static void hash_memory(const unsigned char *data, size_t length, char output[65]) {
    struct av_sha256 context;
    unsigned char digest[32];
    av_sha256_init(&context); av_sha256_update(&context, data, length);
    av_sha256_final(&context, digest); av_sha256_hex(digest, output);
}

static int hash_local_database(const char *path, char output[65]) {
    int descriptor = open(path, O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC);
    struct stat before, after;
    struct av_sha256 context;
    unsigned char buffer[8192], digest[32];
    unsigned long long total = 0;
    if (descriptor < 0 || fstat(descriptor, &before) != 0 || !S_ISREG(before.st_mode) ||
        before.st_size < 0 || (unsigned long long)before.st_size > DATABASE_LIMIT) {
        if (descriptor >= 0) (void)close(descriptor);
        return -1;
    }
    av_sha256_init(&context);
    for (;;) {
        ssize_t amount = read(descriptor, buffer, sizeof buffer);
        if (amount < 0 && errno == EINTR) continue;
        if (amount < 0) { (void)close(descriptor); return -1; }
        if (amount == 0) break;
        total += (unsigned long long)amount;
        av_sha256_update(&context, buffer, (size_t)amount);
    }
    if (fstat(descriptor, &after) != 0 ||
        before.st_dev != after.st_dev || before.st_ino != after.st_ino ||
        before.st_size != after.st_size || before.st_mtime != after.st_mtime ||
        total != (unsigned long long)before.st_size) {
        (void)close(descriptor); return -1;
    }
    if (close(descriptor) != 0) return -1;
    av_sha256_final(&context, digest); av_sha256_hex(digest, output);
    return 0;
}

static int get_manifest(const char *url, struct manifest *manifest) {
    struct download download;
    int result;
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) return -1;
    result = fetch_url(url, MANIFEST_LIMIT, &download);
    if (result == 0 && memchr(download.data, '\0', download.used) != NULL) result = -1;
    if (result == 0) result = parse_manifest((char *)download.data, manifest);
    free(download.data); curl_global_cleanup();
    return result;
}

int av_update_available(void) { return 1; }

int av_check_database_update(const char *manifest_url, const char *database_path) {
    struct manifest manifest;
    char local_hash[65];
    if (get_manifest(manifest_url, &manifest) != 0) { fputs("antivermis: cannot fetch or parse update manifest\n", stderr); return -1; }
    if (hash_local_database(database_path, local_hash) == 0 && strcmp(local_hash, manifest.sha256) == 0)
        printf("database is current (version %s)\n", manifest.version);
    else printf("database update available (version %s)\n", manifest.version);
    return 0;
}

static int write_all(int descriptor, const unsigned char *data, size_t length) {
    size_t used = 0;
    while (used < length) {
        ssize_t amount = write(descriptor, data + used, length - used);
        if (amount < 0 && errno == EINTR) continue;
        if (amount <= 0) return -1;
        used += (size_t)amount;
    }
    return 0;
}

int av_update_database(const char *manifest_url, const char *database_path,
                       av_database_validator validator) {
    struct manifest manifest;
    struct download database;
    struct stat existing;
    char actual_hash[65], temporary[PATH_LIMIT];
    int descriptor = -1, result = -1;
    if (get_manifest(manifest_url, &manifest) != 0) { fputs("antivermis: cannot fetch or parse update manifest\n", stderr); return -1; }
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) return -1;
    if (fetch_url(manifest.database_url, DATABASE_LIMIT, &database) != 0) goto cleanup_curl;
    hash_memory(database.data, database.used, actual_hash);
    if (strcmp(actual_hash, manifest.sha256) != 0) { fputs("antivermis: downloaded database SHA-256 mismatch\n", stderr); goto cleanup_data; }
    if (lstat(database_path, &existing) == 0) {
        if (!S_ISREG(existing.st_mode)) {
            fputs("antivermis: destination must be a regular non-symlink file\n", stderr); goto cleanup_data;
        }
    } else if (errno != ENOENT) {
        fputs("antivermis: cannot inspect database destination\n", stderr); goto cleanup_data;
    }
    if (snprintf(temporary, sizeof temporary, "%s.tmp.XXXXXX", database_path) >= (int)sizeof temporary) goto cleanup_data;
    descriptor = mkstemp(temporary);
    if (descriptor < 0) goto cleanup_data;
    if (fchmod(descriptor, 0600) != 0 ||
        write_all(descriptor, database.data, database.used) != 0 ||
        fsync(descriptor) != 0) {
        (void)close(descriptor); descriptor = -1;
        (void)unlink(temporary); goto cleanup_data;
    }
    if (close(descriptor) != 0) {
        descriptor = -1; (void)unlink(temporary); goto cleanup_data;
    }
    descriptor = -1;
    if (validator(temporary) != 0) { fputs("antivermis: downloaded database is invalid\n", stderr); (void)unlink(temporary); goto cleanup_data; }
    if (rename(temporary, database_path) != 0) { (void)unlink(temporary); goto cleanup_data; }
    printf("database updated to version %s\n", manifest.version); result = 0;
cleanup_data:
    free(database.data);
cleanup_curl:
    curl_global_cleanup(); return result;
}

#else

int av_update_available(void) { return 0; }
int av_check_database_update(const char *manifest_url, const char *database_path) {
    (void)manifest_url; (void)database_path;
    fputs("antivermis: database updating is unavailable; rebuild with libcurl\n", stderr); return -1;
}
int av_update_database(const char *manifest_url, const char *database_path,
                       av_database_validator validator) {
    (void)manifest_url; (void)database_path; (void)validator;
    fputs("antivermis: database updating is unavailable; rebuild with libcurl\n", stderr); return -1;
}

#endif
