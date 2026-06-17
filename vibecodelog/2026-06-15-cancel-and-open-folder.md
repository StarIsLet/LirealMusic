<!--
SPDX-License-Identifier: AGPL-3.0-or-later
Lireal Music - C++ audio visual rendering engine.
Copyright (C) 2026 Lireal contributors
-->

# 2026-06-15 渲染取消与打开输出文件夹日志

## 本次做了什么

- 在 `VideoRenderer` 中新增可选取消回调：
  - `CancelCallback`
- 完整视频渲染循环会在每一帧开始前检查取消请求。
- 合并音轨前也会再次检查取消请求。
- 用户取消后会释放 `VideoWriter` 并清理临时 `.video_only.mp4` 文件。
- dashboard 新增按钮：
  - `取消当前渲染`
  - `打开输出文件夹`
- 使用 `std::atomic_bool` 保存取消请求，避免 UI 线程和渲染线程之间的数据竞争。
- 渲染开始后会禁用预览/渲染按钮并启用取消按钮。
- 渲染结束后会恢复按钮状态。
- `打开输出文件夹` 使用系统默认文件管理器打开输出目录。
- 更新 `README.md`，记录取消渲染和打开输出目录功能。

## 用户体验改进

- 长视频渲染不再只能等待完成，用户可以主动取消。
- 取消时会等待当前帧安全结束，避免写出损坏的临时文件。
- 渲染完成后可一键打开输出目录查看 MP4 和预览图。

## 验证结果

- 执行 `cmake --build build -j` 编译成功。
- VS Code 问题面板未发现错误。

## 下一步建议

1. 添加预览时间点选择，例如 5 秒、15 秒、副歌附近。
2. 添加渲染预设保存/加载功能。
3. 添加更多歌词布局模板。
