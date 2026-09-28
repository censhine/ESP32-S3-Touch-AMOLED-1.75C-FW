#include "host_runtime.h"

#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/event_groups.h"
#include "freertos/idf_additions.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

struct host_event_group {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    EventBits_t bits;
};

struct host_semaphore {
    pthread_mutex_t mutex;
};

struct host_task {
    TaskFunction_t function;
    void *argument;
    bool with_caps;
    UBaseType_t stack_caps;
};

struct host_timer {
    void (*callback)(void *argument);
    void *argument;
    bool running;
};

static _Thread_local TaskHandle_t s_current_task;
static atomic_int s_open_files;
static atomic_int s_fread_calls;
static atomic_int s_fread_fail_after = -1;
static atomic_int s_live_tasks;
static atomic_int s_caps_tasks_created;
static atomic_int s_caps_tasks_deleted;
static atomic_uint s_last_stack_size;
static atomic_uint s_last_stack_caps;
static atomic_int s_internal_stack_reads;
static atomic_int s_other_stack_reads;

host_task_stats_t host_task_stats(void)
{
    return (host_task_stats_t) {
        .live_tasks = atomic_load(&s_live_tasks),
        .caps_tasks_created = atomic_load(&s_caps_tasks_created),
        .caps_tasks_deleted = atomic_load(&s_caps_tasks_deleted),
        .last_stack_size = atomic_load(&s_last_stack_size),
        .last_stack_caps = atomic_load(&s_last_stack_caps),
        .internal_stack_reads = atomic_load(&s_internal_stack_reads),
        .other_stack_reads = atomic_load(&s_other_stack_reads),
    };
}

void host_reset_task_stats(void)
{
    assert(atomic_load(&s_live_tasks) == 0);
    atomic_store(&s_caps_tasks_created, 0);
    atomic_store(&s_caps_tasks_deleted, 0);
    atomic_store(&s_last_stack_size, 0);
    atomic_store(&s_last_stack_caps, 0);
    atomic_store(&s_internal_stack_reads, 0);
    atomic_store(&s_other_stack_reads, 0);
}

static void delay_milliseconds(long milliseconds)
{
    const struct timespec delay = {
        .tv_sec = milliseconds / 1000,
        .tv_nsec = (milliseconds % 1000) * 1000000L,
    };
    nanosleep(&delay, NULL);
}

const char *esp_err_to_name(esp_err_t error)
{
    switch (error) {
    case ESP_OK: return "ESP_OK";
    case ESP_FAIL: return "ESP_FAIL";
    case ESP_ERR_NO_MEM: return "ESP_ERR_NO_MEM";
    case ESP_ERR_INVALID_ARG: return "ESP_ERR_INVALID_ARG";
    case ESP_ERR_INVALID_STATE: return "ESP_ERR_INVALID_STATE";
    case ESP_ERR_INVALID_SIZE: return "ESP_ERR_INVALID_SIZE";
    case ESP_ERR_NOT_SUPPORTED: return "ESP_ERR_NOT_SUPPORTED";
    case ESP_ERR_TIMEOUT: return "ESP_ERR_TIMEOUT";
    default: return "ESP_ERR_UNKNOWN";
    }
}

FILE *host_tracked_fopen(const char *path, const char *mode)
{
    FILE *file = fopen(path, mode);
    if (file != NULL) {
        atomic_fetch_add(&s_open_files, 1);
    }
    return file;
}

int host_tracked_fclose(FILE *file)
{
    const int result = fclose(file);
    atomic_fetch_sub(&s_open_files, 1);
    return result;
}

size_t host_tracked_fread(void *buffer, size_t size, size_t count, FILE *file)
{
    // Observe the task that actually reaches stdio, not just its init config.
    if (s_current_task != NULL && s_current_task->with_caps &&
        (s_current_task->stack_caps & MALLOC_CAP_INTERNAL) != 0 &&
        (s_current_task->stack_caps & MALLOC_CAP_SPIRAM) == 0) {
        atomic_fetch_add(&s_internal_stack_reads, 1);
    } else {
        atomic_fetch_add(&s_other_stack_reads, 1);
    }
    const int call_index = atomic_fetch_add(&s_fread_calls, 1);
    const int fail_after = atomic_load(&s_fread_fail_after);
    if (fail_after >= 0 && call_index >= fail_after) {
        return 0;
    }
    return fread(buffer, size, count, file);
}

