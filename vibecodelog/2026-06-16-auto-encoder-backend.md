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

# 2026-06-16 自动编码后端

## 本次完成

- `RenderConfig::encoderBackend` 默认改为 `auto`。
- GUI 编码后端下拉框新增“自动选择 · 推荐”。
- 渲染启动时自动探测 FFmpeg 编码器：
  1. 优先 `h264_nvenc`
  2. 其次 `h264_vaapi`，并检查 `/dev/dri/renderD128`
  3. 最后回退 `libx264`
- 质量预设默认保持自动后端，避免用户手动判断 GPU 环境。

## 目的

用户只需要选择“自动”，程序会尽量使用可用硬件编码；不可用时保持兼容的软件编码，不影响导出。

## 验证

- 已运行 `cmake --build build -j`，构建成功。
