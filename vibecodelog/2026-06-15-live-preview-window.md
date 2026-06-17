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

# 2026-06-15 实时预览窗口

## 本次完成

- 新增 `VideoRenderer::renderPreviewStream()`。
- 预览流复用最终渲染的 `composeFrame()`，保证窗口看到的就是编码器使用的同一套画面逻辑。
- GUI 预览按钮从“生成预览图”改为“打开实时预览窗口”。
- 实时预览窗口不写入中间帧文件，只通过 `QImage` 在内存里传递画面。
- 关闭预览窗口会设置取消标记，后台预览循环安全停止。
- 大分辨率预览会缩到 1280 宽，避免预览阶段拖慢 GUI。

## 技术说明

这个方案不是系统录屏，也不依赖桌面合成器。窗口只是显示实时合成出来的帧，正式导出仍走 FFmpeg rawvideo 管线编码，因此不会被遮挡、窗口最小化等桌面状态影响。

## 验证

- 已运行 `cmake --build build -j`，构建成功。
