# 1.75C 圆翼整合固件

面向 Waveshare ESP32-S3-Touch-AMOLED-1.75C 的独立工程：保留 ESP-Brookesia 桌面框架，基于官方 1.75 完整应用源码适配 C 板，并整合“圆翼闯关”。这不是 1.75C 旧工厂固件的逐字节复刻。

整机编译、游戏测试、存储测试和独立源码审查已通过。当前版本为0.1.0候选版，尚未烧录或完成真机验证。

原C工厂8个应用的对应功能全部纳入：Settings、MusicPlayer、Calculator、SquareLine、Gravitysphere（重力球）、SpecAnalyzer、AIChats、DrawPanel。另保留Gallery、VideoPlayer、Recorder、Crosshair、Button Test，并加入Round Wing，共14个应用。界面和实现来自可获取的1.75源码，部分版本与原C工厂不同。

- `firmware/`：整机 ESP-IDF 工程、C 板驱动及应用。
- `components/`：配套 Brookesia 框架、SquareLine 示例、圆翼应用适配器。
- `src/`、`include/`：游戏逻辑和圆屏 LVGL 界面。
- `platform/native/`、`tests/`：Mac 预览和游戏测试。
- `docs/`：软硬件差异、源码来源、验证报告及安装边界。

## 使用入口

- [构建与媒体说明](docs/BUILD_ZH.md)：执行 `./tools/build-firmware.sh` 进行本地编译。
- [软硬件兼容性和分区决策](docs/COMPATIBILITY_ZH.md)
- [已完成验证和待测项目](docs/VALIDATION_ZH.md)
- [独立代码审查](docs/reviews/port-review.md)
- [源码来源与许可](docs/SOURCES.md)
- [候选固件清单](artifacts/firmware-0.1.0-candidate/manifest.json)：7个分地址镜像及SHA-256，保留NVS区域。

候选文件在 `artifacts/firmware-0.1.0-candidate/`；重新编译后用 `python3 tools/package-firmware.py artifacts/new-candidate` 生成新包。这些文件在本地交付，Git不跟踪构建产物。

本工程使用新32MB分区布局；首次迁移前要备份当前设备。普通固件更新不包含media.bin，防止覆盖录音和个人素材。媒体导入目前通过离线打包，尚无USB磁盘或网络上传界面。
