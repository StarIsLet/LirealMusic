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

#include "lireal/audio/onnx_source_separator.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <numeric>
#include <stdexcept>

#if LIREAL_HAS_ONNXRUNTIME
#include <onnxruntime_cxx_api.h>
#endif

namespace lireal::audio {
namespace {

constexpr int kExpectedSampleRate = 44100;
constexpr int kChannels = 2;
constexpr int kStemCount = 4;
constexpr int kChunkSamples = 44100 * 8;
constexpr int kHopSamples = 44100 * 6;

std::filesystem::path defaultModelPath() {
    if (const char* envPath = std::getenv("LIREAL_ONNX_SEPARATOR_MODEL"); envPath != nullptr && *envPath != '\0') {
        return envPath;
    }
    return std::filesystem::path("models") / "source_separation.onnx";
}

std::vector<float> resampleLinear(const std::vector<float>& stereoSamples, int inputRate, int outputRate) {
    if (inputRate == outputRate || stereoSamples.empty()) {
        return stereoSamples;
    }
    const std::size_t inputFrames = stereoSamples.size() / 2U;
    const std::size_t outputFrames = static_cast<std::size_t>(static_cast<double>(inputFrames) * static_cast<double>(outputRate) / static_cast<double>(inputRate));
    std::vector<float> output(outputFrames * 2U, 0.0F);
    for (std::size_t frame = 0; frame < outputFrames; ++frame) {
        const double source = static_cast<double>(frame) * static_cast<double>(inputRate) / static_cast<double>(outputRate);
        const std::size_t i0 = std::min<std::size_t>(static_cast<std::size_t>(source), inputFrames - 1U);
        const std::size_t i1 = std::min<std::size_t>(i0 + 1U, inputFrames - 1U);
        const float t = static_cast<float>(source - static_cast<double>(i0));
        for (int channel = 0; channel < 2; ++channel) {
            const float a = stereoSamples[i0 * 2U + static_cast<std::size_t>(channel)];
            const float b = stereoSamples[i1 * 2U + static_cast<std::size_t>(channel)];
            output[frame * 2U + static_cast<std::size_t>(channel)] = a + (b - a) * t;
        }
    }
    return output;
}

std::vector<float> makeSineWindow(int size) {
    std::vector<float> window(static_cast<std::size_t>(size), 1.0F);
    for (int index = 0; index < size; ++index) {
        window[static_cast<std::size_t>(index)] = std::sin(3.14159265358979323846 * (static_cast<double>(index) + 0.5) / static_cast<double>(size));
    }
    return window;
}

} // namespace

OnnxSourceSeparator::OnnxSourceSeparator(std::filesystem::path modelPath)
    : modelPath_(modelPath.empty() ? defaultModelPath() : std::move(modelPath)) {}

OnnxSourceSeparator::~OnnxSourceSeparator() = default;

bool OnnxSourceSeparator::isAvailable() const {
#if LIREAL_HAS_ONNXRUNTIME
    return std::filesystem::exists(modelPath_) && std::filesystem::is_regular_file(modelPath_);
#else
    return false;
#endif
}

std::string OnnxSourceSeparator::backendName() const {
#if LIREAL_HAS_ONNXRUNTIME
    return "ONNX Runtime";
#else
    return "ONNX Runtime disabled";
#endif
}

SourceSeparationResult OnnxSourceSeparator::separate(const std::vector<float>& stereoSamples, int sampleRate) const {
    if (!isAvailable()) {
        throw std::runtime_error("ONNX 源分离模型不可用");
    }

    SourceSeparationResult result;
    result.sampleRate = sampleRate;
    result.stems = {
        {"Vocals", {}},
        {"Drums", {}},
        {"Bass", {}},
        {"Other", {}},
        {"Accompaniment", {}}
    };

#if LIREAL_HAS_ONNXRUNTIME
    const std::vector<float> modelInputStereo = resampleLinear(stereoSamples, sampleRate, kExpectedSampleRate);
    const std::size_t totalFrames = modelInputStereo.size() / 2U;
    std::array<std::vector<float>, kStemCount> separated;
    std::vector<float> weightSum(totalFrames, 0.0F);
    for (auto& stem : separated) {
        stem.assign(totalFrames, 0.0F);
    }

    Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "lireal-source-separator");
    Ort::SessionOptions sessionOptions;
    sessionOptions.SetIntraOpNumThreads(1);
    sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    Ort::Session session(env, modelPath_.c_str(), sessionOptions);
    Ort::AllocatorWithDefaultOptions allocator;

