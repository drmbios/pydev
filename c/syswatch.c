#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__linux__)
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define TEXT_LIMIT (16U * 1024U * 1024U)
#define PATH_LIMIT 4096U

struct sample {
    unsigned long long cpu_total;
    unsigned long long cpu_idle;
    unsigned long long mem_total_kb;
    unsigned long long mem_available_kb;
    unsigned long long net_rx;
    unsigned long long net_tx;
    unsigned long long disk_read_sectors;
    unsigned long long disk_write_sectors;
    double load1;
    unsigned processes;
};

static int read_first_line(const char *path, char *buffer, size_t capacity) {
    FILE *file = fopen(path, "r");
    if (!file) return -1;
    if (!fgets(buffer, (int)capacity, file)) {
        (void)fclose(file);
        return -1;
    }
    return fclose(file) == 0 ? 0 : -1;
}

static int read_cpu(struct sample *sample) {
    char line[1024];
    unsigned long long values[10] = {0};
    int count;
    size_t index;
    if (read_first_line("/proc/stat", line, sizeof line) != 0) return -1;
    count = sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                   &values[0], &values[1], &values[2], &values[3], &values[4],
                   &values[5], &values[6], &values[7], &values[8], &values[9]);
    if (count < 4) return -1;
    sample->cpu_total = 0;
    for (index = 0; index < (size_t)count; ++index) sample->cpu_total += values[index];
    sample->cpu_idle = values[3] + values[4];
    return 0;
}

static int read_memory(struct sample *sample) {
    FILE *file = fopen("/proc/meminfo", "r");
    char key[64];
    unsigned long long value;
    char unit[16];
    int found_total = 0;
    int found_available = 0;
    if (!file) return -1;
    while (fscanf(file, "%63s %llu %15s", key, &value, unit) == 3) {
        if (strcmp(key, "MemTotal:") == 0) { sample->mem_total_kb = value; found_total = 1; }
        else if (strcmp(key, "MemAvailable:") == 0) { sample->mem_available_kb = value; found_available = 1; }
        if (found_total && found_available) break;
    }
    (void)fclose(file);
    return found_total && found_available ? 0 : -1;
}

static int read_network(struct sample *sample) {
    FILE *file = fopen("/proc/net/dev", "r");
    char line[1024];
    unsigned line_number = 0;
    if (!file) return -1;
    sample->net_rx = 0;
    sample->net_tx = 0;
    while (fgets(line, sizeof line, file)) {
        char interface_name[64];
        unsigned long long rx, tx;
        int matched;
        ++line_number;
        if (line_number <= 2U) continue;
        matched = sscanf(line, " %63[^:]: %llu %*u %*u %*u %*u %*u %*u %*u %llu",
                         interface_name, &rx, &tx);
        if (matched == 3 && strcmp(interface_name, "lo") != 0) {
            sample->net_rx += rx;
            sample->net_tx += tx;
        }
    }
    (void)fclose(file);
    return 0;
}

static int read_disk(struct sample *sample) {
    FILE *file = fopen("/proc/diskstats", "r");
    char line[1024];
    if (!file) return -1;
    sample->disk_read_sectors = 0;
    sample->disk_write_sectors = 0;
    while (fgets(line, sizeof line, file)) {
        unsigned major, minor;
        char name[64];
        unsigned long long reads, merged_reads, read_sectors, read_ms;
        unsigned long long writes, merged_writes, write_sectors;
        int fields = sscanf(line, "%u %u %63s %llu %llu %llu %llu %llu %llu %llu",
                            &major, &minor, name, &reads, &merged_reads, &read_sectors,
                            &read_ms, &writes, &merged_writes, &write_sectors);
        (void)minor; (void)reads; (void)merged_reads; (void)read_ms;
        (void)writes; (void)merged_writes;
        if (fields == 10 && major != 1U && major != 7U) {
            sample->disk_read_sectors += read_sectors;
            sample->disk_write_sectors += write_sectors;
        }
    }
    (void)fclose(file);
    return 0;
}

