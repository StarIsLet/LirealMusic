<!--
SPDX-License-Identifier: AGPL-3.0-or-later
Lireal Music - C++ audio visual rendering engine.
Copyright (C) 2026 Lireal contributors
-->

# 2026-06-15 Dashboard 参数面板实现日志

## 本次做了什么

- 在 Qt dashboard 中新增 `画面参数` 区域。
- 新增可调控件：
  - 分辨率：`1280×720`、`1920×1080`、`2560×1440`
  - 帧率：`24` 到 `120 FPS`
  - 视差强度
  - 呼吸强度
  - 频谱半径
  - 频谱高度
  - Glow 强度
  - Bloom 开关
  - 左侧圆形封面开关
- 将所有控件值接入 `RenderConfig`。
- 更新白粉色 QSS 主题：
  - `QComboBox`
  - `QSpinBox`
  - `QDoubleSpinBox`
  - `QCheckBox`
- 更新 `README.md`，说明软件内参数调节方式。

## 验证结果

- 执行 `cmake --build build -j` 编译成功。
- VS Code 问题面板未发现错误。

## 当前效果

用户现在不需要改配置文件，就可以在软件内直接调整视频生成风格和输出质量。

## 下一步建议

1. 增强参考图风格视觉：黑白漫画化滤镜、斜向大字、闪白冲击、粒子碎片。
2. 增加素材合法性检查，例如输出路径扩展名、LRC 是否为空、音频是否过短。
3. 加入轻量预览帧生成功能。
