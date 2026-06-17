<!--
SPDX-License-Identifier: AGPL-3.0-or-later
Lireal Music - C++ audio visual rendering engine.
Copyright (C) 2026 Lireal contributors
-->

# 2026-06-15 依赖安装后构建验证日志

## 本次做了什么

- 用户已在终端中手动安装项目依赖。
- 重新执行 CMake 配置：
  - `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`
- 执行项目编译：
  - `cmake --build build -j`

## 验证结果

- CMake 配置成功。
- OpenCV 4.10.0 已找到。
- FFmpeg 开发库已找到：
  - `libavformat`
  - `libavcodec`
  - `libavutil`
  - `libswresample`
  - `libswscale`
- Qt6 Widgets/Concurrent 已找到并完成链接。
- `lireal_core` 静态库构建成功。
- `lireal` 可执行程序构建成功。

## 当前可运行文件

- `build/lireal`

## 下一步建议

1. 运行 `build/lireal` 打开 dashboard。
2. 选择背景图片、音乐、LRC 歌词与输出路径测试第一版视频生成。
3. 后续继续实现：
   - 输出 MP4 时复用原始音轨。
   - 中文歌词 FreeType/Qt 字体渲染。
   - 渲染参数面板。
   - 更接近参考图的动态文字排版与冲击特效。