static int take_sample(struct sample *sample) {
    char line[256];
    memset(sample, 0, sizeof *sample);
    if (read_cpu(sample) != 0 || read_memory(sample) != 0 ||
        read_network(sample) != 0 || read_disk(sample) != 0) return -1;
    if (read_first_line("/proc/loadavg", line, sizeof line) != 0 ||
        sscanf(line, "%lf %*f %*f %*u/%u", &sample->load1, &sample->processes) != 2)
        return -1;
    return 0;
}

static void bar(const char *label, double percentage) {
    unsigned filled;
    unsigned index;
    if (percentage < 0.0) percentage = 0.0;
    if (percentage > 100.0) percentage = 100.0;
    filled = (unsigned)(percentage / 5.0 + 0.5);
    printf("%-7s [", label);
    for (index = 0; index < 20U; ++index) putchar(index < filled ? '#' : '.');
    printf("] %6.2f%%\n", percentage);
}

static FILE *open_log(const char *directory, const char *name, const char *header) {
    char path[PATH_LIMIT];
    int descriptor;
    FILE *file;
    if (snprintf(path, sizeof path, "%s/%s", directory, name) >= (int)sizeof path) return NULL;
    descriptor = open(path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (descriptor == -1) return NULL;
    file = fdopen(descriptor, "a");
    if (!file) { (void)close(descriptor); return NULL; }
    if (lseek(descriptor, 0, SEEK_END) == 0) fputs(header, file);
    return file;
}

static unsigned long long safe_delta(unsigned long long current,
                                     unsigned long long previous) {
    return current >= previous ? current - previous : 0ULL;
}

static unsigned long long sectors_to_bytes_per_second(unsigned long long current,
                                                       unsigned long long previous,
                                                       unsigned interval) {
    unsigned long long sectors = safe_delta(current, previous);
    if (sectors > ULLONG_MAX / 512ULL) return ULLONG_MAX;
    return sectors * 512ULL / interval;
}

static void close_logs(FILE **logs, size_t count) {
    size_t index;
    for (index = 0; index < count; ++index) if (logs[index]) (void)fclose(logs[index]);
}

static int show_sar(const char *path) {
    struct stat info;
    FILE *file;
    int descriptor;
    char buffer[4096];
    size_t total = 0;
    if (lstat(path, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size < 0 ||
        (unsigned long long)info.st_size > TEXT_LIMIT) {
        fputs("syswatch: SAR input must be a regular text file up to 16 MiB\n", stderr);
        return 1;
    }
    descriptor = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (descriptor == -1 || fstat(descriptor, &info) != 0 || !S_ISREG(info.st_mode)) {
        if (descriptor != -1) (void)close(descriptor);
        return 1;
    }
    file = fdopen(descriptor, "r");
    if (!file) { (void)close(descriptor); return 1; }
    printf("SAR report: %s\n", path);
    while (fgets(buffer, sizeof buffer, file)) {
        size_t index;
        size_t length = strlen(buffer);
        total += length;
        if (total > TEXT_LIMIT) { (void)fclose(file); return 1; }
        for (index = 0; index < length; ++index) {
            unsigned char byte = (unsigned char)buffer[index];
            if (byte == '\t' || byte == '\n' || (byte >= 32U && byte <= 126U)) putchar((int)byte);
            else putchar('?');
        }
    }
    return fclose(file) == 0 ? 0 : 1;
}

int main(int argc, char **argv) {
    unsigned interval = 1U, count = 0U, iteration = 0U;
    const char *log_directory = NULL;
    struct sample previous, current;
    FILE *logs[5] = {NULL, NULL, NULL, NULL, NULL};
    int index;
    for (index = 1; index < argc; ++index) {
        char *end = NULL;
        unsigned long value;
        if (strcmp(argv[index], "--sar") == 0 && index + 1 < argc) return show_sar(argv[index + 1]);
        if ((strcmp(argv[index], "--interval") == 0 || strcmp(argv[index], "--count") == 0) && index + 1 < argc) {
            value = strtoul(argv[++index], &end, 10);
            if (!end || *end != '\0' || value < 1UL || value > 86400UL) { fputs("syswatch: invalid numeric option\n", stderr); return 2; }
            if (strcmp(argv[index - 1], "--interval") == 0) interval = (unsigned)value;
            else count = (unsigned)value;
        } else if (strcmp(argv[index], "--log") == 0 && index + 1 < argc) {
            log_directory = argv[++index];
        } else { fprintf(stderr, "usage: %s [--interval SEC] [--count N] [--log DIR] | --sar TEXT_FILE\n", argv[0]); return 2; }
    }
    if (log_directory) {
        struct stat directory_info;
        if (mkdir(log_directory, 0700) != 0 && errno != EEXIST) { perror("syswatch: log directory"); return 1; }
        if (lstat(log_directory, &directory_info) != 0 || !S_ISDIR(directory_info.st_mode)) {
            fputs("syswatch: log target must be a real directory\n", stderr);
            return 1;
        }
        logs[0] = open_log(log_directory, "cpu.csv", "time,cpu_percent\n");
        logs[1] = open_log(log_directory, "memory.csv", "time,used_kb,total_kb,percent\n");
        logs[2] = open_log(log_directory, "network.csv", "time,rx_bytes_per_sec,tx_bytes_per_sec\n");
        logs[3] = open_log(log_directory, "disk.csv", "time,read_bytes_per_sec,write_bytes_per_sec\n");
        logs[4] = open_log(log_directory, "load.csv", "time,load1,processes\n");
        for (index = 0; index < 5; ++index) if (!logs[index]) { fputs("syswatch: cannot safely open log files\n", stderr); close_logs(logs, 5); return 1; }
    }
    if (take_sample(&previous) != 0) { fputs("syswatch: cannot read Linux /proc metrics\n", stderr); close_logs(logs, 5); return 1; }
    while (count == 0U || iteration < count) {
        struct timespec delay = {(time_t)interval, 0};
        unsigned long long total_delta, idle_delta, rx_rate, tx_rate, read_rate, write_rate;
        double cpu_percent, memory_percent;
        time_t now;
        while (nanosleep(&delay, &delay) != 0 && errno == EINTR) {}
        if (take_sample(&current) != 0) break;
        total_delta = safe_delta(current.cpu_total, previous.cpu_total);
        idle_delta = safe_delta(current.cpu_idle, previous.cpu_idle);
        if (idle_delta > total_delta) idle_delta = total_delta;
        cpu_percent = total_delta ? 100.0 * (double)(total_delta - idle_delta) / (double)total_delta : 0.0;
        memory_percent = current.mem_total_kb ? 100.0 * (double)(current.mem_total_kb - current.mem_available_kb) / (double)current.mem_total_kb : 0.0;
        rx_rate = safe_delta(current.net_rx, previous.net_rx) / interval;
        tx_rate = safe_delta(current.net_tx, previous.net_tx) / interval;
        read_rate = sectors_to_bytes_per_second(current.disk_read_sectors,
                                                previous.disk_read_sectors, interval);
        write_rate = sectors_to_bytes_per_second(current.disk_write_sectors,
                                                 previous.disk_write_sectors, interval);
        now = time(NULL);
        printf("\033[2J\033[Hsyswatch  load %.2f  processes %u\n", current.load1, current.processes);
        bar("CPU", cpu_percent); bar("Memory", memory_percent);
        printf("Network RX %llu B/s  TX %llu B/s\nDisk    R  %llu B/s  W  %llu B/s\n",
               rx_rate, tx_rate, read_rate, write_rate);
        if (logs[0]) {
            fprintf(logs[0], "%lld,%.2f\n", (long long)now, cpu_percent);
            fprintf(logs[1], "%lld,%llu,%llu,%.2f\n", (long long)now,
                    current.mem_total_kb - current.mem_available_kb, current.mem_total_kb, memory_percent);
            fprintf(logs[2], "%lld,%llu,%llu\n", (long long)now, rx_rate, tx_rate);
            fprintf(logs[3], "%lld,%llu,%llu\n", (long long)now, read_rate, write_rate);
            fprintf(logs[4], "%lld,%.2f,%u\n", (long long)now, current.load1, current.processes);
            for (index = 0; index < 5; ++index) (void)fflush(logs[index]);
        }
        previous = current;
        ++iteration;
    }
    close_logs(logs, 5);
    return iteration == count || count == 0U ? 0 : 1;
}

#else
int main(void) {
    fputs("syswatch: supported on Linux only\n", stderr);
    return 1;
}
#endif
