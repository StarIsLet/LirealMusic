/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 * Lireal Music - C++ audio visual rendering engine.
 * Copyright (C) 2026 Lireal contributors
 *
 * This file is part of Lireal Music.
 * Lireal Music is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace lireal::render {

struct RenderConfig {
    std::filesystem::path backgroundImagePath;
    std::filesystem::path musicPath;
    std::filesystem::path lyricPath;
    std::filesystem::path outputVideoPath;

    int width = 1920;
    int height = 1080;
    int fps = 60;
    int bitrateKbps = 12000;
    int encoderCrf = 17;
    int renderThreads = 0;
    int renderBatchFrames = 0;
    std::string encoderBackend = "auto";
    std::string encoderPreset = "veryfast";
    std::string encoderDevice = "auto";
    std::string renderBackend = "webgpu";
    std::string lyricFontFamily = "loli";
    std::string songTitle = "出山DJ";
    std::string artistName = "花粥";
    std::string watermarkText = "Lireal Music";

    // 梦幻粉白风格参数：保留可调节空间，UI 会将控件值写入这里。
    double parallaxStrength = 26.0;
    double pulseStrength = 0.045;
    double spectrumRadius = 188.0;
    double spectrumBarHeight = 96.0;
    double glowStrength = 0.65;

    // 0: 右侧歌词队列；1: 中央大字；2: 底部卡拉 OK。
    int lyricLayoutMode = 0;

    bool enableBloom = true;
    bool enableFloatingLyrics = true;
    bool enableCircularCover = true;
    bool enableMangaFilter = true;
    bool enableImpactFlash = true;
    bool enableParticles = true;

    // 预览专用：极速预览会自动降采样、降帧率并跳过昂贵光效；导出仍使用完整质量。
    bool enableFastPreview = true;
    int previewMaxWidth = 960;
    int previewMaxHeight = 540;
    int previewFps = 30;
};

} // namespace lireal::render
