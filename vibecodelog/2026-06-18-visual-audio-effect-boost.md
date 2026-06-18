# 2026-06-18 视听效果再增强

- 强化音频分析：加入低频谱通量、Bass/Percussion 上升沿检测，让 `beatPulse` 和 `dropIntensity` 对鼓点/Drop 更敏感。
- 强化画面效果：频谱环改为发光双层反应式音柱，并新增 Bass 光晕、极光音浪、节拍粒子、彗星、冲击波、星芒和梦幻调色调用链。
- 保持 FFmpeg rawvideo 管线，不回退到逐帧落盘，避免硬盘 IO 瓶颈。
