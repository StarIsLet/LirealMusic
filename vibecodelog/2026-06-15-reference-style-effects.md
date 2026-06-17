<!--
SPDX-License-Identifier: AGPL-3.0-or-later
Lireal Music - C++ audio visual rendering engine.
Copyright (C) 2026 Lireal contributors
-->

# 2026-06-15 参考图风格视觉增强日志

## 本次做了什么

- 新增参考图风格的视觉增强效果：
  - 黑白漫画化滤镜
  - 边缘描线
  - 节奏闪白冲击
  - 碎片/雪点粒子漂浮
  - 节奏强时出现斜向冲击大字
- 新增渲染配置项：
  - `enableMangaFilter`
  - `enableImpactFlash`
  - `enableParticles`
- 在 dashboard 参数面板中新增开关：
  - 漫画滤镜
  - 闪白冲击
  - 碎片粒子
- 将新增 UI 开关接入 `RenderConfig`。
- 更新 `README.md`，记录新增视觉效果和可调开关。

## 修复内容

- 编译时发现 `std::clamp` 参数类型混用 `double` 与 `float` 导致模板推导失败。
- 已统一为 `double` 后重新编译成功。

## 验证结果

- 执行 `cmake --build build -j` 编译成功。
- VS Code 问题面板未发现错误。

## 当前效果

输出视频现在更接近参考图表达的音乐可视化 MV 类型：黑白冲击背景、强节奏闪白、斜向大字歌词、圆形频谱、粉白光效和漂浮碎片。

## 下一步建议

1. 增加素材合法性检查与更友好的错误提示。
2. 添加预览帧功能，便于渲染前快速确认风格。
3. 加入更多歌词布局模板，例如右侧瀑布流、中央大字、环绕圆盘。
