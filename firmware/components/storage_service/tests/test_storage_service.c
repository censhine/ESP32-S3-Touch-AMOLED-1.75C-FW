/* Host fault-injection tests execute the production service with a mocked IDF
 * backend and pthread mutexes. No device, flash partition or /media is touched. */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include "test_sdk.h"
#include "storage_service.h"

static esp_err_t mount_error, info_error, unmount_error;
static bool backend_mounted, leak_failed_mount, fail_directories;
static unsigned mounts, unmounts, wl_releases, mkdir_calls, temp_creates, temp_removes;
static uint64_t total_bytes = 16000000, free_bytes = 12000000;
static size_t write_limit = SIZE_MAX;
static char temp_path[128];

esp_err_t esp_vfs_fat_spiflash_mount_rw_wl(const char *path, const char *label,
                                        const esp_vfs_fat_mount_config_t *cfg, wl_handle_t *handle) {
    assert(strcmp(path, "/media") == 0 && strcmp(label, "media") == 0);
    assert(!cfg->format_if_mount_failed && cfg->max_files >= 8);
    assert(cfg->allocation_unit_size == 4096);
    assert(!backend_mounted);
    ++mounts;
    *handle = (!mount_error || leak_failed_mount) ? 42 : WL_INVALID_HANDLE;
    backend_mounted = mount_error == ESP_OK;
    return mount_error;
}
esp_err_t esp_vfs_fat_spiflash_unmount_rw_wl(const char *path, wl_handle_t handle) {
    assert(strcmp(path, "/media") == 0 && handle == 42 && backend_mounted);
    ++unmounts;
    // Match IDF 5.5: teardown is complete even when WL flush returns an error.
    backend_mounted = false;
    return unmount_error;
}
esp_err_t wl_unmount(wl_handle_t handle) { assert(handle == 42); ++wl_releases; return ESP_OK; }
esp_err_t esp_vfs_fat_info(const char *path, uint64_t *total, uint64_t *available) {
    assert(strcmp(path, "/media") == 0 && backend_mounted);
    if (info_error) return info_error;
    *total = total_bytes; *available = free_bytes;
    return ESP_OK;
}
static int test_stat(const char *path, struct stat *status) {
    assert(strncmp(path, "/media/", 7) == 0);
    (void)status;
    errno = ENOENT;
    return -1;
}
static int test_mkdir(const char *path, mode_t mode) {
    assert(strncmp(path, "/media/", 7) == 0); (void)mode;
    ++mkdir_calls;
    if (fail_directories) { errno = ENOSPC; return -1; }
    return 0;
}
static int test_open(const char *path, int flags, ...) {
    assert(strncmp(path, "/media/.waveshare-media-benchmark-", 33) == 0);
    assert((flags & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL));
    strcpy(temp_path, "/tmp/media-storage-test-XXXXXX");
    ++temp_creates;
    return mkstemp(temp_path);
}
static FILE *test_fopen(const char *path, const char *mode) {
    assert(strncmp(path, "/media/", 7) == 0);
    return fopen(temp_path, mode);
}
static int test_remove(const char *path) {
    assert(strncmp(path, "/media/", 7) == 0);
    ++temp_removes;
    return remove(temp_path);
}
static size_t test_fwrite(const void *data, size_t size, size_t count, FILE *file) {
    assert(size == 1);
    const size_t allowed = count < write_limit ? count : write_limit;
    const size_t written = fwrite(data, size, allowed, file);
    if (write_limit != SIZE_MAX) write_limit -= written;
    if (written < count) errno = ENOSPC;
    return written;
}
#define stat(...) test_stat(__VA_ARGS__)
#define mkdir test_mkdir
#define open test_open
#define fopen test_fopen
#define remove test_remove
#define fwrite test_fwrite
#include "../storage_service.c"
#undef stat
#undef mkdir
#undef open
#undef fopen
#undef remove
#undef fwrite