int host_open_file_count(void)
{
    return atomic_load(&s_open_files);
}

void host_fail_fread_after(int successful_calls)
{
    assert(successful_calls >= 0);
    atomic_store(&s_fread_calls, 0);
    atomic_store(&s_fread_fail_after, successful_calls);
}

void host_clear_fread_failure(void)
{
    atomic_store(&s_fread_fail_after, -1);
    atomic_store(&s_fread_calls, 0);
}

EventGroupHandle_t xEventGroupCreate(void)
{
    EventGroupHandle_t group = calloc(1, sizeof(*group));
    if (group != NULL) {
        pthread_mutex_init(&group->mutex, NULL);
        pthread_cond_init(&group->condition, NULL);
    }
    return group;
}

EventBits_t xEventGroupSetBits(EventGroupHandle_t group, EventBits_t bits)
{
    pthread_mutex_lock(&group->mutex);
    group->bits |= bits;
    const EventBits_t result = group->bits;
    pthread_cond_broadcast(&group->condition);
    pthread_mutex_unlock(&group->mutex);
    return result;
}

EventBits_t xEventGroupClearBits(EventGroupHandle_t group, EventBits_t bits)
{
    pthread_mutex_lock(&group->mutex);
    const EventBits_t previous = group->bits;
    group->bits &= ~bits;
    pthread_mutex_unlock(&group->mutex);
    return previous;
}

EventBits_t xEventGroupWaitBits(
    EventGroupHandle_t group,
    EventBits_t bits,
    BaseType_t clear_on_exit,
    BaseType_t wait_for_all,
    TickType_t ticks_to_wait
)
{
    pthread_mutex_lock(&group->mutex);
    while (true) {
        const EventBits_t matching = group->bits & bits;
        if ((wait_for_all && matching == bits) || (!wait_for_all && matching != 0)) {
            break;
        }
        if (ticks_to_wait == 0) {
            pthread_mutex_unlock(&group->mutex);
            return 0;
        }
        pthread_cond_wait(&group->condition, &group->mutex);
    }
    pthread_mutex_unlock(&group->mutex);

    // Let requests that were issued back-to-back coalesce, which exercises the
    // worker's explicit DEINIT > STOP > START precedence.
    delay_milliseconds(2);

    pthread_mutex_lock(&group->mutex);
    const EventBits_t result = group->bits;
    if (clear_on_exit) {
        group->bits &= ~bits;
    }
    pthread_mutex_unlock(&group->mutex);
    return result;
}

void vEventGroupDelete(EventGroupHandle_t group)
{
    pthread_cond_destroy(&group->condition);
    pthread_mutex_destroy(&group->mutex);
    free(group);
}

SemaphoreHandle_t xSemaphoreCreateMutex(void)
{
    SemaphoreHandle_t semaphore = calloc(1, sizeof(*semaphore));
    if (semaphore != NULL) {
        pthread_mutex_init(&semaphore->mutex, NULL);
    }
    return semaphore;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t semaphore, TickType_t ticks_to_wait)
{
    (void)ticks_to_wait;
    return pthread_mutex_lock(&semaphore->mutex) == 0 ? pdTRUE : pdFALSE;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t semaphore)
{
    return pthread_mutex_unlock(&semaphore->mutex) == 0 ? pdTRUE : pdFALSE;
}

void vSemaphoreDelete(SemaphoreHandle_t semaphore)
{
    pthread_mutex_destroy(&semaphore->mutex);
    free(semaphore);
}

static void task_cleanup(void *argument)
{
    free(argument);
    s_current_task = NULL;
    atomic_fetch_sub(&s_live_tasks, 1);
}

