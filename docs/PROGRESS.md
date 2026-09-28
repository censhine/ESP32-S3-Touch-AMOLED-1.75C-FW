# Integration progress

2026-09-28: independent project extracted from verified Waveshare1.75 sources.
Hardware adaptation: complete at source level.
Internal media adaptation: complete at source level, fault-injection tests passed.
Round Wing integration: complete, all14apps linked.
Full build: passed in this independent project with ESP-IDF5.5.5.
Native LVGL9.4 game tests: passed.
Images/partition/media round-trip/hashes: passed.
Independent source review and packaging supplement: approved.
Device operations: authorized full backup, flash and verification completed on2026-09-28. First boot reached desktop ready and installed14apps with no panic/reboot loop during45s capture. Physical interaction/voice/media acceptance remains pending. See DEVICE_INSTALL_ZH.md.

The upstream-to-c-port.patch records firmware adaptations relative to the imported application baseline. Independent project relocation (firmware/ and components/) is documented in SOURCES.md and represented in final CMake files. Unchanged imported sources and generated assets retain their upstream formatting.

2026-09-28 runtime follow-up: corrected MusicPlayer EOF/time display and moved TLS allocations to PSRAM after an observed Xiaozhi prompt-task allocation failure. Nine targeted host checks and firmware build passed. App-only flash/readback and unchanged settings-region verification passed; updated desktop booted. Xiaozhi wake/conversation subsequently confirmed by device logs and user. Success prompt resource remains unavailable. See RUNTIME_FIX_ZH.md.

Playback reboot follow-up: decoded two crashes to VideoPlayer AVI flash-reader task running on PSRAM stack. Changed reader stack to internal SRAM; build and independent review passed. Corrective app image is ready; USB was disconnected before flash, so device verification is pending.
