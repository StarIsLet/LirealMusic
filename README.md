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

# 莉瑞尔 Lireal Music

`Lireal Music` 是面向 Kubuntu/Linux 的 C++20 音乐视频生成软件。它允许用户在浅蓝白 Dashboard 或可爱终端 TUI 中选择背景图片、音乐文件与 LRC 歌词，并生成清冷仙侠风横屏 MV：左侧歌名/作者、下方圆盘封面与白色环形频谱，右侧连续滚动歌词，底部品红进度线，画面配色会根据背景自动浅色化。

## 当前实现状态

本版本已经完成第一版 C++ 原生工程骨架与可运行渲染链路：

- C++20 + CMake 标准项目结构
- Qt6 Widgets 浅色 Dashboard UI
- `lireal_tui` 可爱终端 UI：交互式填写素材路径、标题作者、4K/2K/1080p、FPS，并显示爱心进度条；支持调用 `kdialog` / `zenity` 打开系统文件选择器
- 软件内选择：背景图片、音乐文件、歌词文件、输出 MP4，并可填写歌曲名、作者和右上角水印
- 软件内调节：720p/1080p/2K/4K 分辨率、60/120FPS 帧率模式、手动帧率、视差、呼吸、频谱、Glow、Bloom、圆形封面、漫画滤镜、闪白冲击、碎片粒子
- 软件内可设置 CPU 合成线程数，默认自动使用多核心批量合成帧，并可与 NVIDIA NVENC/VAAPI 硬件编码并行工作
- TUI 会自动检测 CPU 线程、内存、NVIDIA/VAAPI/Vulkan/WebGPU 运行条件，并全局启用 `webgpu` 渲染后端配置，同时推荐最大并发线程、批量帧数和编码器
- 软件内质量预设：快速预览、标准梦幻、高清 2K、高能发布、4K 超清发布
- GUI 实时预览与最终导出已分离：预览默认最高 1280×720 / 独立 30FPS，最终导出继续使用选择的 1080p/2K/4K 与导出 FPS
- 歌词/标题使用 QPainterPath 缓存，减少每帧重复构建中文/萝莉体文字路径的 CPU 开销
- NVIDIA NVENC/VAAPI 只加速 H.264 编码；如果 WebGPU/Dawn 后端未编译，粒子、频谱、Bloom、光晕等画面合成仍由 CPU/OpenCV/OpenMP 完成
- 软件内风格预设保存/加载，可复用画面参数、质量参数和歌词布局
- 软件内歌词布局模板：右侧歌词队列、中央大字歌词、底部卡拉 OK
- 支持将背景图片、音乐、歌词或输出 MP4 拖拽到窗口自动填充
- 软件内快速生成指定时间点的实时预览窗口；正式渲染时同步打开录制预览窗口，窗口只负责显示，最终 MP4 由同源原始合成帧直接编码，避免屏幕截图式录制变糊；预览窗口右侧会显示音频处理状态、频段能量、Beat/Drop、声场宽度和多轨 stem 参数
- 软件内取消当前渲染任务，并可快速打开输出文件夹
- 软件内统计预览和完整渲染耗时
- 渲染前自动检查素材路径、文件格式、空文件、输出目录和覆盖确认
- FFmpeg 解码音频并重采样
- C++ Hann 窗 + OpenCV DFT 频谱分析、全曲自适应归一化与 64 段平滑频谱驱动
- 高级音乐驱动：频谱通量、频谱质心、节拍脉冲、drop 强度、色彩情绪曲线
- 可选 ONNX Runtime 神经网络源分离，支持 Vocals、Drums、Bass、Other、Accompaniment stem 驱动
- WebGPU 后端已全局设为默认渲染模式，并新增 `lireal::render::gpu` 后端查询接口与 `assets/shaders/lireal_effects.wgsl` shader 入口；未链接 Dawn/wgpu-native 时会明确报告 `cpu-opencv-webgpu-fallback`，不会把 NVENC 编码误报成 GPU 合成
- 自动启发式分离 Lead Vocal、Harmony/Crowd、Bass/Kick、Percussion、Air/Reverb 多轨能量
- 基于声像、宽度、深度、高度的 2.5D 环绕舞台可视化算法
- OpenCV 内存合成视频画面，并通过 FFmpeg rawvideo 管线直接编码，避免逐帧图片落盘
- 正式渲染采用批量多线程帧合成，再按顺序写入 FFmpeg 管线，避免乱序并提高 2K/4K 吞吐
- 实时预览窗口复用同一套内存合成帧，可在渲染前播放短片段确认效果
- GUI 可调 FFmpeg `libx264` 编码速度与 CRF 画质，能在极速预览和高画质发布之间切换
- GUI 可自动选择 `libx264`、`h264_nvenc`、`h264_vaapi` 编码后端，并会枚举真实可用的 NVIDIA CUDA 显卡与 `/dev/dri/renderD*` VAAPI 设备用于硬件加速导出
- 渲染状态面板显示实时合成速度、剩余时间和帧进度，并对日志刷新做节流避免卡顿
- Linux/KDE 环境下应用全局禁用原生文件对话框，使用 Qt 内置对话框减少 KIO/Solid 文件监视警告
- FFmpeg 命令行将原始音乐音轨合并进最终 MP4
- 清冷仙侠浅色模板、背景自动取色、白色雪点粒子
- 圆盘封面区域与黑色唱片环
- 白色环形频谱光效
- 背景视差与节奏缩放
- LRC 歌词解析
- 右侧歌词队列使用连续滚动插值，切换歌词时不再“跳帧式”换行
- Qt `QPainter` 中文歌词渲染
- MP4 视频输出

