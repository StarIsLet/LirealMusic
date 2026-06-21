# 2026-06-21 Video music source support

- GUI 音乐来源从纯音频升级为音频/视频来源。
- 支持选择或拖入 `.mp4`、`.mkv`、`.webm`、`.mov`、`.avi`、`.flv`、`.wmv` 等带音轨视频。
- FFmpeg/Libav 分析路径继续使用 `av_find_best_stream(..., AVMEDIA_TYPE_AUDIO, ...)`，会自动抽取视频内第一条可用音频流用于频谱、stem、播放器预览和导出音轨处理。
- 拖拽视频现在优先填入音乐来源，不再误当作输出路径。
- 更新素材校验和 README 文案，明确无音频流视频会被拒绝。
