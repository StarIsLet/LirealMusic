<!--
SPDX-License-Identifier: AGPL-3.0-or-later
Lireal Music - C++ audio visual rendering engine.
Copyright (C) 2026 Lireal contributors
-->

# 2026-06-15 初始 C++ 重构实现日志

## 本次做了什么

- 按要求开始使用 C++ 重构 `Lireal Music`。
- 创建标准 C++20 + CMake 项目结构。
- 新增 Qt6 dashboard UI，可在软件内选择：
  - 背景图片
  - 音乐文件
  - LRC 歌词文件
  - 输出 MP4 路径
- 新增白粉色梦幻可爱系 QSS 主题 `assets/themes/sakura.qss`。
- 新增音频分析模块：
  - 使用 FFmpeg 解码音频。
  - 重采样到 48kHz 单声道。
  - 计算 RMS、低频、中频、高频与 64 段频谱能量。
- 新增 LRC 解析模块：
  - 支持 `[mm:ss.xx]歌词` 格式。
  - 支持同一行多个时间戳。
- 新增视频渲染模块：
  - OpenCV 读取背景图片。
  - 生成横屏视频帧。
  - 绘制圆形封面区域。
  - 绘制环形频谱。
  - 根据音频低频做背景视差与呼吸缩放。
  - 绘制歌词队列和底部进度条。
- 新增 `README.md`，说明项目结构、依赖、构建方式与当前限制。
- 新增 `LICENSE`，声明 AGPL-3.0-or-later。

## 当前限制

- 已尝试执行 `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`，但当前系统提示 `cmake` 未安装，需要先安装依赖后再继续编译验证。
- 当前 `VideoWriter` 主要输出视频画面，音乐音轨复用到 MP4 的 mux 流程需要后续继续实现。
- OpenCV 默认字体不适合中文歌词渲染，后续建议接入 FreeType 或用 Qt `QPainter` 渲染文字层。
- 目前 UI 参数还比较基础，后续可以增加分辨率、帧率、频谱半径、粉色强度等可视化调节控件。

## 下一步建议

1. 修复/验证本机 CMake 配置问题。
2. 接入真正的 MP4 音频复用。
3. 加入中文字体渲染。
4. 加入渲染参数面板和实时预览。
