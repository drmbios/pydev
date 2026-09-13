#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#else
#define _POSIX_C_SOURCE 200809L
#endif

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <sys/sysctl.h>
#endif

static void print_bytes(const char *label, unsigned long long bytes) {
    printf("%-16s %llu bytes (%.2f GiB)\n", label, bytes,
           (double)bytes / (1024.0 * 1024.0 * 1024.0));
}

#if defined(__linux__)
static void linux_details(void) {
    FILE *file = fopen("/proc/cpuinfo", "r");
    char line[1024];
    int model_printed = 0;
    if (file) {
        while (fgets(line, sizeof line, file)) {
            if (!model_printed && (strncmp(line, "model name", 10) == 0 ||
                                   strncmp(line, "Hardware", 8) == 0)) {
                char *colon = strchr(line, ':');
                if (colon) { colon += 1; colon[strcspn(colon, "\r\n")] = '\0'; printf("%-16s%s\n", "processor", colon); model_printed = 1; }
            }
        }
        (void)fclose(file);
    }
    file = fopen("/proc/meminfo", "r");
    if (file) {
        unsigned long long kb;
        if (fscanf(file, "MemTotal: %llu kB", &kb) == 1) print_bytes("physical-memory", kb * 1024ULL);
        (void)fclose(file);
    }
}
#elif defined(__APPLE__)
static void apple_details(void) {
    char model[256];
    size_t model_size = sizeof model;
    uint64_t memory = 0;
    size_t memory_size = sizeof memory;
    if (sysctlbyname("machdep.cpu.brand_string", model, &model_size, NULL, 0) != 0) {
        model_size = sizeof model;
        if (sysctlbyname("hw.model", model, &model_size, NULL, 0) != 0) model[0] = '\0';
    }
    if (model[0]) printf("%-16s %s\n", "processor", model);
    if (sysctlbyname("hw.memsize", &memory, &memory_size, NULL, 0) == 0) print_bytes("physical-memory", memory);
}
#endif

int main(int argc, char **argv) {
    struct utsname system_info;
    long processors = -1;
    long page_size = sysconf(_SC_PAGESIZE);
    int json_output = argc == 2 && strcmp(argv[1], "--json") == 0;
    char timestamp[32];
    time_t now;
    struct tm utc;
    if (argc > 2 || (argc == 2 && !json_output)) { fprintf(stderr, "usage: %s [--json]\n", argv[0]); return 2; }
    if (uname(&system_info) != 0) { fprintf(stderr, "coreinfo: uname: %s\n", strerror(errno)); return 1; }
#if defined(__APPLE__)
    {
        int logical = 0;
        size_t logical_size = sizeof logical;
        if (sysctlbyname("hw.logicalcpu", &logical, &logical_size, NULL, 0) == 0) processors = logical;
    }
#elif defined(_SC_NPROCESSORS_ONLN)
    processors = sysconf(_SC_NPROCESSORS_ONLN);
#endif
    if (json_output) {
        now = time(NULL);
        if (gmtime_r(&now, &utc) == NULL || strftime(timestamp, sizeof timestamp, "%Y-%m-%dT%H:%M:%SZ", &utc) == 0U)
            snprintf(timestamp, sizeof timestamp, "1970-01-01T00:00:00Z");
        printf("{\"schema_version\":\"1.0\",\"tool\":\"get_system_info\",\"implementation_version\":\"c-1.0\","
               "\"request_id\":\"c-%ld-%lld\",\"observed_at_utc\":\"%s\",\"completed_at_utc\":\"%s\",\"duration_ms\":0.0,"
               "\"platform\":{\"system\":\"%s\",\"release\":\"%s\",\"machine\":\"%s\"},"
               "\"capabilities\":{\"cross_platform\":true},\"status\":\"success\","
               "\"data\":{\"system\":\"%s\",\"kernel\":\"%s\",\"architecture\":\"%s\",\"logical_cpus\":%ld,\"page_size_bytes\":%ld,\"evidence_id\":\"E0001\"},"
               "\"evidence\":[{\"evidence_id\":\"E0001\",\"kind\":\"system_info\",\"source\":\"uname/sysconf\"}],"
               "\"warnings\":[],\"errors\":[],\"coverage\":{\"scanned\":1,\"skipped\":0,\"failed\":0,\"truncated\":0,\"limit_reasons\":[]}}\n",
               (long)getpid(), (long long)now, timestamp, timestamp, system_info.sysname, system_info.release,
               system_info.machine, system_info.sysname, system_info.release, system_info.machine, processors, page_size);
        return 0;
    }
    printf("%-16s %s %s\n%-16s %s\n", "system", system_info.sysname, system_info.release,
           "architecture", system_info.machine);
    if (processors > 0) printf("%-16s %ld\n", "logical-cpus", processors);
    if (page_size > 0) printf("%-16s %ld bytes\n", "page-size", page_size);
#if defined(__linux__)
    linux_details();
#elif defined(__APPLE__)
    apple_details();
#endif
    return 0;
}
