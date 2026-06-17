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

#include "lireal/render/render_config.hpp"

#include <string>
#include <vector>

namespace lireal::system {

struct EncoderDeviceInfo {
    std::string label;
    std::string device;
    std::string backend;
};

struct HardwareProfile {
    unsigned int cpuThreads = 1;
    unsigned long long memoryBytes = 0;
    bool hasNvidia = false;
    bool hasVaapi = false;
    bool hasVulkan = false;
    bool hasWebGpuRuntime = false;
    std::string gpuName = "CPU";
    int recommendedRenderThreads = 1;
    int recommendedBatchFrames = 1;
    std::string recommendedEncoderBackend = "libx264";
    std::string recommendedEncoderDevice = "auto";
    std::string recommendedRenderBackend = "webgpu";
    std::string summary;
};

std::vector<EncoderDeviceInfo> detectEncoderDevices();
HardwareProfile detectHardwareProfile(int width = 1920, int height = 1080, int fps = 60);
void applyHardwareProfile(render::RenderConfig& config, const HardwareProfile& profile);

} // namespace lireal::system
