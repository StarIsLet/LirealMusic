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

struct SourceStemSamples {
    std::string name;
    std::vector<float> monoSamples;
};

struct SourceSeparationResult {
    int sampleRate = 48000;
    std::vector<SourceStemSamples> stems;
};

class OnnxSourceSeparator {
public:
    explicit OnnxSourceSeparator(std::filesystem::path modelPath = {});
    ~OnnxSourceSeparator();

    [[nodiscard]] bool isAvailable() const;
    [[nodiscard]] std::string backendName() const;

    SourceSeparationResult separate(const std::vector<float>& stereoSamples, int sampleRate) const;

private:
    std::filesystem::path modelPath_;
};

} // namespace lireal::audio