    const std::string inputName = session.GetInputNameAllocated(0, allocator).get();
    const std::string outputName = session.GetOutputNameAllocated(0, allocator).get();
    const char* inputNames[] = {inputName.c_str()};
    const char* outputNames[] = {outputName.c_str()};
    const std::vector<float> window = makeSineWindow(kChunkSamples);
    Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    for (std::size_t offset = 0; offset < totalFrames; offset += kHopSamples) {
        std::vector<float> input(static_cast<std::size_t>(kChannels * kChunkSamples), 0.0F);
        for (int frame = 0; frame < kChunkSamples; ++frame) {
            const std::size_t sourceFrame = offset + static_cast<std::size_t>(frame);
            if (sourceFrame >= totalFrames) {
                break;
            }
            input[static_cast<std::size_t>(frame)] = modelInputStereo[sourceFrame * 2U];
            input[static_cast<std::size_t>(kChunkSamples + frame)] = modelInputStereo[sourceFrame * 2U + 1U];
        }

        std::array<int64_t, 3> inputShape = {1, kChannels, kChunkSamples};
        Ort::Value inputTensor = Ort::Value::CreateTensor<float>(memoryInfo, input.data(), input.size(), inputShape.data(), inputShape.size());
        auto outputs = session.Run(Ort::RunOptions{nullptr}, inputNames, &inputTensor, 1, outputNames, 1);
        const float* output = outputs.front().GetTensorData<float>();

        for (int stem = 0; stem < kStemCount; ++stem) {
            for (int frame = 0; frame < kChunkSamples; ++frame) {
                const std::size_t targetFrame = offset + static_cast<std::size_t>(frame);
                if (targetFrame >= totalFrames) {
                    break;
                }
                const float left = output[static_cast<std::size_t>(stem * kChannels * kChunkSamples + frame)];
                const float right = output[static_cast<std::size_t>(stem * kChannels * kChunkSamples + kChunkSamples + frame)];
                const float mono = (left + right) * 0.5F;
                const float weight = window[static_cast<std::size_t>(frame)];
                separated[static_cast<std::size_t>(stem)][targetFrame] += mono * weight;
                if (stem == 0) {
                    weightSum[targetFrame] += weight;
                }
            }
        }

        if (offset + kHopSamples >= totalFrames && offset + kChunkSamples >= totalFrames) {
            break;
        }
    }

    for (auto& stem : separated) {
        for (std::size_t frame = 0; frame < stem.size(); ++frame) {
            stem[frame] /= std::max(0.000001F, weightSum[frame]);
        }
    }

    const std::array<std::string, kStemCount> names = {"Vocals", "Drums", "Bass", "Other"};
    for (int stem = 0; stem < kStemCount; ++stem) {
        result.stems[static_cast<std::size_t>(stem)].monoSamples = resampleLinear([&]() {
            std::vector<float> pseudoStereo(separated[static_cast<std::size_t>(stem)].size() * 2U, 0.0F);
            for (std::size_t i = 0; i < separated[static_cast<std::size_t>(stem)].size(); ++i) {
                pseudoStereo[i * 2U] = separated[static_cast<std::size_t>(stem)][i];
                pseudoStereo[i * 2U + 1U] = separated[static_cast<std::size_t>(stem)][i];
            }
            return pseudoStereo;
        }(), kExpectedSampleRate, sampleRate);
        std::vector<float> mono;
        mono.reserve(result.stems[static_cast<std::size_t>(stem)].monoSamples.size() / 2U);
        for (std::size_t i = 0; i + 1 < result.stems[static_cast<std::size_t>(stem)].monoSamples.size(); i += 2) {
            mono.push_back((result.stems[static_cast<std::size_t>(stem)].monoSamples[i] + result.stems[static_cast<std::size_t>(stem)].monoSamples[i + 1U]) * 0.5F);
        }
        result.stems[static_cast<std::size_t>(stem)].name = names[static_cast<std::size_t>(stem)];
        result.stems[static_cast<std::size_t>(stem)].monoSamples = std::move(mono);
    }

    const std::size_t frameCount = result.stems[0].monoSamples.size();
    result.stems[4].monoSamples.assign(frameCount, 0.0F);
    for (std::size_t frame = 0; frame < frameCount; ++frame) {
        const float drums = frame < result.stems[1].monoSamples.size() ? result.stems[1].monoSamples[frame] : 0.0F;
        const float bass = frame < result.stems[2].monoSamples.size() ? result.stems[2].monoSamples[frame] : 0.0F;
        const float other = frame < result.stems[3].monoSamples.size() ? result.stems[3].monoSamples[frame] : 0.0F;
        result.stems[4].monoSamples[frame] = drums + bass + other;
    }
#else
    (void)stereoSamples;
    throw std::runtime_error("当前构建未启用 ONNX Runtime");
#endif

    return result;
}

} // namespace lireal::audio