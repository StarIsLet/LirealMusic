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

# 2026-06-16 渲染状态与 ETA

## 本次完成

- Dashboard 渲染状态区新增实时统计标签。
- 渲染过程中显示：
  - 当前状态
  - 百分比
  - 实时合成速度 FPS
  - 预计剩余时间 ETA
  - 当前帧 / 总帧数
- 日志输出改为约每 5% 进度记录一次，避免大量逐帧日志拖慢 UI。
- 状态标签约 350ms 刷新一次，兼顾实时性与界面流畅度。

## 修复

- 为 `RenderProgress` 添加前向声明，修复 UI 头文件编译可见性问题。

## 验证

- 已运行 `cmake --build build -j`，构建成功。
