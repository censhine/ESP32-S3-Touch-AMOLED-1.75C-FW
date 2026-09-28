# 字体与图形来源

小鸟、光门、按钮均由本项目的 LVGL 几何对象绘制，无外部游戏贴图。

中文字体为 **Noto Sans SC Medium (500)**，来源是 Google Fonts 的 Noto Sans SC v40 子集接口，遵循本目录的 `OFL-NotoSansSC.txt`。`font-subset.css` 保存了实际来源 URL；`font-characters.txt` 记录所需字符。子集 TTF 不参与运行，构建直接使用已生成的两个 C 文件。

生成工具为 `lv_font_conv@1.5.3`，字体尺寸 22 / 30 px，4 bpp，未压缩。需重新生成时，下载 CSS 中的 TTF 到 `NotoSansSC-subset.ttf`，在项目根目录执行：

```sh
npx --yes lv_font_conv@1.5.3 --font assets/NotoSansSC-subset.ttf \
  --size 22 --bpp 4 --format lvgl --symbols "$(cat assets/font-characters.txt)" \
  --range 0x20-0x7e --no-compress -o assets/roundwing_font_22.c
npx --yes lv_font_conv@1.5.3 --font assets/NotoSansSC-subset.ttf \
  --size 30 --bpp 4 --format lvgl --symbols "$(cat assets/font-characters.txt)" \
  --range 0x20-0x7e --no-compress -o assets/roundwing_font_30.c
```

LVGL 9.4.0 来自 Espressif Component Registry，使用 MIT 许可证，许可证随依赖源文件保存在 `vendor/lvgl/LICENCE.txt`。仓库内已复制的音乐示例另保留 `firmware/components/MusicPlayer/gui_music/LICENCE.txt`。SDL2 为 zlib 许可证，由 Homebrew 安装。
