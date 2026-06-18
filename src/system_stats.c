#include <system_stats.h>
#include <file-system.h>

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <sys/statvfs.h>
    #include <unistd.h>
#endif

static uint64_t read_disk_stats(const char *path, uint64_t *total_out, uint64_t *free_out) {
    if (total_out != NULL) {
        *total_out = 0U;
    }
    if (free_out != NULL) {
        *free_out = 0U;
    }
    if (path == NULL) {
        return 1U;
    }

#if defined(_WIN32)
    ULARGE_INTEGER free_bytes;
    ULARGE_INTEGER total_bytes;
    if (!GetDiskFreeSpaceExA(path, &free_bytes, &total_bytes, NULL)) {
        return 1U;
    }
    if (total_out != NULL) {
        *total_out = total_bytes.QuadPart;
    }
    if (free_out != NULL) {
        *free_out = free_bytes.QuadPart;
    }
    return 0U;
#else
    struct statvfs vfs;
    if (statvfs(path, &vfs) != 0) {
        return 1U;
    }
    const uint64_t total = (uint64_t)vfs.f_blocks * (uint64_t)vfs.f_frsize;
    const uint64_t free = (uint64_t)vfs.f_bavail * (uint64_t)vfs.f_frsize;
    if (total_out != NULL) {
        *total_out = total;
    }
    if (free_out != NULL) {
        *free_out = free;
    }
    return 0U;
#endif
}

static void read_memory_stats(uint64_t *total_out, uint64_t *used_out, double *percent_out) {
    if (total_out != NULL) {
        *total_out = 0U;
    }
    if (used_out != NULL) {
        *used_out = 0U;
    }
    if (percent_out != NULL) {
        *percent_out = 0.0;
    }

#if defined(_WIN32)
    MEMORYSTATUSEX status;
    memset(&status, 0, sizeof status);
    status.dwLength = sizeof status;
    if (!GlobalMemoryStatusEx(&status)) {
        return;
    }
    const uint64_t total = status.ullTotalPhys;
    const uint64_t used = total > status.ullAvailPhys ? total - status.ullAvailPhys : 0U;
    if (total_out != NULL) {
        *total_out = total;
    }
    if (used_out != NULL) {
        *used_out = used;
    }
    if (percent_out != NULL && total > 0U) {
        *percent_out = ((double)used * 100.0) / (double)total;
    }
#else
    FILE *fp = fopen("/proc/meminfo", "r");
    if (fp == NULL) {
        return;
    }

    uint64_t mem_total = 0U;
    uint64_t mem_available = 0U;
    char line[256];
    while (fgets(line, sizeof line, fp) != NULL) {
        if (strncmp(line, "MemTotal:", 9) == 0) {
            sscanf(line + 9, "%llu", (unsigned long long *)&mem_total);
            mem_total *= 1024U;
        } else if (strncmp(line, "MemAvailable:", 13) == 0) {
            sscanf(line + 13, "%llu", (unsigned long long *)&mem_available);
            mem_available *= 1024U;
        }
    }
    fclose(fp);

    if (mem_total == 0U) {
        return;
    }
    const uint64_t used = mem_total > mem_available ? mem_total - mem_available : 0U;
    if (total_out != NULL) {
        *total_out = mem_total;
    }
    if (used_out != NULL) {
        *used_out = used;
    }
    if (percent_out != NULL) {
        *percent_out = ((double)used * 100.0) / (double)mem_total;
    }
#endif
}

#if defined(_WIN32)
static uint64_t filetime_to_uint64(const FILETIME *ft) {
    ULARGE_INTEGER value;
    value.LowPart = ft->dwLowDateTime;
    value.HighPart = ft->dwHighDateTime;
    return value.QuadPart;
}
#endif

static double read_cpu_usage_percent(void) {
#if defined(_WIN32)
    static uint64_t last_idle = 0U;
    static uint64_t last_kernel = 0U;
    static uint64_t last_user = 0U;

    FILETIME idle_ft;
    FILETIME kernel_ft;
    FILETIME user_ft;
    if (!GetSystemTimes(&idle_ft, &kernel_ft, &user_ft)) {
        return 0.0;
    }

    const uint64_t idle = filetime_to_uint64(&idle_ft);
    const uint64_t kernel = filetime_to_uint64(&kernel_ft);
    const uint64_t user = filetime_to_uint64(&user_ft);

    const uint64_t idle_delta = idle - last_idle;
    const uint64_t kernel_delta = kernel - last_kernel;
    const uint64_t user_delta = user - last_user;
    last_idle = idle;
    last_kernel = kernel;
    last_user = user;

    const uint64_t total = kernel_delta + user_delta;
    if (total == 0U) {
        return 0.0;
    }
    const uint64_t busy = total > idle_delta ? total - idle_delta : 0U;
    return ((double)busy * 100.0) / (double)total;
#else
    static uint64_t last_total = 0U;
    static uint64_t last_idle = 0U;

    FILE *fp = fopen("/proc/stat", "r");
    if (fp == NULL) {
        return 0.0;
    }

    char line[256];
    if (fgets(line, sizeof line, fp) == NULL) {
        fclose(fp);
        return 0.0;
    }
    fclose(fp);

    unsigned long long user = 0U;
    unsigned long long nice = 0U;
    unsigned long long system = 0U;
    unsigned long long idle = 0U;
    unsigned long long iowait = 0U;
    unsigned long long irq = 0U;
    unsigned long long softirq = 0U;
    sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu", &user, &nice, &system, &idle,
           &iowait, &irq, &softirq);

    const uint64_t idle_total = idle + iowait;
    const uint64_t total = user + nice + system + idle + iowait + irq + softirq;
    const uint64_t total_delta = total - last_total;
    const uint64_t idle_delta = idle_total - last_idle;
    last_total = total;
    last_idle = idle_total;

    if (total_delta == 0U) {
        return 0.0;
    }
    const uint64_t busy = total_delta > idle_delta ? total_delta - idle_delta : 0U;
    return ((double)busy * 100.0) / (double)total_delta;
#endif
}

int system_stats_collect(SystemStats *out) {
    if (out == NULL) {
        return -1;
    }

    memset(out, 0, sizeof(*out));

    char *download_path = default_download_path();
    if (download_path != NULL) {
        (void)read_disk_stats(download_path, &out->disk_total_bytes, &out->disk_free_bytes);
        free(download_path);
    }

    read_memory_stats(&out->memory_total_bytes, &out->memory_used_bytes, &out->memory_used_percent);
    out->cpu_usage_percent = read_cpu_usage_percent();

    return 0;
}
