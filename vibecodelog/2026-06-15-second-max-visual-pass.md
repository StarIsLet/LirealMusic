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

# 2026-06-15 二次最高视觉强化

## 本次完成

- 新增音频驱动相机运动：
  - 节拍脉冲驱动高能抖动。
  - drop 强度驱动垂直冲击。
  - 立体声宽度驱动横向漂移和轻微 roll 旋转。
  - 低频与 drop 共同驱动画面缩放。
- 新增频谱彗星轨迹，根据频谱段、声场宽度和氛围能量围绕中心运动。
- 新增动态星芒，高频和人声高点会触发闪耀线条。
- 新增音频驱动景深，中心舞台保持清晰，边缘随氛围产生柔焦。
- 新增胶片扫描线，高能段增强赛博/音乐现场质感。
- 所有效果都接入现有 `AudioFrameEnergy` 高级参数，兼容 ONNX stem 和启发式多轨。

## 验证

- 已运行 `cmake --build build -j`，构建成功。
- 已更新 README 渲染风格说明。
