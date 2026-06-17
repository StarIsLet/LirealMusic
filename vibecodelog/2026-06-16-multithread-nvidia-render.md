<!--
SPDX-License-Identifier: AGPL-3.0-or-later
Lireal Music - development log.
-->

# 多线程合成与 NVIDIA 编码优化

- 增加 `RenderConfig::renderThreads`，0 表示自动选择 CPU 合成线程数。
- 正式渲染改为批量多线程合成帧，再顺序写入 FFmpeg rawvideo 管线，保持视频帧顺序正确。
- Dashboard 增加“合成线程”选项，并支持风格预设保存/加载。
- 4K 超清预设默认自动多线程合成，并优先选择 NVIDIA `h264_nvenc` 第 0 张显卡；不可用时仍由已有预检逻辑回退到 `libx264`。
- NVIDIA NVENC 命令启用 `p5`、AQ、lookahead、B 帧与 surfaces，提高硬件编码质量和吞吐。
