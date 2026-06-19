# 2026-06-19 WebGPU backend and render performance split

- Added a `lireal::render::gpu` WebGPU backend status interface so the app reports whether shader composition is actually compiled or falling back to CPU/OpenCV.
- Added `assets/shaders/lireal_effects.wgsl` with WGSL entries for fullscreen effects, spectrum ring, particles, and bloom tint as the shaderization entry point.
- Raised fast preview default to 1280x720 / independent 30 FPS while keeping export FPS and resolution separate.
- Added QPainterPath-based text caching for repeated title, watermark, and lyric glyph paths to reduce per-frame CPU text layout overhead.
- Updated GUI/README wording to clarify that NVENC/VAAPI only accelerates H.264 encoding, not CPU/OpenCV visual composition.
