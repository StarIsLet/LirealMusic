<!--
SPDX-License-Identifier: AGPL-3.0-or-later
Lireal Music - C++ audio visual rendering engine.
Copyright (C) 2026 Lireal contributors
-->

# 2026-06-17 file-picker audio-preview loli-font

- TUI 增加类 GUI 文件选择：优先使用 `kdialog`，其次 `zenity`，不可用时回退手动输入路径。
- Dashboard 实时预览窗口增加右侧音频处理状态面板，显示 RMS、低/中/高频、人声、鼓点、氛围、Beat、Drop、声场宽度与 stem 3D 参数。
- 录制预览窗口也增加同款右侧音频处理面板，随预览帧刷新。
- `PreviewFrameCallback` 扩展为携带 `AudioFrameEnergy`，方便 UI 展示每帧音频处理状态。
- 视频字体优先尝试“萝莉体 / 汉仪萝莉体简 / Aa萝莉体 / Lolita”等可爱字体；系统未安装时回退到现有中文字体。
- 当前系统 `fc-match '萝莉体'` 回退到 `NotoSansCJK-Regular.ttc`，如需真正萝莉体效果需要用户安装对应字体。
