<!--
SPDX-License-Identifier: AGPL-3.0-or-later
Lireal Music - C++ audio visual rendering engine.
Copyright (C) 2026 Lireal contributors
-->

# 2026-06-15 中文歌词渲染实现日志

## 本次做了什么

- 将渲染器中的歌词绘制从 OpenCV `putText` 切换为 Qt `QPainter`。
- 使用 `QImage::Format_BGR888` 直接包裹 OpenCV `cv::Mat` 数据，避免额外大规模拷贝。
- 新增中文字体自动选择逻辑，优先使用：
  - `Noto Sans CJK SC`
  - `Noto Sans CJK JP`
  - `Source Han Sans SC`
  - `WenQuanYi Micro Hei`
  - `Microsoft YaHei`
  - `Sans Serif`
- 新增描边歌词绘制：
  - 激活歌词使用更大的字号和粉色高亮。
  - 非激活歌词使用半透明白色。
  - 使用深色描边提升复杂背景上的可读性。
- 更新 `CMakeLists.txt`：
  - 核心渲染库链接 `Qt6::Gui`。
- 更新 `README.md`：
  - 说明已经支持 Qt `QPainter` 中文歌词渲染。
  - 增加中文字体建议。

## 验证结果

- 执行 `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j` 成功。
- VS Code 问题面板未发现错误。

## 当前效果

中文 LRC 歌词现在应能在输出视频中正常显示，不再依赖 OpenCV 默认英文轮廓字体。

## 下一步建议

1. 增加 dashboard 渲染参数面板。
2. 增加参考图风格动态排版：斜向大字、歌词漂浮、节奏闪白、漫画黑白滤镜。
3. 增加运行前素材合法性检查与更清晰的错误提示。
