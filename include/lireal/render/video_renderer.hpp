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

#include "lireal/audio/audio_analyzer.hpp"
#include "lireal/lyrics/lrc_parser.hpp"
#include "lireal/render/render_config.hpp"

#include <functional>
#include <filesystem>
#include <string>

#include <QImage>

namespace lireal::render {

struct RenderProgress {
    int currentFrame = 0;
    int totalFrames = 0;
    double progress = 0.0;
    std::string message;
};

class VideoRenderer {
public:
    using ProgressCallback = std::function<void(const RenderProgress&)>;
    using CancelCallback = std::function<bool()>;
    using PreviewFrameCallback = std::function<void(const QImage&, int, int, const audio::AudioFrameEnergy&)>;

    VideoRenderer();
    ~VideoRenderer();

    // 执行完整渲染：读取背景图、分析音乐、解析 LRC、逐帧合成并输出视频。
    void render(const RenderConfig& config, const ProgressCallback& onProgress = {}, const CancelCallback& shouldCancel = {}, const PreviewFrameCallback& onPreviewFrame = {}) const;

    // 快速生成单张预览图，便于在完整渲染前确认当前风格参数。
    void renderPreviewImage(const RenderConfig& config, const std::filesystem::path& previewPath, double preferredTimeSeconds = 12.0) const;

    // 在内存中连续合成预览帧并回调给界面窗口，不写入任何中间帧文件。
    void renderPreviewStream(const RenderConfig& config, double startSeconds, double durationSeconds, const PreviewFrameCallback& onFrame, const CancelCallback& shouldCancel = {}) const;
};

} // namespace lireal::render