static storage_service_info_t snapshot(void) {
    storage_service_info_t info;
    assert(storage_service_get_info(&info) == ESP_OK);
    return info;
}
static void *lease_worker(void *unused) {
    (void)unused;
    for (unsigned i = 0; i < 1000; ++i) {
        storage_service_lease_t lease = {0};
        assert(storage_service_acquire(&lease) == ESP_OK);
        assert(storage_service_safe_eject() == ESP_ERR_INVALID_STATE);
        storage_service_release(&lease);
        assert(!lease.active && lease.generation == 0);
    }
    return NULL;
}
int main(void) {
    assert(storage_service_init() == ESP_OK);
    assert(snapshot().state == STORAGE_SERVICE_STATE_UNMOUNTED && mounts == 0);
    assert(storage_service_acquire(NULL) == ESP_ERR_INVALID_ARG);
    assert(storage_service_get_info(NULL) == ESP_ERR_INVALID_ARG);
    storage_service_lease_t one = {0}, two = {0};

    mount_error = ESP_FAIL; leak_failed_mount = true;
    assert(storage_service_acquire(&one) == ESP_FAIL);
    assert(!one.active && snapshot().state == STORAGE_SERVICE_STATE_ERROR);
    assert(wl_releases == 1); // failed mount did not leak its driver
    mount_error = ESP_OK;
    assert(storage_service_acquire(&one) == ESP_OK);
    assert(mkdir_calls == 6);
    assert(snapshot().capacity_bytes == total_bytes && snapshot().free_bytes == free_bytes);
    assert(storage_service_acquire(&one) == ESP_ERR_INVALID_STATE);
    assert(storage_service_acquire(&two) == ESP_OK);
    const unsigned before = mounts;
    assert(snapshot().active_leases == 2);
    assert(storage_service_mount() == ESP_ERR_INVALID_STATE);
    assert(storage_service_safe_eject() == ESP_ERR_INVALID_STATE && unmounts == 0);
    storage_service_release(&one); storage_service_release(&one);
    assert(snapshot().active_leases == 1 && mounts == before);
    storage_service_release(&two);
    assert(storage_service_safe_eject() == ESP_OK);
    assert(storage_service_acquire(&one) == ESP_ERR_INVALID_STATE); // explicit unmount stays unmounted
    assert(storage_service_mount() == ESP_OK);
    assert(storage_service_acquire(&one) == ESP_OK);

    info_error = ESP_FAIL;
    storage_service_info_t info;
    assert(storage_service_get_info(&info) == ESP_FAIL && info.state == STORAGE_SERVICE_STATE_ERROR);
    assert(storage_service_acquire(&two) == ESP_FAIL && mounts == before + 1);
    storage_service_release(&one);
    info_error = ESP_OK;
    assert(storage_service_acquire(&two) == ESP_OK);
    storage_service_release(&two);

    const unsigned before_recovery = mounts;
    info_error = ESP_FAIL;
    assert(storage_service_mount() == ESP_OK); // bad idle probe tears down and remounts
    info_error = ESP_OK;
    assert(mounts == before_recovery + 1 && snapshot().state == STORAGE_SERVICE_STATE_MOUNTED);

    unmount_error = ESP_FAIL;
    assert(storage_service_safe_eject() == ESP_FAIL);
    assert(snapshot().state == STORAGE_SERVICE_STATE_ERROR && !backend_mounted);
    unmount_error = ESP_OK;
    fail_directories = true;
    assert(storage_service_mount() == ESP_OK); // full directory table doesn't hide readable media
    fail_directories = false;

    pthread_t workers[8];
    for (unsigned i = 0; i < 8; ++i) assert(pthread_create(&workers[i], NULL, lease_worker, NULL) == 0);
    for (unsigned i = 0; i < 8; ++i) assert(pthread_join(workers[i], NULL) == 0);
    assert(snapshot().active_leases == 0);

    storage_service_benchmark_t benchmark;
    assert(storage_service_run_benchmark(0, &benchmark) == ESP_ERR_INVALID_ARG);
    assert(storage_service_run_benchmark(4096, NULL) == ESP_ERR_INVALID_ARG);
    free_bytes = 1024;
    assert(storage_service_run_benchmark(4096, &benchmark) == ESP_ERR_INVALID_SIZE);
    assert(temp_creates == 0 && snapshot().active_leases == 0);
    free_bytes = 12000000;
    write_limit = 20000; // disk fills after the initial preflight, during the second chunk
    assert(storage_service_run_benchmark(32768, &benchmark) == ESP_FAIL);
    assert(benchmark.written_bytes == 20000 && benchmark.read_bytes == 0);
    assert(temp_creates == 1 && temp_removes == 1 && snapshot().active_leases == 0);
    write_limit = SIZE_MAX;
    assert(storage_service_run_benchmark(32769, &benchmark) == ESP_OK);
    assert(benchmark.written_bytes == 32769 && benchmark.read_bytes == 32769);
    assert(benchmark.actual_crc32 == benchmark.expected_crc32);
    assert(temp_creates == 2 && temp_removes == 2 && snapshot().active_leases == 0);
    assert(storage_service_safe_eject() == ESP_OK);
    puts("PASS: mount failures, retry, usable capacity, leases, unmount failures, 8000 concurrent acquire/release cycles, full-disk cleanup, benchmark CRC");
    return 0;
}
