# 2026-06-19 Player-mode audio-first preview

- 将 GUI 预览改成播放器式连续播放：从指定时间点播放到歌曲结束，关闭窗口即可取消。
- 新增 `VideoRenderer::analyzePreviewAudio` 与 `renderPreviewStreamFromAnalysis`，把音频预处理和画面循环拆成两个阶段。
- 预览流程先完成音乐解码、重采样、stem/频谱分析，再用已分析音频数据驱动画面，避免播放器入口重复绑定解码和帧循环。
- 正式导出进度增加“先处理音频”阶段提示，明确先音频后视频的流水线。
- README 更新播放器式预览说明。
