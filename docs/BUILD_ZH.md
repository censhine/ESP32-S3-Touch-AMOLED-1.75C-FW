# 构建与媒体文件

已验证环境为 ESP-IDF 5.5.5、Python 3.12、Xtensa GCC 14.2、CMake/Ninja。以下是新 Mac 的安装步骤；已经安装 SDK 时，直接激活同一版本环境即可。

```sh
brew install python@3.12 cmake ninja sdl2
export PATH="/opt/homebrew/opt/python@3.12/libexec/bin:$PATH"
export IDF_TOOLS_PATH="$HOME/esp/tools-v5.5.5"
mkdir -p "$HOME/esp"
git clone --recursive --branch v5.5.5 https://github.com/espressif/esp-idf.git "$HOME/esp/esp-idf-v5.5.5"
"$HOME/esp/esp-idf-v5.5.5/install.sh" esp32s3
source "$HOME/esp/esp-idf-v5.5.5/export.sh"
```

回到本工程根目录，安装初始媒体生成所需的 Pillow；此命令使用上一步激活的 SDK Python 环境。

```sh
python -m pip install -r firmware/tools/requirements.txt
```

在工程根目录执行：

```sh
./tools/build-firmware.sh
```

此命令只编译，不访问设备。主程序、引导程序、分区表、WakeNet模型、SPIFFS字体资源及初始媒体镜像都在 `firmware/build/`。不要把旧的 Round Wing API 测试工程当成这里的整机固件。

首次克隆或移动工程目录后，CMake 会自动更新 `firmware/dependencies.lock` 中 5 个随仓库提供的本地组件路径，保留注册表依赖的版本和校验值。直接执行 `idf.py -C firmware build` 也会运行这一步；无需删除锁文件或手动改路径。

## Mac游戏预览和测试

已安装 SDL2。先构建整机以解析相同版本的LVGL，再执行：

```sh
./tools/prepare-native-deps.sh
cmake -S . -B build-native -DCMAKE_BUILD_TYPE=Release
cmake --build build-native -j 8
ctest --test-dir build-native --output-on-failure
./build-native/round-wing-preview
python3 firmware/components/storage_service/tests/run_host_tests.py
python3 firmware/components/MusicPlayer/tests/test_music_player_support.py
python3 firmware/components/avi_player_safe/avi_player/tests/host/run_tests.py firmware/build/initial_media/video/color-test.avi
python firmware/tools/tests/test_rebase_local_lock.py
```

游戏主机测试使用和整机一致的LVGL 9.4.0。存储测试用主机桩检查服务的错误处理和并发逻辑，不能代替真实Flash/WL/FatFs测试。

`ctest` 也包含 Wi-Fi 密码大键盘的真实 LVGL 指针测试，覆盖分页、字符输入、密码校验、圆屏边界和销毁。测试生成的界面预览位于 `build-native/artifacts/wifi-keyboard-*.ppm`。

## 已迁移设备的应用更新

设备已经使用本项目的分区表和资源时，只更新应用可执行：

```sh
idf.py -C firmware -p /dev/cu.usbmodem1101 app-flash
```

串口名称以实际连接为准。`app-flash` 仅写应用；首次从工厂固件迁移需要完整备份、分区与资源安装，参见[安装记录](DEVICE_INSTALL_ZH.md)。不要把普通 `flash` 当作只更新应用，它还会写入分区表、模型和 SPIFFS 资源。

## 内部媒体

| 文件夹 | 支持内容 |
|---|---|
| music | MP3、WAV |
| pictures | baseline JPG/JPEG，单张不超过4MiB；photos也兼容 |
| video | MJPEG AVI，最大466×466，显示最高10fps，可选16位PCM音轨 |
| recordings | 24kHz、16位双声道录音WAV |
| history | 用户开启的小智聊天记录JSONL |
| diagnostics | 用户导出的诊断TXT |

首次镜像包含测试音频、两张图片和短视频。录音等目录运行时按需创建。录音时长受剩余容量限制，内部Flash不适合无休止循环录音或频繁测速。

要打包自选媒体，准备这些文件夹，在激活SDK的终端执行：

```sh
python firmware/tools/pack_media.py /absolute/path/to/my-media /absolute/path/to/custom-media.bin
```

工具只生成16MiB FAT/WL镜像，不烧录。输出必须位于输入文件夹外且不能覆盖已有文件。FAT卷序列号由SDK随机生成，同一素材两次打包的镜像哈希可能不同。

media.bin不列入普通固件flash清单，避免以后升级覆盖录音和个人素材。首次安装需要单独初始化media分区；替换整个媒体镜像会替换该分区中已有录音、记录和素材。尚未实现USB磁盘或网络文件上传界面。
