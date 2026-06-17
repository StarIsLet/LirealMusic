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

#include <filesystem>
#include <string>
#include <vector>

namespace lireal::audio {

struct AudioStemEnergy {
    std::string name;
    float energy = 0.0F;
    float presence = 0.0F;
    float pan = 0.0F;
    float depth = 0.0F;
    float height = 0.0F;
    float width = 0.0F;
};

struct AudioFrameEnergy {
    double timeSeconds = 0.0;
    float rms = 0.0F;
    float bass = 0.0F;
    float mid = 0.0F;
    float treble = 0.0F;
    float vocal = 0.0F;
    float percussion = 0.0F;
    float ambience = 0.0F;
    float stereoWidth = 0.0F;
    float transient = 0.0F;
    float spectralFlux = 0.0F;
    float spectralCentroid = 0.0F;
    float beatPulse = 0.0F;
    float dropIntensity = 0.0F;
    float colorMood = 0.0F;
    std::vector<float> spectrumBins;
    std::vector<AudioStemEnergy> stems;
};

struct AudioAnalysisResult {
    double durationSeconds = 0.0;
    int sampleRate = 48000;
    int channels = 2;
    std::vector<AudioFrameEnergy> frames;
};

class AudioAnalyzer {
public:
    AudioAnalyzer();
    ~AudioAnalyzer();

    // 使用 FFmpeg 解码音频，并用 C++ FFT/Goertzel 风格频带能量计算得到逐帧驱动数据。
    AudioAnalysisResult analyze(const std::filesystem::path& audioPath, int targetFps) const;
};

} // namespace lireal::audio
