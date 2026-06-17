# 2026-06-16 Cute Lyric Transition and Title Highlight

## Goal
- Make lyric switching visibly animated instead of only animating within the current line.
- Add highlight/glow to song title and artist.
- Prefer cuter Chinese font families when available.

## Changes
- Added previous/next lyric transition state around active-line progress.
- Lyrics now slide, scale, lift, fade, glow, and sweep during line changes.
- Added title/artist highlight pill, animated gradient sweep, pulse glow, and small sparkle strokes.
- Font preference now prioritizes LXGW WenKai, ZCOOL, Ma Shan Zheng, and YouYuan before standard CJK sans fonts.

## Validation
- `cmake --build build -j` passed.
- VS Code diagnostics show no errors for `video_renderer.cpp`.
