# ESP32-S3-Touch-AMOLED-1.75C-FW

面向 Waveshare ESP32-S3-Touch-AMOLED-1.75C 的独立工程：保留 ESP-Brookesia 桌面框架，基于官方 1.75 完整应用源码适配 C 板，并整合“圆翼闯关”。这不是 1.75C 旧工厂固件的逐字节复刻。

整机编译、游戏测试、存储测试和播放器主机测试已通过。已安装版本的启动日志确认14个应用加载、桌面就绪；小智唤醒及语音回复已由实机日志和用户反馈确认。当前源码还包含最新的 VideoPlayer 闪存读取任务栈修复，该修复已构建和复核，尚未烧录到设备验证。详细状态见[运行问题与修复记录](docs/RUNTIME_FIX_ZH.md)。

原C工厂8个应用的对应功能全部纳入：Settings、MusicPlayer、Calculator、SquareLine、Gravitysphere（重力球）、SpecAnalyzer、AIChats、DrawPanel。另保留Gallery、VideoPlayer、Recorder、Crosshair、Button Test，并加入Round Wing，共14个应用。界面和实现来自可获取的1.75源码，部分版本与原C工厂不同。

- `firmware/`：整机 ESP-IDF 工程、C 板驱动及应用。
- `components/`：配套 Brookesia 框架、SquareLine 示例、圆翼应用适配器。
- `src/`、`include/`：游戏逻辑和圆屏 LVGL 界面。
- `platform/native/`、`tests/`：Mac 预览和游戏测试。
- `docs/`：软硬件差异、源码来源、验证报告及安装边界。

## 使用入口

```sh
git clone git@github.com:censhine/ESP32-S3-Touch-AMOLED-1.75C-FW.git
cd ESP32-S3-Touch-AMOLED-1.75C-FW
```

首次构建请按[构建说明](docs/BUILD_ZH.md)安装 ESP-IDF 5.5.5 和媒体生成工具依赖。仓库包含整合源码、资源、依赖锁文件及生成脚本；ESP-IDF 和组件注册表依赖在本地安装／下载。

- [差异清单：硬件、原8应用和使用体验](docs/DIFFERENCES_ZH.md)
- [实机烧录、完整备份与首次启动记录](docs/DEVICE_INSTALL_ZH.md)
- [构建与媒体说明](docs/BUILD_ZH.md)：执行 `./tools/build-firmware.sh` 进行本地编译。
- [软硬件兼容性和分区决策](docs/COMPATIBILITY_ZH.md)
- [已完成验证和待测项目](docs/VALIDATION_ZH.md)
- [独立代码审查](docs/reviews/port-review.md)
- [源码来源与许可](docs/SOURCES.md)
- [运行问题与修复记录](docs/RUNTIME_FIX_ZH.md)：音乐播放状态、小智内存及视频读取任务栈。

编译后用 `python3 tools/package-firmware.py artifacts/new-candidate` 生成分地址镜像、清单及 SHA-256 校验文件。`artifacts/` 是本地输出目录，不随 Git 克隆；设备完整备份、NVS、录音、聊天记录和运行日志也不进入仓库。

本工程使用新32MB分区布局；首次迁移前要备份当前设备。普通固件更新不包含media.bin，防止覆盖录音和个人素材。媒体导入目前通过离线打包，尚无USB磁盘或网络上传界面。