> 注意：歌词字体会自动加载项目根目录的 `loli.ttf`、`萝莉体 第二版/loli.ttf` 或 `assets/fonts/loli.ttf`，并优先尝试 `loli`、`萝莉体`、`汉仪萝莉体简`、`Aa萝莉体`、`Lolita` 等可爱字体；如果系统没有安装，会回退到 `LXGW WenKai`、`ZCOOL KuaiLe`、`Noto Sans CJK SC` 等字体。请自行确保字体文件授权可用于当前用途。

## 推荐系统

- Kubuntu 22.04/24.04 或其它 Linux 发行版
- CMake 3.22+
- GCC 11+ / Clang 14+
- Qt6
- OpenCV 4
- FFmpeg 开发库
- 可选：ONNX Runtime C/C++ 开发库与 Demucs/MDX 风格 4-stem ONNX 模型

## 依赖安装示例

在 Kubuntu 上可安装：

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake pkg-config \
  qt6-base-dev \
  libopencv-dev \
  ffmpeg \
  libavformat-dev libavcodec-dev libavutil-dev libswresample-dev libswscale-dev
```

如果系统安装了 `onnxruntime` 的 pkg-config 开发包，CMake 会自动启用神经网络源分离。否则会自动回退到内置启发式多轨分析，不影响构建和渲染。

ONNX 模型路径支持两种方式：

1. 放到项目目录：`models/source_separation.onnx`
2. 或设置环境变量：`LIREAL_ONNX_SEPARATOR_MODEL=/path/to/model.onnx`

当前推理接口面向 Demucs/MDX 风格 4-stem 模型，输入形状为 `[1, 2, samples]`，输出形状为 `[1, 4, 2, samples]`，四个 stem 顺序为 `Vocals / Drums / Bass / Other`。程序会自动合成 `Accompaniment = Drums + Bass + Other` 用于视觉层。

## 构建

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### NixOS / Nix Flake

```bash
nix build .#lirealMusic
nix run .#lirealMusic
```

TUI 可运行：

```bash
nix run .#lirealTui
```

### GitHub Release

仓库已提供 `.github/workflows/release.yml`：

- 推送 `v*` 标签会自动构建并发布 Release。
- 也可在 GitHub Actions 手动运行 `可爱 Release 构建`。
- Release 产物包含 Kubuntu/Ubuntu 24.04、Arch Linux 与 NixOS x86_64 包。
- NixOS 使用 `flake.nix` 构建，适合 NixOS 用户直接复现环境。

如果系统提示 `cmake: command not found`，请先安装上文依赖中的 `cmake`。

## 运行

```bash
./build/lireal
```

打开软件后依次选择：

1. 背景图片：`.png` / `.jpg` / `.jpeg` / `.webp` / `.bmp`
2. 音乐文件：`.mp3` / `.wav` / `.flac` / `.aac` / `.ogg` / `.m4a`
3. 歌词文件：`.lrc`
4. 输出路径：`.mp4`

然后可以按需要调整画面参数：

- 质量预设：快速预览 / 标准梦幻 / 高清 2K / 高能发布
- 分辨率：`1280×720` / `1920×1080` / `2560×1440`
- 帧率模式：`60FPS` / `120FPS` / 手动 FPS
- 手动帧率：`24` 到 `120 FPS`
- 视差强度
- 呼吸强度
- 频谱半径
- 频谱高度
- Glow 强度
- 编码后端：自动 / `libx264` / `h264_nvenc` / `h264_vaapi`
- 编码显卡：自动 / VAAPI 核显 renderD128/renderD129 / NVIDIA 第 0 或第 1 张显卡
- Bloom 开关
- 圆形封面开关
- 黑白漫画化滤镜开关
- 节奏闪白冲击开关
- 碎片/雪点粒子开关
- 歌词布局：右侧歌词队列 / 中央大字歌词 / 底部卡拉 OK
- 预览时间点：`5 秒` / `15 秒` / `30 秒` / 自定义秒数

可以点击 **保存风格预设** 将当前画面参数保存为 `.lirealstyle.json`，之后通过 **加载风格预设** 恢复同一套风格。风格预设只保存画面与布局参数，不保存具体素材路径。

也可以直接把素材文件拖入主窗口：图片会自动填入背景，音频会自动填入音乐，`.lrc`/`.txt` 会自动填入歌词，`.mp4` 会自动填入输出路径。

可以先选择预览时间点，再点击 **生成当前风格预览图**，程序会在输出视频同目录生成一张 `_preview.png` 预览图，并在日志中显示预览耗时。

确认效果后，点击 **开始生成梦幻音乐视频**。完整渲染结束后会显示总耗时和输出位置。

渲染过程中可以点击 **取消当前渲染**，程序会在当前帧安全结束后停止，并清理临时视频文件。

渲染完成后可以点击 **打开输出文件夹**，快速查看生成的 MP4 与预览图。

渲染前程序会自动执行素材检查：

- 背景、音乐、歌词、输出路径不能为空
- 背景图片、音乐文件、歌词文件必须存在且可读
- 文件扩展名必须属于支持列表
- 歌词文件为空会阻止渲染
- 歌词缺少 LRC 时间轴标签时会询问是否继续
- 输出路径未写 `.mp4` 时会自动补全
- 输出目录不存在时可一键创建
- 输出文件已存在时会询问是否覆盖

## 项目结构

```text
LirealMusic/
├── CMakeLists.txt
├── README.md
├── LICENSE
├── assets/
│   └── themes/
│       └── sakura.qss
├── include/lireal/
│   ├── audio/audio_analyzer.hpp
│   ├── audio/onnx_source_separator.hpp
│   ├── lyrics/lrc_parser.hpp
│   ├── render/render_config.hpp
│   ├── render/video_renderer.hpp
│   └── ui/dashboard_window.hpp
├── src/
│   ├── main.cpp
│   └── lireal/
│       ├── audio/audio_analyzer.cpp
│       ├── audio/onnx_source_separator.cpp
│       ├── lyrics/lrc_parser.cpp
│       ├── render/video_renderer.cpp
│       └── ui/dashboard_window.cpp
├── Project_Guid/
└── vibecodelog/
```

## 渲染风格

当前渲染器围绕“音乐可视化 MV”风格实现：

- 背景图片全屏铺满并根据低频节奏进行轻微视差位移
- 音乐低频触发画面缩放呼吸
- 使用全曲峰值归一化与攻击/释放平滑，让频谱更稳定、节奏响应更敏捷
- 通过频谱通量与瞬态联合检测节拍峰值，让画面冲击跟随鼓点和 drop 爆发
- 从立体声音频中估算声像、空间宽度、瞬态、空气感和人声存在感
- 检测到 ONNX 模型时优先使用神经网络 stem 能量；没有模型时自动使用启发式多轨分析
- 将自动分离的多人/多轨映射到 2.5D 环绕声场，形成前后深度、左右声像和高度漂浮
- 在 2.5D 舞台中用空间光团表现 Vocals、Drums、Bass、Other、Accompaniment 的位置与能量
- 节拍峰值会生成多层扩散冲击波，drop 高潮会强化空间光环
- 相机运动会跟随节拍、drop 和立体声宽度产生缩放、漂移、轻微旋转和高能抖动
- 频谱彗星轨迹围绕画面中心运动，表现高频、空间宽度和氛围能量
- 人声与高频会触发动态星芒，让旋律高点更闪耀
- 左侧生成柔边圆形封面区域，避免方形黑边和过强重影
- 圆形封面外绘制预计算三角函数加速的 64 段环形频谱
- 右侧绘制三行安全区歌词队列，自动缩放/省略超长歌词，避免文字冲出画面
- 可切换为中央大字歌词或底部卡拉 OK 进度歌词
- 节奏强时绘制斜向冲击大字
- 可选黑白漫画化滤镜和边缘描线
- 可选闪白冲击与碎片/雪点粒子
- 内置梦幻粉蓝渐变调色与胶片式暗角增强
- 基于频谱质心和人声/空气感的神经色彩分级，自动在粉白、金橙和蓝紫之间流动
- 高能段适度叠加 RGB 棱镜色散，让鼓点和高潮出现视觉冲击，同时避免歌词长期重影
- 后期合成加入音频驱动景深与胶片扫描线，增强 MV 质感和层次
- 底部绘制粉色播放进度条
- 整体叠加粉白色发光 bloom

## 许可证

本项目使用 `AGPL-3.0-or-later`。每个源码、配置、文档文件均加入对应 SPDX/许可证声明。
