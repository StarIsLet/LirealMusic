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

# 2026-06-16 编码后端选择

## 本次完成

- `RenderConfig` 新增 `encoderBackend`。
- FFmpeg rawvideo 管线支持三种后端：
  - `libx264`：默认软件编码，兼容性最好。
  - `h264_nvenc`：NVIDIA 硬件编码。
  - `h264_vaapi`：Linux VAAPI 硬件编码。
- Dashboard 新增“编码后端”下拉框。
- 风格预设保存/加载同步包含：
  - `encoderBackend`
  - `encoderPreset`
  - `encoderCrf`

## 注意

硬件编码需要用户系统的 FFmpeg 已编译对应编码器，并且 GPU 驱动/设备节点可用。默认仍使用 `libx264`，避免无硬件环境下失败。

## 验证

- 已运行 `cmake --build build -j`，构建成功。
