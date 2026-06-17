<!--
SPDX-License-Identifier: AGPL-3.0-or-later
Lireal Music - development log.
-->

# Dashboard 与仙侠浅色模板重构

- 将视频默认画面重构为清冷浅色仙侠模板：左侧歌名/作者、下方圆盘封面、白色放射频谱、右侧滚动歌词、右上角水印和底部品红进度线。
- 增加基于背景均色的浅色调估计，用于标题辉光与整体冷色洗色。
- 降低默认怪异强特效干扰，改用轻柔视差、雪点粒子和透明冷色遮罩。
- Dashboard 增加歌曲名、作者、水印输入，并切换为浅蓝白卡片风格。
- 修正高能发布预设文案为 1080p 120FPS。
- 构建验证：`cmake --build build -j` 通过。