static void *task_entry(void *argument)
{
    TaskHandle_t task = argument;
    s_current_task = task;
    pthread_cleanup_push(task_cleanup, task);
    task->function(task->argument);
    pthread_cleanup_pop(1);
    return NULL;
}

static BaseType_t create_task(
    TaskFunction_t function,
    const char *name,
    uint32_t stack_size,
    void *argument,
    UBaseType_t priority,
    TaskHandle_t *task_handle,
    BaseType_t core_id,
    bool with_caps,
    UBaseType_t stack_caps
)
{
    (void)name;
    (void)priority;
    (void)core_id;
    TaskHandle_t task = calloc(1, sizeof(*task));
    if (task == NULL) {
        return pdFAIL;
    }
    task->function = function;
    task->argument = argument;
    task->with_caps = with_caps;
    task->stack_caps = stack_caps;
    atomic_fetch_add(&s_live_tasks, 1);
    *task_handle = task;
    pthread_t thread;
    if (pthread_create(&thread, NULL, task_entry, task) != 0) {
        atomic_fetch_sub(&s_live_tasks, 1);
        *task_handle = NULL;
        free(task);
        return pdFAIL;
    }
    pthread_detach(thread);
    atomic_store(&s_last_stack_size, stack_size);
    atomic_store(&s_last_stack_caps, stack_caps);
    if (with_caps) {
        atomic_fetch_add(&s_caps_tasks_created, 1);
    }
    return pdPASS;
}

BaseType_t xTaskCreatePinnedToCore(
    TaskFunction_t function, const char *name, uint32_t stack_size,
    void *argument, UBaseType_t priority, TaskHandle_t *task_handle,
    BaseType_t core_id
)
{
    return create_task(function, name, stack_size, argument, priority,
                       task_handle, core_id, false, MALLOC_CAP_INTERNAL);
}

BaseType_t xTaskCreatePinnedToCoreWithCaps(
    TaskFunction_t function, const char *name, uint32_t stack_size,
    void *argument, UBaseType_t priority, TaskHandle_t *task_handle,
    BaseType_t core_id, UBaseType_t stack_caps
)
{
    return create_task(function, name, stack_size, argument, priority,
                       task_handle, core_id, true, stack_caps);
}

TaskHandle_t xTaskGetCurrentTaskHandle(void)
{
    return s_current_task;
}

void vTaskDelete(TaskHandle_t task_handle)
{
    assert(task_handle == NULL || task_handle == s_current_task);
    assert(s_current_task != NULL && !s_current_task->with_caps);
    pthread_exit(NULL);
}

void vTaskDeleteWithCaps(TaskHandle_t task_handle)
{
    assert(task_handle == NULL || task_handle == s_current_task);
    assert(s_current_task != NULL && s_current_task->with_caps);
    atomic_fetch_add(&s_caps_tasks_deleted, 1);
    pthread_exit(NULL);
}

esp_err_t esp_timer_create(const esp_timer_create_args_t *args, esp_timer_handle_t *handle)
{
    if (args == NULL || args->callback == NULL || handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_timer_handle_t timer = calloc(1, sizeof(*timer));
    if (timer == NULL) {
        return ESP_ERR_NO_MEM;
    }
    timer->callback = args->callback;
    timer->argument = args->arg;
    *handle = timer;
    return ESP_OK;
}

esp_err_t esp_timer_start_periodic(esp_timer_handle_t handle, uint64_t period_us)
{
    (void)period_us;
    if (handle == NULL || handle->running) {
        return ESP_ERR_INVALID_STATE;
    }
    handle->running = true;
    handle->callback(handle->argument);
    return ESP_OK;
}

esp_err_t esp_timer_stop(esp_timer_handle_t handle)
{
    if (handle == NULL || !handle->running) {
        return ESP_ERR_INVALID_STATE;
    }
    handle->running = false;
    return ESP_OK;
}

esp_err_t esp_timer_delete(esp_timer_handle_t handle)
{
    if (handle == NULL || handle->running) {
        return ESP_ERR_INVALID_STATE;
    }
    free(handle);
    return ESP_OK;
}
