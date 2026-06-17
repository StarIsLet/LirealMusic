<!--
SPDX-License-Identifier: AGPL-3.0-or-later
Lireal Music - C++ audio visual rendering engine.
Copyright (C) 2026 Lireal contributors
-->

# 2026-06-17 tui-snap-dialog-encoder-preset-fix

- 修复 TUI 在 Snap/Portal 环境中调用 `kdialog`/`zenity` 时可能出现的文件描述符/Portal 报错：默认改为手动粘贴路径。
- 如需强制使用桌面文件选择器，可设置环境变量 `LIREAL_TUI_FILE_DIALOGS=1`。
- 修复 NVENC 自动回退到 `libx264` 时错误沿用 `p5` 预设导致 `x264 [error]: invalid preset 'p5'`。
- 为 NVENC 与 libx264 分别增加安全预设映射。
- TUI 默认编码预设改为 `veryfast`，由渲染器按实际编码器自动转换。
- 构建验证：`cmake --build build -j` 通过。
