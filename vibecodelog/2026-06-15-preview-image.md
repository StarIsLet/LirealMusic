<!--
SPDX-License-Identifier: AGPL-3.0-or-later
Lireal Music - C++ audio visual rendering engine.
Copyright (C) 2026 Lireal contributors
-->

# 2026-06-15 预览图功能日志

## 本次做了什么

- 新增单张预览图生成能力。
- 在 `VideoRenderer` 中新增：
  - `renderPreviewImage()`
- 抽取完整视频渲染和预览图共用的单帧合成函数：
  - 背景视差
  - 漫画滤镜
  - 粒子效果
  - 闪白冲击
  - 圆形封面
  - 环形频谱
  - 中文歌词
  - 斜向冲击大字
  - Bloom 光效
- 在 dashboard 中新增按钮：
  - `生成当前风格预览图`
- 预览图会保存到输出视频同目录，文件名为：
  - `<输出文件名>_preview.png`
- 更新 `README.md`，说明可先生成预览图再完整渲染。

## 用户体验改进

- 用户无需等待完整视频渲染即可快速确认当前风格参数。
- 预览图复用正式渲染逻辑，因此效果与最终视频单帧保持一致。
- 预览生成失败时会弹出独立的错误提示。

## 验证结果

- 执行 `cmake --build build -j` 编译成功。
- VS Code 问题面板未发现错误。

## 下一步建议

1. 添加渲染取消按钮。
2. 添加渲染完成后打开输出文件夹按钮。
3. 增加预览时间点选择，例如 5 秒、15 秒、副歌附近。
