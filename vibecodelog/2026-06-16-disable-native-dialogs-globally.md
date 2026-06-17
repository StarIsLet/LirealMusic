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

# 2026-06-16 全局禁用原生文件对话框

## 本次完成

- 在 `QApplication` 创建前设置：
  - `QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, true)`
- 目的为避免 KDE 平台插件在应用启动或文件选择时继续加载原生 KIO 文件对话框。

## 背景

单次 `QFileDialog::DontUseNativeDialog` 仍可能无法阻止平台插件初始化产生的 KIO 日志。全局属性必须在 `QApplication` 创建前设置，才能更彻底地禁用原生对话框路径。

## 验证

- 已运行 `cmake --build build -j`，构建成功。
