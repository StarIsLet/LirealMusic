# 2026-06-16 Lyric Motion, Higher Concurrency, Spatial Stem Stage

## Goal
- Add visible lyric animation instead of static text.
- Increase high-concurrency render throughput without writing per-frame images.
- Improve stereo/source-stem spatial visualization for stronger 3D stage feeling.

## Changes
- Added animated lyric text with slide-in, floating, glow pulse, karaoke sweep, and line-progress highlight.
- Raised automatic render thread usage and manual cap; batch size is now memory-aware and can use larger concurrency.
- Enhanced stereo analysis using left/right correlation plus mid/side width.
- Expanded pan/depth/height/width mapping for heuristic and ONNX stems.
- Reworked surround stage projection with perspective depth, speaker anchors, floor ellipses, and stronger spatial rings.
- Connected the surround stage into the current shallow xianxia render template.

## Validation
- `cmake --build build -j` passed.
- VS Code diagnostics show no errors for renderer and analyzer files.
