#include <cstdio>
#include <cstdlib>
#include <ctime>

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

    if (failures != 0) {
        std::fprintf(stderr, "%d server-time check(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    std::puts("All server-time UTC/Beijing regression checks passed");
    return EXIT_SUCCESS;
}
