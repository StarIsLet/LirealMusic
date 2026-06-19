# 2026-06-19 Performance and HiFi audio boost

- Reduced high-resolution export memory pressure by lowering the frame batch memory budget, especially for 4K exports.
- Added FFmpeg filter capability detection so the HiFi chain can use `firequalizer` when available and a compatible EQ fallback otherwise.
- Expanded final stereo 3D processing from five stems to seven layers: dry, wide field, rear depth, air, sparkle, bass center, and body glue.
- Tightened final audio limiting and raised SoXr precision to keep loudness controlled while preserving detail.
- Documented the new 7-layer HiFi stereo 3D export chain in README.
