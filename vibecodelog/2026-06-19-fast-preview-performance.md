# 2026-06-19 Fast preview performance

- Added a dedicated fast preview path that clamps preview rendering to 960x540 / 30 FPS by default.
- Preview stream and preview image now use a lightweight composition mode while final export keeps full quality.
- Fast preview skips expensive aurora, stage, particles, comets, shockwaves, starbursts, bloom, impact flash, and dream-grade passes.
- Export preview callbacks now downscale UI frames before converting to QImage to reduce 4K GUI copy cost.
- Compose path now respects effect toggles for particles, bloom, and impact flash in high-quality export.
