# Task 1: 1.75C hardware adaptation

## Changed files

- `ports/waveshare-1.75/firmware/brookesia/components/waveshare__esp32_s3_touch_amoled_1_75/include/bsp/esp32_s3_touch_amoled_1_75.h`: 1.75C LCD reset GPIO1, touch reset GPIO2, MCLK GPIO16, BOOT GPIO0, PWR GPIO3; SD capability off and SD/expander declarations removed. Corrected the shared I2C device list.
- `ports/waveshare-1.75/firmware/brookesia/components/waveshare__esp32_s3_touch_amoled_1_75/esp32_s3_touch_amoled_1_75.c`: removed SDMMC mount/unmount and TCA9554 initialization; corrected board log tag and touch reset comment. Enhanced audio implementation remains.
- `ports/waveshare-1.75/firmware/brookesia/components/waveshare__esp32_s3_touch_amoled_1_75/{CMakeLists.txt,Kconfig,idf_component.yml,README.md}`: removed SDMMC/FAT/expander dependencies and SD configuration; documented local C adaptation.
- `ports/waveshare-1.75/firmware/brookesia/components/ButtonTest/{ButtonTest.cpp,ButtonTest.hpp}`: samples GPIO3 as an input without internal pulls; preserves active-high PWR and active-low BOOT, debounce, and released-pressed-released pass sequence. Updated labels and logs.
- `ports/waveshare-1.75/firmware/brookesia/components/bsp_extra/{CMakeLists.txt,src/bsp_board_extra.c,README.md,README_ZH.md}`: removed unused FAT dependency/include, declared the public `espressif__esp_codec_dev` dependency required by `bsp_board_extra.h`, and changed SD-specific audio lifecycle wording to media storage. Audio behavior is unchanged.

## Verification

- `git diff --check` on the three owned components: clean.
- Source search across owned components found no SDMMC mount, `bsp_sdcard`, `bsp_io_expander`, or expander API references. The BSP README retains a descriptive mention of absent TCA9554 hardware.
- Pin and audio source inspection confirms LCD reset GPIO1, touch reset GPIO2, MCLK GPIO16, PWR GPIO3, BOOT GPIO0, 3.3 V PA, and the existing `bsp_audio_init_voice_24k()` / `bsp_audio_deinit()` path.
- Root's integrated build first exposed the missing public `esp_codec_dev.h` include path in `bsp_extra`; its CMake dependency is now explicit. Root will rerun the full build. No device operation was performed.

## Integration notes

- `storage_service` must use its new internal media backend and must not refer to removed `bsp_sdcard` APIs.
- Top-level README files and `HARDWARE_VALIDATION.md` / `_ZH.md` section 3 still describe PWR as TCA9554 EXIO4; root should update those outside this task's component ownership.
- Physical C-board validation is still required for display/touch reset timing, PWR GPIO3 released/pressed levels, audio MCLK/PA, microphone slot order, and PMU behavior.
