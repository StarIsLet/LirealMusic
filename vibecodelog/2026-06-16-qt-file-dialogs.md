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

# 2026-06-16 Qt 内置文件对话框

## 本次完成

- 将所有素材/输出/预设文件选择改为 `QFileDialog::DontUseNativeDialog`。
- 新增内部 helper：
  - `openFileDialog()`
  - `saveFileDialog()`
- 避免 KDE 原生文件对话框触发大量 KIO/Solid 文件监视与 MIME 服务警告。

## 背景

运行日志中出现：

- `kf.solid.backends.fstab: Failed to acquire watch file descriptor Too many open files`
- `kf.service.sycoca: Service type not found`
- `kf.kio.widgets.kdirmodel: No node found for item that was just removed`

这些来自 KDE 原生文件对话框和 KIO 文件模型，不是渲染核心错误。改用 Qt 内置文件对话框可以减少这类平台噪声。

## 验证

- 已运行 `cmake --build build -j`，构建成功。
