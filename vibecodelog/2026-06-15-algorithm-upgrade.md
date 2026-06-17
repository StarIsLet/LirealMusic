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

# 2026-06-15 算法增强

## 本次完成

- 将音频频谱分析从每频带多探针 Goertzel 升级为 Hann 窗 + OpenCV DFT。
- 增加全曲峰值自适应归一化，减少不同音乐响度差异造成的画面不稳定。
- 增加攻击/释放平滑，让低频、均方根、频谱柱响应更自然。
- 频谱环预计算三角函数表，减少逐帧重复计算。
- 缓存中文歌词字体族选择，减少每帧字体扫描开销。
- 优化粒子伪随机分布，提升飘动稳定性。
- 新增梦幻粉蓝渐变调色和胶片暗角算法，增强成片质感。

## 验证

- 已运行 `cmake --build build -j`，构建成功。
- 已检查 `audio_analyzer.cpp` 与 `video_renderer.cpp` 诊断，无错误。
