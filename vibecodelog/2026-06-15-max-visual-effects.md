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

# 2026-06-15 最高效果增强

## 本次完成

- 在音频分析结果中新增高级驱动参数：
  - `spectralFlux`：频谱通量，用于检测新音符、鼓点和切分冲击。
  - `spectralCentroid`：频谱质心，用于估计明亮度和音色重心。
  - `beatPulse`：综合瞬态、频谱通量和低频的节拍脉冲。
  - `dropIntensity`：综合低频、打击和频谱通量的高潮/drop 强度。
  - `colorMood`：综合频谱质心、人声和空气感的色彩情绪曲线。
- 新增基于节拍峰值的多层扩散冲击波。
- 新增高能段 RGB 棱镜色散，让鼓点和 drop 更有冲击力。
- 新增神经色彩分级，根据音色和人声自动改变画面色彩气氛。
- 新增 2.5D stem 标签，直观显示 Vocals、Drums、Bass、Other、Accompaniment 或启发式多轨位置。
- 保留所有原有回退路径，ONNX 模型不存在时仍可使用启发式多轨和高级视觉驱动。

## 验证

- 已运行 `cmake --build build -j`，构建成功。
- 已消除新增高级音频缓存复制引发的 GCC 告警。
