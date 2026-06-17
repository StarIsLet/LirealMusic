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

# 2026-06-15 编码速度与画质控制

## 本次完成

- `RenderConfig` 新增：
  - `encoderPreset`
  - `encoderCrf`
- FFmpeg rawvideo 管线现在根据配置生成 `libx264 -preset` 与 `-crf` 参数。
- Dashboard 新增：
  - 编码速度下拉框：`ultrafast / veryfast / fast / medium`
  - 编码 CRF 数值框：`14-28`
- 质量预设会同步调整编码策略：
  - 快速预览：`ultrafast + CRF 20`
  - 标准梦幻：`veryfast + CRF 17`
  - 高清 2K：`veryfast + CRF 16`
  - 高能发布：`fast + CRF 15`

## 目的

让用户能明确选择“速度优先”或“画质优先”，配合 rawvideo 管线减少等待时间，同时保留高画质发布选项。

## 验证

- 已运行 `cmake --build build -j`，构建成功。
