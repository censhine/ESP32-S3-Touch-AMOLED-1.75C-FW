# 1.75 → 1.75C 兼容性与整合决策

本工程以官方 1.75 完整 Brookesia 应用源码为基础，按 C 板文档、原理图和配套 BSP 适配；保留桌面框架与全部 13 个源应用，追加圆翼闯关。目标是功能整合，不能视为恢复 C 板原 8 应用工厂固件。

## 关键软硬件差异

| 项目 | 1.75 源设计 | 1.75C / 本工程处理 |
|---|---|---|
| 芯片 / PSRAM | ESP32-S3R8 / 8MB | 相同；保留 octal 80MHz、240MHz CPU |
| Flash | 16MB | 32MB，DIO模式 |
| 屏幕 | CO5300、466×466 QSPI圆屏 | 相同；保留偶数传输对齐、小DMA缓冲和内部RAM的LVGL任务栈 |
| LCD复位 | GPIO39 | GPIO1 |
| 触摸复位 | GPIO40 | GPIO2；CST9217中断仍11 |
| I²S MCLK | GPIO42 | GPIO16；BCLK9/LRCK45/DOUT8/DIN10相同 |
| 功放 | GPIO46，NS4150B | 保留3.3V供电模型及共享音频总线所有权 |
| PWR键 | TCA9554 EXIO4 | GPIO3按下为高；BOOT仍GPIO0按下为低 |
| 扩展器 | TCA9554 | 删除访问与依赖；C板没有此器件 |
| SD卡 | SDMMC用GPIO1/2/3 | 删除SD驱动；这些引脚已用于复位与PWR |
| 媒体 | SD卡文件夹 | 16MiB内部FAT + wear levelling，挂载/media |
| 传感器 | QMI8658 | 保留I²C 0x6B；实际摆动方向待真机核验 |
| 电源 | AXP2101 | 保留已核对的充电参数，未批量覆盖电源轨设置 |
| 串口 | 自定义UART，2Mbps | USB Serial/JTAG，适合Type-C调试 |
| 小智 | 桌面内整合esp_xiaozhi | 保留整合架构、模型分区和中文字体；中文界面 |

原C工厂中的小智2.1.0是独立固件；本工程采用1.75源码的桌面内小智。不能用旧工厂的OTA槽或升级包直接更新本工程。服务端`/ota/`接口用于激活和连接信息，并不意味着本工程提供整机OTA写入器。

## 应用保留清单

SquareLine、Calculator、DrawPanel、SpecAnalyzer、MusicPlayer、Gallery、VideoPlayer、Recorder、Settings、AIChats、Gravitysphere、Crosshair、Button Test，加上Round Wing（圆翼闯关），共14个。保留框架插件/应用生命周期；游戏支持4关、暂停/倒计时恢复、失败重试和独立NVS进度。

音乐/视频/录音/语音继续共用互斥音频所有权；没有把缺失的SD功能简单禁用。音乐MP3/WAV、相册baseline JPEG、视频MJPEG AVI改为/media。MP4/H.264解码不是原应用能力。内置小型合成图片、声音、视频样本供真机检查。

## 新分区与数据边界

| 分区 | 起点 | 大小 | 用途 |
|---|---|---|---|
| nvsfactory | 0x9000 | 200KiB | 保留原地址 |
| nvs | 0x3b000 | 840KiB | 保留原地址；启动失败不自动擦除 |
| otadata | 0x10d000 | 8KiB | 引导数据，整机应用为factory分区 |
| phy_init | 0x10f000 | 4KiB | PHY数据 |
| model | 0x110000 | 960KiB | WakeNet模型 |
| factory | 0x200000 | 8MiB | 完整桌面与14应用 |
| storage | 0xa00000 | 6MiB | SPIFFS资源与小智字库 |
| media | 0x1000000 | 16MiB | 可写内部媒体 |

新表与旧C工厂app/OTA/assets/storage布局不兼容。旧NVS地址相同也不保证所有旧键能被新应用解释，Wi-Fi/小智可能需重新配网或激活。任何首次安装前应读取当前32MiB全闪存并校验备份，已有恢复前备份不含最近的小智配网。

两种文件系统都禁止挂载失败自动格式化。media.bin只用于首次初始化或明确替换全部媒体，不在普通固件flash清单内；普通更新不会覆盖录音。内部存储容量不能等同SD卡：24kHz、16位双声道录音每秒96000字节，16MiB理论不足3分钟，实际更少。

## 验证边界

本地编译、静态审查与主机测试能验证源码接口和镜像结构，不能证明真实面板、触摸、麦克风槽序、扬声器、网络及电池运行正常。具体结果见VALIDATION_ZH.md；真机项目必须逐项检查后再标记通过。
