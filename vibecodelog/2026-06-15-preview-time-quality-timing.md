<!--
SPDX-License-Identifier: AGPL-3.0-or-later
Lireal Music - C++ audio visual rendering engine.
Copyright (C) 2026 Lireal contributors
-->

# 2026-06-15 三项体验增强日志

## 本次一次性新增三个功能

### 1. 预览时间点选择

- 新增预览时间点下拉框：
  - 5 秒
  - 15 秒
  - 30 秒
  - 自定义秒数
- 新增自定义秒数输入框。
- 生成预览图时会把选择的时间点传给 `VideoRenderer::renderPreviewImage()`。

### 2. 渲染质量预设

- 新增质量预设下拉框：
  - 快速预览 · 720p 30FPS
  - 标准梦幻 · 1080p 60FPS
  - 高清 2K · 1440p 60FPS
  - 高能发布 · 1080p 90FPS
- 选择预设后会自动调整：
  - 分辨率
  - FPS
  - 视差强度
  - 呼吸强度
  - 频谱半径
  - 频谱高度
  - Glow 强度
  - Bloom / 漫画滤镜 / 粒子等开关

### 3. 预览与渲染耗时统计

- 新增 `QElapsedTimer` 记录任务耗时。
- 预览图完成后会在日志中显示预览耗时。
- 完整渲染完成后会在日志中显示总耗时。
- 渲染成功后弹窗显示输出位置和耗时。

## 修改文件

- `include/lireal/ui/dashboard_window.hpp`
- `src/lireal/ui/dashboard_window.cpp`
- `README.md`

## 验证结果

- 执行 `cmake --build build -j` 编译成功。
- VS Code 问题面板未发现错误。

## 下一步建议

1. 添加风格预设保存/加载功能。
2. 添加更多歌词布局模板。
3. 添加拖拽导入素材功能。
