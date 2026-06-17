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

# 2026-06-15 ONNX 神经网络源分离

## 本次完成

- 新增 `OnnxSourceSeparator`，用于接入 ONNX Runtime 神经网络源分离。
- 支持 Demucs/MDX 风格 4-stem 模型：Vocals、Drums、Bass、Other。
- 自动合成 `Accompaniment = Drums + Bass + Other` 作为伴奏视觉层。
- 新增可选 CMake 选项 `LIREAL_ENABLE_ONNXRUNTIME`。
- 如果系统没有 ONNX Runtime 或没有模型文件，自动回退到启发式多轨分析，不影响构建。
- 支持通过 `LIREAL_ONNX_SEPARATOR_MODEL` 指定模型路径。
- 默认模型路径为 `models/source_separation.onnx`。
- 音频分析流程已优先使用 ONNX stem 能量驱动 2.5D 环绕舞台。

## 模型接口约定

- 输入：`[1, 2, samples]`
- 输出：`[1, 4, 2, samples]`
- stem 顺序：`Vocals / Drums / Bass / Other`

## 验证

- 已运行 `cmake -S . -B build && cmake --build build -j`，构建成功。
- 已检查相关源码与 CMake 诊断，无错误。
