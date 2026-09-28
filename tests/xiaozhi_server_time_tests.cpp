#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <limits>

#include "esp_xiaozhi_server_time.h"

namespace {

int failures = 0;

void check(bool condition, const char *test, const char *expectation)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL %s: %s\n", test, expectation);
        ++failures;
    }
}

void check_time(const char *name, const char *json, suseconds_t expected_usec)
{
    cJSON *root = cJSON_Parse(json);
    check(root != nullptr, name, "JSON parses with production cJSON");
    if (!root) {
        return;
    }
    const cJSON *server_time = cJSON_GetObjectItemCaseSensitive(root, "server_time");
    timeval tv = {};
    const bool parsed = esp_xiaozhi_server_time_parse(server_time, &tv);
    check(parsed, name, "server_time parses");
    if (parsed) {
        if (tv.tv_sec != 1790611200) {
            std::fprintf(stderr, "%s: epoch=%lld expected=1790611200 delta=%lld s\n",
                         name, static_cast<long long>(tv.tv_sec),
                         static_cast<long long>(tv.tv_sec) - 1790611200LL);
        }
        check(tv.tv_sec == 1790611200, name, "epoch remains UTC regardless of timezone_offset");
        check(tv.tv_usec == expected_usec, name, "milliseconds preserved as microseconds");
        tm beijing = {};
        check(localtime_r(&tv.tv_sec, &beijing) != nullptr, name, "localtime conversion succeeds");
        check(beijing.tm_year == 126 && beijing.tm_mon == 8 && beijing.tm_mday == 29 &&
                  beijing.tm_hour == 0 && beijing.tm_min == 0 && beijing.tm_sec == 0,
              name, "CST-8 localtime is 2026-09-29 00:00:00");
    }
    cJSON_Delete(root);
}

void check_rejected(const char *name, const cJSON *server_time)
{
    timeval tv = {123456789, 654321};
    check(!esp_xiaozhi_server_time_parse(server_time, &tv), name,
          "invalid timestamp is rejected");
    check(tv.tv_sec == 123456789 && tv.tv_usec == 654321, name,
          "rejection preserves both output fields");
}

void check_rejected_json(const char *name, const char *json)
{
    cJSON *server_time = cJSON_Parse(json);
    check(server_time != nullptr, name, "JSON parses with production cJSON");
    if (server_time) {
        check_rejected(name, server_time);
    }
    cJSON_Delete(server_time);
}

void check_boundary(const char *name, const char *json,
                    long long expected_seconds, suseconds_t expected_usec)
{
    cJSON *server_time = cJSON_Parse(json);
    check(server_time != nullptr, name, "JSON parses with production cJSON");
    if (server_time) {
        timeval tv = {};
        check(esp_xiaozhi_server_time_parse(server_time, &tv), name,
              "supported timestamp is accepted");
        check(static_cast<long long>(tv.tv_sec) == expected_seconds &&
                  tv.tv_usec == expected_usec,
              name, "boundary seconds and microseconds are preserved");
    }
    cJSON_Delete(server_time);
}

} // namespace

int main()
{
    if (setenv("TZ", "CST-8", 1) != 0) {
        std::perror("setenv TZ");
        return EXIT_FAILURE;
    }
    tzset();

    check_time("Beijing offset", R"({"server_time":{"timestamp":1790611200000,"timezone_offset":480}})", 0);
    check_time("UTC offset", R"({"server_time":{"timestamp":1790611200000,"timezone_offset":0}})", 0);
    check_time("West offset", R"({"server_time":{"timestamp":1790611200000,"timezone_offset":-300}})", 0);
    check_time("Half-hour offset", R"({"server_time":{"timestamp":1790611200000,"timezone_offset":330}})", 0);
    check_time("Missing offset and milliseconds", R"({"server_time":{"timestamp":1790611200123}})", 123000);
    check_time("Beijing offset and milliseconds", R"({"server_time":{"timestamp":1790611200999,"timezone_offset":480}})", 999000);

    cJSON *invalid = cJSON_Parse(R"({"timestamp":"1790611200000"})");
    timeval tv = {};
    check(!esp_xiaozhi_server_time_parse(invalid, &tv), "Invalid timestamp", "non-numeric timestamp rejected");
    check(!esp_xiaozhi_server_time_parse(nullptr, &tv), "Missing server_time", "missing object rejected");
    cJSON_Delete(invalid);

    check_rejected_json("Negative timestamp", R"({"timestamp":-1})");
    check_rejected_json("Negative fractional timestamp", R"({"timestamp":-0.5})");
    check_rejected_json("Huge finite timestamp", R"({"timestamp":1e100})");
    check_rejected_json("Overflowing exponent", R"({"timestamp":1e999})");
    check_rejected_json("Year 10000 excluded", R"({"timestamp":253402300800000})");
    check_rejected_json("Beyond year 10000", R"({"timestamp":253402300800001})");

    cJSON *nan_time = cJSON_Parse(R"({"timestamp":0})");
    check(nan_time != nullptr, "NaN timestamp", "test object allocated");
    if (nan_time) {
        // JSON has no NaN literal; retain the numeric type and supply NaN directly.
        cJSON_GetObjectItemCaseSensitive(nan_time, "timestamp")->valuedouble =
            std::numeric_limits<double>::quiet_NaN();
        check_rejected("NaN timestamp", nan_time);
    }
    cJSON_Delete(nan_time);

    check_boundary("Unix epoch", R"({"timestamp":0})", 0, 0);
    check_boundary("Last millisecond of year 9999",
                   R"({"timestamp":253402300799999})", 253402300799LL, 999000);

    if (failures != 0) {
        std::fprintf(stderr, "%d server-time check(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    std::puts("All server-time UTC/Beijing regression checks passed");
    return EXIT_SUCCESS;
}
