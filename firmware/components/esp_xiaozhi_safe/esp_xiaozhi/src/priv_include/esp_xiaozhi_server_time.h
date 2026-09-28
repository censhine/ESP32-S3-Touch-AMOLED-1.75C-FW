/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <sys/time.h>
#include <time.h>

#include "cJSON.h"

/* Parse OTA server_time milliseconds into the value used by settimeofday. */
static inline bool esp_xiaozhi_server_time_parse(const cJSON *server_time, struct timeval *tv)
{
    if (!cJSON_IsObject(server_time) || tv == NULL) {
        return false;
    }
    const cJSON *timestamp = cJSON_GetObjectItemCaseSensitive(server_time, "timestamp");
    if (!cJSON_IsNumber(timestamp)) {
        return false;
    }

    // Unix timestamps are UTC. timezone_offset is presentation metadata;
    // applying it here would make localtime apply the local offset twice.
    const double ts = timestamp->valuedouble;
    tv->tv_sec = (time_t)(ts / 1000);
    tv->tv_usec = (suseconds_t)((long long)ts % 1000) * 1000;
    return true;
}
