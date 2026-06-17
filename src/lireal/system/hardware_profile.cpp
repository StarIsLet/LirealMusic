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

#include "lireal/system/hardware_profile.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace lireal::system {
namespace {

bool commandSucceeds(const std::string& command) {
    return std::system(command.c_str()) == 0;
}

std::string trim(const std::string& value) {
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

std::string readCommandOutput(const std::string& command) {
    std::array<char, 512> buffer{};
    std::string output;
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe == nullptr) {
        return {};
    }
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }
    pclose(pipe);
    return output;
}

std::string readFirstLine(const std::filesystem::path& path) {
    std::ifstream stream(path);
    std::string line;
    std::getline(stream, line);
    return line;
}

unsigned long long detectMemoryBytes() {
    std::ifstream stream("/proc/meminfo");
    std::string key;
    unsigned long long valueKb = 0;
    std::string unit;
    while (stream >> key >> valueKb >> unit) {
        if (key == "MemTotal:") {
            return valueKb * 1024ULL;
        }
    }
    return 4ULL * 1024ULL * 1024ULL * 1024ULL;
}

std::string detectGpuName(bool hasNvidia, bool hasVaapi, bool hasVulkan) {
    if (hasNvidia) {
        const std::filesystem::path procGpu("/proc/driver/nvidia/gpus");
        if (std::filesystem::exists(procGpu)) {
            for (const auto& entry : std::filesystem::directory_iterator(procGpu)) {
                const std::string info = readFirstLine(entry.path() / "information");
                if (!info.empty()) {
                    return info;
                }
            }
        }
        return "NVIDIA GPU";
    }
    if (hasVaapi) {
        return "VAAPI GPU";
    }
    if (hasVulkan) {
        return "Vulkan/WebGPU-capable GPU";
    }
    return "CPU renderer";
}

} // namespace

std::vector<EncoderDeviceInfo> detectEncoderDevices() {
    std::vector<EncoderDeviceInfo> devices;

    const std::string nvidiaOutput = readCommandOutput("nvidia-smi --query-gpu=index,name --format=csv,noheader 2>/dev/null");
    std::istringstream nvidiaStream(nvidiaOutput);
    std::string line;
    while (std::getline(nvidiaStream, line)) {
        line = trim(line);
        if (line.empty()) {
            continue;
        }
        const std::size_t comma = line.find(',');
        const std::string index = trim(comma == std::string::npos ? line : line.substr(0, comma));
        const std::string name = trim(comma == std::string::npos ? std::string("NVIDIA GPU") : line.substr(comma + 1));
        if (!index.empty()) {
            devices.push_back({"NVIDIA · " + name + " · cuda:" + index, "cuda:" + index, "h264_nvenc"});
        }
    }

    const std::filesystem::path driPath("/dev/dri");
    if (std::filesystem::exists(driPath)) {
        std::vector<std::filesystem::path> renderNodes;
        for (const auto& entry : std::filesystem::directory_iterator(driPath)) {
            const std::string filename = entry.path().filename().string();
            if (filename.rfind("renderD", 0) == 0) {
                renderNodes.push_back(entry.path());
            }
        }
        std::sort(renderNodes.begin(), renderNodes.end());
        for (const auto& node : renderNodes) {
            const std::string name = node.filename().string();
            const std::filesystem::path sysDevice = std::filesystem::path("/sys/class/drm") / name / "device";
            const std::string vendor = trim(readFirstLine(sysDevice / "vendor"));
            std::string vendorName = "GPU";
            if (vendor == "0x8086") {
                vendorName = "Intel 核显";
            } else if (vendor == "0x1002") {
                vendorName = "AMD 显卡";
            } else if (vendor == "0x10de") {
                vendorName = "NVIDIA 显卡";
            }
            devices.push_back({"VAAPI · " + vendorName + " · " + name, "vaapi:" + node.string(), "h264_vaapi"});
        }
    }

    return devices;
}

HardwareProfile detectHardwareProfile(int width, int height, int fps) {
    HardwareProfile profile;
    profile.cpuThreads = std::max(1U, std::thread::hardware_concurrency());
    profile.memoryBytes = detectMemoryBytes();
    profile.hasNvidia = commandSucceeds("command -v nvidia-smi >/dev/null 2>&1") || std::filesystem::exists("/proc/driver/nvidia/version");
    profile.hasVaapi = std::filesystem::exists("/dev/dri/renderD128") || std::filesystem::exists("/dev/dri/renderD129");
    profile.hasVulkan = commandSucceeds("command -v vulkaninfo >/dev/null 2>&1") || std::filesystem::exists("/usr/lib/x86_64-linux-gnu/libvulkan.so.1");
    profile.hasWebGpuRuntime = profile.hasVulkan || commandSucceeds("ldconfig -p 2>/dev/null | grep -Eq 'libwgpu|libdawn'");
    profile.gpuName = detectGpuName(profile.hasNvidia, profile.hasVaapi, profile.hasVulkan);

    const unsigned long long bytesPerFrame = static_cast<unsigned long long>(std::max(1, width)) * static_cast<unsigned long long>(std::max(1, height)) * 3ULL;
    const unsigned long long renderBudget = std::max(256ULL * 1024ULL * 1024ULL, profile.memoryBytes / 8ULL);
    const int memoryBatchLimit = static_cast<int>(std::clamp<unsigned long long>(renderBudget / std::max(1ULL, bytesPerFrame), 1ULL, 256ULL));

    int threadTarget = static_cast<int>(profile.cpuThreads);
    if (profile.hasNvidia || profile.hasVaapi || profile.hasWebGpuRuntime) {
        threadTarget = static_cast<int>(std::ceil(static_cast<double>(profile.cpuThreads) * 1.15));
    }
    if (fps >= 60) {
        threadTarget = static_cast<int>(std::ceil(static_cast<double>(threadTarget) * 1.10));
    }
    profile.recommendedRenderThreads = std::clamp(threadTarget, 1, 96);
    profile.recommendedBatchFrames = std::clamp(std::min(profile.recommendedRenderThreads * 4, memoryBatchLimit), 1, 256);

    if (profile.hasNvidia) {
        profile.recommendedEncoderBackend = "h264_nvenc";
        profile.recommendedEncoderDevice = "cuda:0";
    } else if (profile.hasVaapi) {
        profile.recommendedEncoderBackend = "h264_vaapi";
        profile.recommendedEncoderDevice = std::filesystem::exists("/dev/dri/renderD128") ? "vaapi:/dev/dri/renderD128" : "vaapi:/dev/dri/renderD129";
    } else {
        profile.recommendedEncoderBackend = "libx264";
        profile.recommendedEncoderDevice = "auto";
    }
    profile.recommendedRenderBackend = "webgpu";

    std::ostringstream summary;
    summary << "CPU " << profile.cpuThreads << " threads, RAM " << (profile.memoryBytes / 1024ULL / 1024ULL / 1024ULL)
            << " GiB, GPU " << profile.gpuName << ", render=" << profile.recommendedRenderBackend
            << ", encoder=" << profile.recommendedEncoderBackend << ", threads=" << profile.recommendedRenderThreads
            << ", batch=" << profile.recommendedBatchFrames;
    profile.summary = summary.str();
    return profile;
}

void applyHardwareProfile(render::RenderConfig& config, const HardwareProfile& profile) {
    config.renderThreads = profile.recommendedRenderThreads;
    config.renderBatchFrames = profile.recommendedBatchFrames;
    config.encoderBackend = profile.recommendedEncoderBackend;
    config.encoderDevice = profile.recommendedEncoderDevice;
    config.renderBackend = "webgpu";
}

} // namespace lireal::system
