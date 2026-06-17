<!--
SPDX-License-Identifier: AGPL-3.0-or-later
Lireal Music - C++ audio visual rendering engine.
Copyright (C) 2026 Lireal contributors
-->

# 2026-06-17 bundled-loli-font-title-layout

- 自动加载项目根目录 `loli.ttf`、`萝莉体 第二版/loli.ttf`、`assets/fonts/loli.ttf`。
- 默认歌词字体偏好改为 `loli`，并继续兼容“萝莉体 / 汉仪萝莉体简 / Aa萝莉体 / Lolita”等名称。
- 右侧滑动歌词字号加大：激活歌词与非激活歌词都更醒目。
- 歌曲名和作者字号缩小，并移动到左侧圆形图片正上方居中显示。
- README 补充根目录字体加载说明。
- 构建验证：`cmake --build build -j` 通过。
