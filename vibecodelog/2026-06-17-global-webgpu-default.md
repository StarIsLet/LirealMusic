<!--
SPDX-License-Identifier: AGPL-3.0-or-later
Lireal Music - C++ audio visual rendering engine.
Copyright (C) 2026 Lireal contributors
-->

# 2026-06-17-global-webgpu-default

- 将 `RenderConfig::renderBackend` 默认值从 `auto` 改为 `webgpu`。
- 将硬件配置推荐的渲染后端统一改为 `webgpu`，不再按检测结果写入 `cpu`。
- `applyHardwareProfile` 始终应用全局 WebGPU 渲染模式。
- TUI 与 Dashboard 日志提示改为 WebGPU 全局模式，运行库不可用时走兼容合成与硬件编码回退。
- README 同步说明 WebGPU 已成为全局默认渲染配置。
