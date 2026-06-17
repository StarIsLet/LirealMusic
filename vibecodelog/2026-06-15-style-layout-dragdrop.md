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

# 2026-06-15 风格预设、歌词布局与拖拽导入

## 本次完成

- 增加 `.lirealstyle.json` 风格预设保存/加载。
- 风格预设覆盖质量预设、分辨率、FPS、视觉参数、效果开关、预览时间和歌词布局。
- 增加歌词布局模板：右侧歌词队列、中央大字歌词、底部卡拉 OK。
- 新增底部卡拉 OK 当前行进度裁剪高亮。
- 增加主窗口拖拽导入素材：图片、音频、歌词和输出 MP4。
- 更新 README 使用说明。

## 验证

- 已运行 `cmake --build build -j`，构建成功。
- 已检查相关文件诊断，无错误。
