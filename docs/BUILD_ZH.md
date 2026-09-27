# 构建与媒体文件

本机已准备 ESP-IDF 5.5.5、Python 3.12、Xtensa GCC 14.2、CMake/Ninja。SDK位于 `~/esp/esp-idf-v5.5.5`，工具位于 `~/esp/tools-v5.5.5`。

在工程根目录执行：

```sh
./tools/build-firmware.sh
```

此命令只编译，不访问设备。主程序、引导程序、分区表、WakeNet模型、SPIFFS字体资源及初始媒体镜像都在 `firmware/build/`。不要把旧的 Round Wing API 测试工程当成这里的整机固件。

## Mac游戏预览和测试

已安装 SDL2。先构建整机以解析相同版本的LVGL，再执行：

```sh
./tools/prepare-native-deps.sh
cmake -S . -B build-native -DCMAKE_BUILD_TYPE=Release
cmake --build build-native -j 8
ctest --test-dir build-native --output-on-failure
./build-native/round-wing-preview
python3 firmware/components/storage_service/tests/run_host_tests.py
```

游戏主机测试使用和整机一致的LVGL 9.4.0。存储测试用主机桩检查服务的错误处理和并发逻辑，不能代替真实Flash/WL/FatFs测试。

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
