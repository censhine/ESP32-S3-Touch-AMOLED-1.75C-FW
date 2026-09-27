# Task 2 — Internal media backend

Implemented in `ports/waveshare-1.75/firmware/brookesia/components/` in the C port worktree. No device operations, Git stage/commit, or full firmware build were performed by this task.

## Image and folder contract

- Partition label `media`; root provides a 16 MiB data/FAT partition at `0x1000000` and an explicitly generated wear-levelled FAT image.
- Mount point `/media`. Firmware uses `esp_vfs_fat_spiflash_mount_rw_wl`, `format_if_mount_failed=false`, `max_files=12`, allocation unit setting 4096. Image and firmware must agree on 4096-byte FAT/WL sectors. Formatting API is never called.
- The service has no BSP, `bsp_sdcard`, SDMMC, or SD pin dependencies.
- Service creates missing canonical directories at a successful mount. A directory creation failure is logged but does not hide an otherwise readable full volume. Writing apps still check directory/file/write failures. Existing files are never deleted to create space.

| Directory relative to image root | Meaning / supported formats |
| --- | --- |
| `music` | MP3 and WAV; existing audio-player decoders. `Music` spelling compatibility and firmware `/spiffs/music` fallback remain. |
| `pictures` | Baseline JPG/JPEG, at most 4 MiB compressed/file, up to 128 entries, 2 MiB decoded buffer cap after downscaling. `/media/photos` is a compatibility fallback when the canonical library has no usable photos. |
| `video` | MJPEG AVI, native decoded size at most 466×466, 512 KiB AVI buffer, UI capped at 10 fps. Optional 16-bit mono/stereo PCM at 8/12/16/24/32/44.1/48 kHz. Existing `videos`, `avi`, `movies`, capitalization, and root scan fallbacks remain. |
| `recordings` | Recorder writes 24 kHz, 16-bit stereo PCM WAV (`REC-*.wav`); unfinished/error recordings remain `.wav.partial` when there is recoverable PCM. |
| `history` | Optional chat history writes session JSONL, preserving default-disabled preference and append rollback. |
| `diagnostics` | Settings writes `device-diagnostics.txt`. |

The six canonical directories should be packaged into the initial image. Metadata and wear-levelling reduce usable capacity below the raw 16 MiB; `esp_vfs_fat_info` now supplies both usable capacity and free bytes using the configured filesystem sector size.

## Lifecycle and failure handling

- Initialization allocates the mutex only; first acquire or Settings Mount / retry mounts the volume.
- Lease pins are retained for complete media object lifetimes. Concurrent users share one mounted generation. Explicit remount/probe and unmount reject active owners. An error does not remount underneath live leases.
- Explicit Unmount media latches the unmounted state so background history writes cannot remount it; Mount / retry clears the latch. No UI tells users to insert, eject, or remove a card.
- Failed mounts remain errors and do not format. The local IDF 5.5 mount failure path can leave the WL instance allocated after unregistering VFS/FAT; the service releases the returned WL handle on failure before retrying.
- IDF unmount tears down VFS/WL even if WL flushing reports failure. The service invalidates mounted state/generation on that failure so a subsequent retry does not probe torn-down resources.
- Benchmark remains a private exclusively-created temporary file with readback CRC. It now rejects requests larger than current free capacity before creating a file; concurrent writers can still consume space, so short-write/flush/fsync/close checks and cleanup remain essential. Only its own temporary file is removed.
- Recorder and history keep their source write/finalization error handling: short/failed writes are errors, recorder attempts a final durable WAV checkpoint and retains recoverable partial output; history closes and truncates failed appends to the prior record boundary. FAT or hardware failures can still prevent checkpoint/rollback, which is logged; no automatic deletion or formatting is used as recovery.

## Files changed

- `storage_service/storage_service.c`, `include/storage_service.h`, `CMakeLists.txt`: internal FAT/WL backend, capacity, directory setup, lifecycle and free-space preflight.
- `storage_service/tests/`: self-contained host fault-injection harness, IDF stubs, Python build/run helper. Production C is compiled directly; tests are not part of firmware sources.
- `MusicPlayer/MusicPlayer.cpp`: internal-media naming and paths via mount macro; player/iterator ownership and SPIFFS fallback preserved.
- `Gallery/Gallery.cpp`, `.hpp`: picture folder contract, compatibility scan, internal storage retry UI.
- `VideoPlayer/VideoPlayer.cpp`, `.hpp`: internal paths/copy; unavailable storage gets distinct retry UI instead of being mistaken for missing/invalid AVI files.
- `Recorder/Recorder.cpp`: `/media/recordings`, internal recording copy/task label; recording pipeline preserved.
- `chat_history/chat_history.c`, `include/chat_history.h`: `/media/history` and internal media terminology; queue/NVS/rollback preserved.
- `Settings/Settings.cpp`, `ui/StoragePage.cpp`: internal volume identity, usable/free bytes, FAT/WL description, Mount / retry and Unmount media, updated diagnostics, clear insufficient-space benchmark result.
- `Settings/ui/AboutPage.cpp`, `Settings/CMakeLists.txt`: root-requested 1.75C product identity and firmware version from `esp_app_get_description`; private `esp_app_format` dependency.

## Verification performed

From the Brookesia source directory:

```sh
python3 components/storage_service/tests/run_host_tests.py
git diff --check
```

Both completed successfully on 2026-09-28. Host compiler uses `-Wall -Wextra -Werror -pthread -fsanitize=address,undefined`.

The fault-injection test verifies: lazy init; argument validation; mount failure without formatting and WL-handle cleanup; retry; canonical directory creation; filesystem-derived capacity/free bytes; duplicate acquire rejection; concurrent owner count; busy unmount/probe rejection; idempotent release; explicit unmount latch/re-arm; info errors while leased; idle probe recovery/remount; unmount-flush failure recovery; readable mount when directory creation gets ENOSPC; 8 threads × 1,000 acquire/release cycles; benchmark argument validation; insufficient-space preflight without file creation; mid-write ENOSPC with partial byte count, file removal and lease release; and successful uneven-sized write/read/CRC round trip.

Source inspection confirms no `/sdcard`, SDMMC, `bsp_sdcard` calls or card insertion/removal instructions remain in the owned application/storage files. Legacy API field/function identifiers (`card_name`, `safe_eject`, enum `EJECTING`) remain compatible, with internal-volume semantics and labels.

## Limits and remaining integration checks

- Host tests mock IDF/FAT/WL and exercise the service state machine and POSIX error cleanup; they do not validate flash hardware, generated FAT layout, wear levelling persistence, actual codec playback or LVGL appearance. Full firmware build and image/partition validation are owned by root.
- Runtime hardware checks still needed: mount packaged media, play MP3/WAV/JPEG/AVI, record stereo WAV, verify free-space change, chat history/diagnostics, busy unmount, explicit unmount/retry, and full-volume writes. This work does not claim on-device verification.
- The 16 MiB volume offers under three minutes of 24 kHz stereo 16-bit PCM before accounting for other media, FAT/WL overhead and partial files. There is no automatic space reclamation. Initial image replacement is an explicit provisioning operation and overwrites existing recordings/history; packaging documentation must state that.
- Internal media is not exposed as USB mass storage by this task. Offline image provisioning/export workflows remain root-owned.
