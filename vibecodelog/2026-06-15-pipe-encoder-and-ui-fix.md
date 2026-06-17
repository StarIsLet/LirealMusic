<!--
SPDX-License-Identifier: AGPL-3.0-or-later
Lireal Music - C++ audio visual rendering engine.
Copyright (C) 2026 Lireal contributors

This file is part of Lireal Music.
Lireal Music is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
-->

# 2026-06-15 管线编码与观感修复

## 本次完成

- 将完整视频输出从 `OpenCV VideoWriter` 改为 FFmpeg `rawvideo` 管线编码。
- 每帧仍在内存中合成，但不再逐帧图片落盘，也避免 OpenCV 视频封装路径的额外开销。
- FFmpeg 直接接收 BGR24 raw frame，使用 `libx264` 编码临时无声视频，再复用现有音轨 mux 流程。
- 收敛默认画面观感：
  - 降低漫画滤镜强度。
  - 降低扫描线强度。
  - 限制 RGB 棱镜色散位移。
  - 将歌词绘制移动到后期色散/景深之后，避免歌词长期重影。
  - 提高斜向冲击歌词触发阈值。
- 修复 GUI 路径编辑框体验：
  - 增加最小宽度。
  - 增加清空按钮。
  - 增加占位提示。
  - 主内容放入滚动区域，避免窗口较小时控件被挤压。

## 说明

窗口录制式实时渲染以后可以继续做，但当前先改成更稳定的 rawvideo 管线编码：不写中间帧文件、质量可控、遮挡无关、跨桌面环境更可靠。

## 验证

- 已运行 `cmake --build build -j`，构建成功。
