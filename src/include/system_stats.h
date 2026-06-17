#ifndef AVAR_SYSTEM_STATS_H
#define AVAR_SYSTEM_STATS_H

#include <stdint.h>

typedef struct {
    uint64_t disk_total_bytes;
    uint64_t disk_free_bytes;
    uint64_t memory_total_bytes;
    uint64_t memory_used_bytes;
    double memory_used_percent;
    double cpu_usage_percent;
    uint64_t network_rx_bytes_per_sec;
    uint64_t network_tx_bytes_per_sec;
} SystemStats;

/* Collects disk (download path), memory, CPU, and network throughput samples. */
int system_stats_collect(SystemStats *out);

#endif
