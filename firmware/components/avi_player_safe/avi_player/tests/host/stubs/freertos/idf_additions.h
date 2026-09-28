#pragma once

#include "freertos/task.h"

BaseType_t xTaskCreatePinnedToCoreWithCaps(
    TaskFunction_t function,
    const char *name,
    uint32_t stack_size,
    void *argument,
    UBaseType_t priority,
    TaskHandle_t *task_handle,
    BaseType_t core_id,
    UBaseType_t stack_caps
);
void vTaskDeleteWithCaps(TaskHandle_t task_handle);
