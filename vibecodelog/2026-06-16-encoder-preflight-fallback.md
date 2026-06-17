<!--
SPDX-License-Identifier: AGPL-3.0-or-later
Lireal Music - C++ audio visual rendering engine.
Copyright (C) 2026 Lireal contributors
-->

# 2026-06-16 编码器预检与回退

## 背景

用户运行时出现 `FFmpeg 管线写入失败，视频编码中断`。终端曾显示 `h264_nvenc` 找不到 `libcuda.so.1`，说明系统 FFmpeg 编译了 NVENC，但当前机器没有可用 CUDA 运行库或驱动设备。

## 改动

- 新增硬件编码器真实预检：使用 FFmpeg 编码一个极小测试帧。
- 自动模式不再只根据 `ffmpeg -encoders` 判断硬件编码可用。
- NVENC 预检会检查 `libcuda.so.1` 并尝试 `h264_nvenc -gpu N`。
- VAAPI 预检会检查指定 `/dev/dri/renderD128/renderD129` 并尝试 `h264_vaapi`。
- 用户手选硬件后端但不可用时，自动回退到 `libx264`，避免管线写入失败。

## 验证

- `cmake --build build -j` 构建通过。
- `src/lireal/render/video_renderer.cpp` 无编辑器诊断错误。
