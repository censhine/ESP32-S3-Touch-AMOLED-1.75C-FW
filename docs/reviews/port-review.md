# Independent 1.75C port review

Date: 2026-09-28. Reviewer: independent Codex review agent. Source-only review; no build, test execution, serial connection, reset, flash, or device operation was performed by this reviewer. The sole written artifact is this report. This is a single-reviewer assessment, not multi-model consensus.

## Verdict

**Spec: pass for source implementation of the agreed functional port. Code quality: approved with no confirmed P1/P2 defect in the reviewed changes. Hardware/runtime acceptance: not established.**

The standalone project includes the complete 13-application source suite and Round Wing, with the C-board substitutions needed to keep the corresponding functions usable. It does not reproduce the exact original C factory binary, independent Xiaozhi firmware, or old OTA layout; the compatibility document states that distinction. No requirement-breaking application removal was found. The absence of removable SD is addressed by writable internal media, with the smaller capacity disclosed.

There are no actionable code findings to list by priority. The limitations below are release-validation boundaries, not speculative defects or requests for refactoring.

## Scope and evidence

Reviewed the implementation diff against `e2960df` in the isolated source worktree and the corresponding standalone files in this project. Read the previous runtime compatibility audit and the C-board hardware audit, then checked the changed hardware/storage code and its callers independently. Unchanged official Brookesia core was treated as an imported dependency, not re-audited in full. The project was still undergoing packaging/documentation work during review; this report does not certify later changes to installation scripts or release manifests.

### Hardware and dependencies

- `firmware/components/waveshare__esp32_s3_touch_amoled_1_75/include/bsp/esp32_s3_touch_amoled_1_75.h:38,53,54,58,59` maps MCLK16, LCD reset1, touch reset2, BOOT0 and PWR3 as specified by the recorded C schematic/reference audit. Remaining I2C, QSPI, audio data/clock and PA46 assignments are retained. The old `1_75` component/API name is intentional compatibility naming.
- `firmware/components/ButtonTest/ButtonTest.cpp:240` configures GPIO3 as an input with internal pulls disabled. The retained active-high PWR and active-low BOOT debounce/released-pressed-released validation agree with the BSS138 PWR signal described in the hardware evidence. UI labels reflect GPIO3.
- Search of firmware main/application/component source found no remaining `bsp_sdcard`, `/sdcard`, `BSP_SD_`, TCA9554 or old GPIO39/40/42 references. The SD host and expander dependencies/APIs are removed, preventing the old SD implementation from claiming C reset/key pins.
- The enhanced audio APIs, four-slot receive path, two front microphone selection, echo-reference handling, PA ownership and shutdown logic are retained. `bsp_extra` now exports the BSP and codec requirements used by its public headers. No additional PMU rail reconfiguration was introduced by this diff. The existing narrow AXP2101 settings remain subject to physical validation.

### Storage state, ownership and errors

- `firmware/components/storage_service/storage_service.c:189` uses label `media`, 4096-byte allocation units, 12 open files and `format_if_mount_failed=false`. `/media` has separate music, pictures, video, recordings, history and diagnostics directories. Directory creation failure logs and preserves readable existing files rather than rejecting the volume or deleting data.
- `storage_service.c:146,308,362` retains generation-tagged leases, rejects reuse of a live lease and refuses unmount/remount while application owners exist. Explicit unmount blocks background on-demand remount until an explicit mount/retry. Error recovery does not replace the filesystem underneath existing owners.
- `storage_service.c:197` releases an allocated wear-levelling handle after mount failure. `storage_service.c:128` clears the service handle and mounted state after the normal IDF unmount path even if the WL flush reports failure. These assumptions were checked against the installed, clean ESP-IDF 5.5.5 `vfs_fat_spiflash.c`: mount failure can retain the WL instance, while registered-volume unmount tears down FAT/VFS/context before returning its flush result.
- `storage_service.c:67` uses `esp_vfs_fat_info` for usable capacity/free bytes. The installed IDF implementation uses the actual FAT sector size, including a fixed 4096-byte configuration, so this does not report a 512-byte-sector total on the generated media image.
- The benchmark preflight rejects insufficient free bytes; subsequent short writes, flush/sync, close/read and CRC failures are handled. It exclusively creates its private temporary file and removes only that path. Capacity is checked as a preflight, not incorrectly treated as a reservation against concurrent writers.
- Recorder and chat-history paths use `/media/recordings` and `/media/history`. The retained recording implementation uses exclusive `.partial` creation and sync/checkpoint/rename, preserves nonempty partial recordings on failure, and releases storage after closing its file. The history append rollback remains applicable to IDF FAT. Diagnostics uses `/media/diagnostics`.

### Apps, framework and game

