<!--
SPDX-License-Identifier: AGPL-3.0-or-later
Lireal Music - C++ audio visual rendering engine.
Copyright (C) 2026 Lireal contributors
-->

# 2026-06-16 圆形封面、歌词排版、帧率与显卡选择修复

## 背景

用户反馈左侧圆形图片和歌词视觉异常，并要求支持 60/120 帧切换以及指定核显或 NVIDIA 显卡导出。

## 改动

- 修复圆形封面使用普通矩形 alpha 混合导致的黑色方块边。
- 新增圆形遮罩混合 `alphaBlendCircle()`，封面边缘改为柔边圆形叠加。
- 默认歌词队列改为右侧安全区三行排版：上一句、当前句、下一句。
- 歌词支持自动缩小和省略超长文本，避免冲出画面。
- 降低斜向冲击歌词的触发频率、字号和透明度。
- 新增帧率模式：60FPS、120FPS、手动 FPS。
- 高能发布预设从 90FPS 调整为 120FPS。
- 新增编码显卡选择：自动、VAAPI renderD128/renderD129、NVIDIA GPU 0/1。
- FFmpeg NVENC 使用 `-gpu` 指定 NVIDIA 显卡。
- FFmpeg VAAPI 使用 `-vaapi_device` 指定核显渲染节点。
- 风格预设保存/加载新增 `fpsMode` 与 `encoderDevice`。
- README 更新对应说明。

## 验证

- `cmake --build build -j` 构建通过。
- VS Code Problems 检查相关源码与头文件无错误。
