# BSP: Waveshare ESP32-S3-Touch-AMOLED-1.75C

[![Component Registry](https://components.espressif.com/components/waveshare/esp32_s3_touch_amoled_1_75/badge.svg)](https://components.espressif.com/components/waveshare/esp32_s3_touch_amoled_1_75)

This local BSP adapts the Waveshare ESP32-S3-Touch-AMOLED-1.75 source for the
1.75C board. It retains the enhanced audio APIs used by Brookesia, maps display
reset to GPIO1, touch reset to GPIO2, and audio MCLK to GPIO16. PWR is an
active-high input on GPIO3; BOOT is an active-low input on GPIO0. The 1.75C
has no microSD socket or TCA9554 expander, so neither is accessed by this BSP.

|                            HW version                            | BSP Version |
|:----------------------------------------------------------------:| :---------: |
| [1.75C](https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.75C) | Local adaptation |
