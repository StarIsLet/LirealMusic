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

# 2026-06-15 多人多轨与 2.5D 环绕算法

## 本次完成

- 音频分析改为保留 48kHz 立体声，用 mono 通道做 DFT 主分析，用左右通道估算声像和空间宽度。
- 新增 `AudioStemEnergy`，每帧输出多轨能量、存在感、左右声像、深度、高度和宽度。
- 自动启发式分离：Lead Vocal、Harmony / Crowd、Bass / Kick、Percussion、Air / Reverb。
- 新增人声、打击、氛围、瞬态、立体声宽度等特征。
- 对新增特征执行全曲归一化和攻击/释放平滑。
- 渲染器新增 2.5D 环绕舞台，将多轨映射到左右、前后、上下空间。
- 环绕舞台包含椭圆空间轨道、声部光点、连线、宽度椭圆和柔光模糊。

## 说明

当前实现是无需外部 AI 模型的实时启发式分离算法，适合快速驱动视觉。它不是专业离线源分离模型，但已经能把人声、和声/群众感、低频、打击和空气混响分成独立视觉层。

## 验证

- 已运行 `cmake --build build -j`，构建成功。
- 已检查相关文件诊断，无错误。
