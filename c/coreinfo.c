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

int main(void) {
    struct utsname system_info;
    long processors = -1;
    long page_size = sysconf(_SC_PAGESIZE);
    if (uname(&system_info) != 0) { fprintf(stderr, "coreinfo: uname: %s\n", strerror(errno)); return 1; }
    printf("%-16s %s %s\n%-16s %s\n", "system", system_info.sysname, system_info.release,
           "architecture", system_info.machine);
#if defined(__APPLE__)
    {
        int logical = 0;
        size_t logical_size = sizeof logical;
        if (sysctlbyname("hw.logicalcpu", &logical, &logical_size, NULL, 0) == 0) processors = logical;
    }
#elif defined(_SC_NPROCESSORS_ONLN)
    processors = sysconf(_SC_NPROCESSORS_ONLN);
#endif
    if (processors > 0) printf("%-16s %ld\n", "logical-cpus", processors);
    if (page_size > 0) printf("%-16s %ld bytes\n", "page-size", page_size);
#if defined(__linux__)
    linux_details();
#elif defined(__APPLE__)
    apple_details();
#endif
    return 0;
}
