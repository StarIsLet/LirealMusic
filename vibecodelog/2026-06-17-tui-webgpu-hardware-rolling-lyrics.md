<!--
SPDX-License-Identifier: AGPL-3.0-or-later
Lireal Music - C++ audio visual rendering engine.
Copyright (C) 2026 Lireal contributors
-->

# 2026-06-17 TUI / WebGPU / hardware / rolling lyrics

- 将右侧歌词队列改为连续滚动插值，歌词切换时平滑上卷。
- 增强音频视觉驱动：更强的 beatPulse/dropIntensity、声场宽度和 stem 空间分布。
- 新增 `lireal_tui` 终端 UI：可爱 ANSI 面板、交互素材输入、画质/FPS选择、爱心进度条。
- 新增硬件自动检测模块：CPU 线程、内存、NVIDIA、VAAPI、Vulkan/WebGPU 运行条件。
- 根据硬件自动推荐渲染线程、批量帧数、编码器和渲染后端。
- 预留 WebGPU 后端字段与检测提示；当前稳定路径保持 CPU/OpenMP 合成并自动回退。
- 更新 README 说明 TUI、连续滚动歌词、自动硬件配置与 WebGPU 预留状态。
