#pragma once

#include <stdio.h>
#include <stdint.h>

typedef struct {
    int live_tasks;
    int caps_tasks_created;
    int caps_tasks_deleted;
    uint32_t last_stack_size;
    uint32_t last_stack_caps;
    int internal_stack_reads;
    int other_stack_reads;
} host_task_stats_t;

host_task_stats_t host_task_stats(void);
void host_reset_task_stats(void);

FILE *host_tracked_fopen(const char *path, const char *mode);
int host_tracked_fclose(FILE *file);
size_t host_tracked_fread(void *buffer, size_t size, size_t count, FILE *file);
int host_open_file_count(void);
void host_fail_fread_after(int successful_calls);
void host_clear_fread_failure(void);
