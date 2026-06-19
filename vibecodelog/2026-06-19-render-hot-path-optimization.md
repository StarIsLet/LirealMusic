# 2026-06-19 Render hot path optimization

- Precomputed static frame wash layers and the background accent color once per render/preview session instead of rebuilding them every frame.
- Added a tiny thread-local text path hot cache in front of the shared QPainterPath cache to reduce mutex contention during OpenMP frame synthesis.
- Reduced export preview callback frequency to avoid excessive 4K-to-preview conversion work while encoding.
- Used `QImage::Format_BGR888` for preview callbacks to avoid an extra BGR-to-RGB `cv::cvtColor` copy.