- Music scans internal music and retains its firmware-resource fallback; Gallery scans `pictures` then the compatible `photos` path when empty; Video's scan includes `video` and the existing compatible AVI folders. These paths match generated media. Updated unavailable/empty states give storage retry or supported-format guidance. Storage Settings displays internal FAT capacity and has mount/retry, unmount, benchmark and diagnostics actions.
- `firmware/main/main.cpp` installs the nine explicit apps and calls the registry containing SquareLine, Gravitysphere, Crosshair, Button Test and Round Wing, for 14 total source registrations. Registry components use `WHOLE_ARCHIVE`; the game adapter participates in the normal framework lifecycle rather than replacing the shell.
- `components/round_wing/round_wing_app.cpp` owns its LVGL timer/view, suspends on pause, defers close outside event-tree mutation, cancels pending close on cleanup and saves validated progress in the separate `roundwing/unlocked` NVS key. No C-specific dependency was found in the game view/core.
- NVS initialization returns an initialization error without whole-partition erase. This preserves data but is deliberately not a promise that every legacy NVS schema can boot or be interpreted automatically.

### Effective configuration and generated artifacts

- The current standalone `firmware/sdkconfig` selects 32 MiB DIO flash, octal 80 MHz PSRAM, USB Serial/JTAG without a secondary UART console, internal 20 KiB main stack, FAT4096/WL4096 and disabled SPIFFS format-on-failure. Dependency lock resolves ESP-IDF 5.5.5 and LVGL 9.4.0.
- `firmware/partitions.csv` is non-overlapping and ends exactly at 32 MiB: app 0x200000/8 MiB, resource SPIFFS 0xa00000/6 MiB, media 0x1000000/16 MiB, with model at 0x110000/0xf0000. NVS regions retain their declared addresses.
- At inspection time, the existing application image was 6,505,872 bytes, model 291,042 bytes, SPIFFS 6,291,456 bytes and media 16,777,216 bytes; these fit the declared partitions. File presence/size inspection is not this reviewer's assertion of a successful build or valid runtime.
- `firmware/main/CMakeLists.txt:100` generates an initial media artifact without `FLASH_IN_PROJECT`. The inspected generated `flasher_args.json` includes bootloader, partition table, otadata, app, model and resource SPIFFS, and excludes both NVS partitions and media. Thus ordinary project flashing does not implicitly replace recordings/media. First installation must explicitly initialize media using the separately packaged image.
- `firmware/tools/pack_media.py` matches the 16 MiB/4096-byte WL filesystem, supports long names, refuses existing output/input-tree output and rejects symlinks. The preparation tool generates synthetic baseline JPEG, stereo WAV and MJPEG/PCM AVI in the app scan paths.

## Validation boundaries

The parent reported passing storage host tests, including 8,000 concurrent acquire/release cycles. This reviewer inspected those tests but did not rerun them. They exercise production service code with mocked IDF/filesystem entry points and host file I/O; they do not mount the generated image through target FatFs or simulate real flash timing/power loss. Full-disk assertions cover benchmark short-write cleanup and directory-creation failure, not an end-to-end Recorder/AIChats device endurance test.

Before claiming device acceptance, verify display/touch geometry, short-press key behavior, microphone slot ordering and echo reference, speaker output, IMU orientation/calibration, USB/battery/PMU behavior, Wi-Fi and cloud activation, wake word/Chinese resources, app-to-app audio ownership, and internal heap/stack headroom under repeated use. Internal-flash recording/video throughput and full-volume recovery require the actual C board. No such test was performed in this review.

A first install changes the old factory partition map and writes new model/resource/media content. It therefore remains distinct from a normal update even though NVS addresses are preserved. The release documentation should continue to state the migration/backup boundary and avoid claiming byte-for-byte factory restoration, removable SD equivalence, firmware OTA support, or validated hardware operation.

## Scoped packaging supplement

Reviewed `tools/package-firmware.py` and `artifacts/firmware-0.1.0-candidate/manifest.json` after the main review. **No confirmed actionable finding; packaging source/manifest approved for this candidate.** No packaging command or device operation was run by this reviewer.

- The script has no subprocess, serial, device, erase, reset or flashing call. It reads fixed project build paths, refuses an existing destination, checks the exact ordinary-flash address/file mapping and DIO/32MB/80MHz configuration, checks image bounds, then copies files and writes manifest/checksum documents into a new directory.
- The seven addressed files match the reviewed partition map. The bundle omits `nvsfactory`, `nvs` and `phy_init`, contains no full-flash merged image, and records NVS `[0x9000, 0x10d000)` and PHY `[0x10f000, 0x110000)` as preserved. The nearest partition-table and otadata writes also remain outside those ranges when rounded to 4 KiB erase sectors.
- The model, app, SPIFFS and media offsets/sizes remain within their respective regions and the 32 MiB device. Media is explicitly marked `initial_media_only: true`; the note correctly says it replaces recordings/history/media and is excluded from ordinary flashing. Hardware-validation status is pending, and old-layout/schema/cloud compatibility is not overclaimed.
- The parent reports independently checked copied-file hashes/bounds, a valid application image, exact staged Chinese-font bytes, an IDF media-image extraction/hash roundtrip, successful standalone builds and LVGL 9.4 game tests. These are parent-provided validation results, not additional tests performed by this reviewer. The script itself is an offline address/size/hash packager; it does not independently parse the partition-table binary or validate device behavior.
