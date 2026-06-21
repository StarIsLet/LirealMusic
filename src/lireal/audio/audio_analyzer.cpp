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

#include "lireal/audio/audio_analyzer.hpp"

#include "lireal/audio/onnx_source_separator.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

#include <opencv2/core.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <memory>
#include <numeric>
#include <stdexcept>

namespace lireal::audio {
namespace {

struct FormatContextDeleter {
    void operator()(AVFormatContext* context) const {
        if (context != nullptr) {
            avformat_close_input(&context);
        }
    }
};

struct CodecContextDeleter {
    void operator()(AVCodecContext* context) const {
        avcodec_free_context(&context);
    }
};

struct FrameDeleter {
    void operator()(AVFrame* frame) const {
        av_frame_free(&frame);
    }
};

struct PacketDeleter {
    void operator()(AVPacket* packet) const {
        av_packet_free(&packet);
    }
};

struct SwrDeleter {
    void operator()(SwrContext* context) const {
        swr_free(&context);
    }
};

int nextPowerOfTwo(int value) {
    int result = 1;
    while (result < value) {
        result <<= 1;
    }
    return result;
}

std::vector<float> makeHannWindow(int size) {
    std::vector<float> window(static_cast<std::size_t>(size));
    if (size <= 1) {
        std::fill(window.begin(), window.end(), 1.0F);
        return window;
    }
    for (int index = 0; index < size; ++index) {
        window[static_cast<std::size_t>(index)] = 0.5F - 0.5F * std::cos(2.0F * static_cast<float>(M_PI) * static_cast<float>(index) / static_cast<float>(size - 1));
    }
    return window;
}

float magnitudeBandEnergy(const cv::Mat& magnitudes, int sampleRate, int dftSize, float lowHz, float highHz) {
    const int firstBin = std::clamp(static_cast<int>(std::floor(lowHz * static_cast<float>(dftSize) / static_cast<float>(sampleRate))), 1, magnitudes.rows - 1);
    const int lastBin = std::clamp(static_cast<int>(std::ceil(highHz * static_cast<float>(dftSize) / static_cast<float>(sampleRate))), firstBin, magnitudes.rows - 1);
    double sum = 0.0;
    for (int bin = firstBin; bin <= lastBin; ++bin) {
        const float value = magnitudes.at<float>(bin, 0);
        sum += static_cast<double>(value) * static_cast<double>(value);
    }
    const double mean = sum / static_cast<double>(std::max(1, lastBin - firstBin + 1));
    return static_cast<float>(std::log1p(std::sqrt(mean) * 8.0));
}

std::vector<float> makeSpectrumBins(const cv::Mat& magnitudes, int sampleRate, int dftSize) {
    std::vector<float> bins(64, 0.0F);
    for (int index = 0; index < 64; ++index) {
        const float low = 28.0F * std::pow(1.083F, static_cast<float>(index));
        const float high = std::min(18000.0F, low * 1.14F);
        bins[static_cast<std::size_t>(index)] = magnitudeBandEnergy(magnitudes, sampleRate, dftSize, low, high);
    }
    return bins;
}

float normalizeEnergy(float value, float maxValue, float gain = 1.0F) {
    if (maxValue <= 0.000001F) {
        return 0.0F;
    }
    return std::clamp(std::pow(std::clamp(value / maxValue, 0.0F, 2.0F), 0.62F) * gain, 0.0F, 1.0F);
}

float smoothAttackRelease(float previous, float current, float attack, float release) {
    const float factor = current > previous ? attack : release;
    return previous + (current - previous) * factor;
}

float sampleRmsInRange(const std::vector<float>& samples, int begin, int end) {
    if (samples.empty() || begin >= end) {
        return 0.0F;
    }
    const int safeBegin = std::clamp(begin, 0, static_cast<int>(samples.size()));
    const int safeEnd = std::clamp(end, safeBegin, static_cast<int>(samples.size()));
    double squareSum = 0.0;
    for (int index = safeBegin; index < safeEnd; ++index) {
        const float sample = samples[static_cast<std::size_t>(index)];
        squareSum += static_cast<double>(sample) * static_cast<double>(sample);
    }
    return static_cast<float>(std::sqrt(squareSum / static_cast<double>(std::max(1, safeEnd - safeBegin))));
}

} // namespace

AudioAnalyzer::AudioAnalyzer() = default;
AudioAnalyzer::~AudioAnalyzer() = default;

AudioAnalysisResult AudioAnalyzer::analyze(const std::filesystem::path& audioPath, int targetFps) const {
    AVFormatContext* rawFormat = nullptr;
    if (avformat_open_input(&rawFormat, audioPath.c_str(), nullptr, nullptr) < 0) {
        throw std::runtime_error("无法打开音频/视频音乐来源: " + audioPath.string());
    }
    std::unique_ptr<AVFormatContext, FormatContextDeleter> format(rawFormat);

    if (avformat_find_stream_info(format.get(), nullptr) < 0) {
        throw std::runtime_error("无法读取音频/视频流信息: " + audioPath.string());
    }

    const int streamIndex = av_find_best_stream(format.get(), AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (streamIndex < 0) {
        throw std::runtime_error("文件中没有可用音频流，请选择带声音的视频或音乐文件: " + audioPath.string());
    }

    AVStream* stream = format->streams[streamIndex];
    const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (codec == nullptr) {
        throw std::runtime_error("找不到音频解码器");
    }

    std::unique_ptr<AVCodecContext, CodecContextDeleter> codecContext(avcodec_alloc_context3(codec));
    avcodec_parameters_to_context(codecContext.get(), stream->codecpar);
    if (avcodec_open2(codecContext.get(), codec, nullptr) < 0) {
        throw std::runtime_error("无法打开音频解码器");
    }

    constexpr int outputSampleRate = 48000;
    AVChannelLayout outLayout;
    av_channel_layout_default(&outLayout, 2);

    SwrContext* rawSwr = nullptr;
    swr_alloc_set_opts2(
        &rawSwr,
        &outLayout,
        AV_SAMPLE_FMT_FLT,
        outputSampleRate,
        &codecContext->ch_layout,
        codecContext->sample_fmt,
        codecContext->sample_rate,
        0,
        nullptr);
    std::unique_ptr<SwrContext, SwrDeleter> swr(rawSwr);
    if (swr == nullptr || swr_init(swr.get()) < 0) {
        av_channel_layout_uninit(&outLayout);
        throw std::runtime_error("无法初始化音频重采样器");
    }

    std::vector<float> stereoSamples;
    std::unique_ptr<AVPacket, PacketDeleter> packet(av_packet_alloc());
    std::unique_ptr<AVFrame, FrameDeleter> frame(av_frame_alloc());

    auto receiveFrames = [&]() {
        while (avcodec_receive_frame(codecContext.get(), frame.get()) == 0) {
            const int dstSamples = av_rescale_rnd(
                swr_get_delay(swr.get(), codecContext->sample_rate) + frame->nb_samples,
                outputSampleRate,
                codecContext->sample_rate,
                AV_ROUND_UP);
            std::vector<float> converted(static_cast<std::size_t>(dstSamples) * 2U);
            uint8_t* outData[] = {reinterpret_cast<uint8_t*>(converted.data())};
            const int convertedCount = swr_convert(
                swr.get(),
                outData,
                dstSamples,
                const_cast<const uint8_t**>(frame->extended_data),
                frame->nb_samples);
            if (convertedCount > 0) {
                converted.resize(static_cast<std::size_t>(convertedCount) * 2U);
                stereoSamples.insert(stereoSamples.end(), converted.begin(), converted.end());
            }
            av_frame_unref(frame.get());
        }
    };

    while (av_read_frame(format.get(), packet.get()) >= 0) {
        if (packet->stream_index == streamIndex) {
            if (avcodec_send_packet(codecContext.get(), packet.get()) == 0) {
                receiveFrames();
            }
        }
        av_packet_unref(packet.get());
    }
    avcodec_send_packet(codecContext.get(), nullptr);
    receiveFrames();
    av_channel_layout_uninit(&outLayout);

    if (stereoSamples.empty()) {
        throw std::runtime_error("音乐解码后没有采样数据");
    }

    std::vector<float> monoSamples;
    monoSamples.reserve(stereoSamples.size() / 2U);
    for (std::size_t index = 0; index + 1 < stereoSamples.size(); index += 2) {
        monoSamples.push_back((stereoSamples[index] + stereoSamples[index + 1]) * 0.5F);
    }

    const int samplesPerFrame = std::max(1, outputSampleRate / std::max(1, targetFps));
    const int totalFrames = static_cast<int>((monoSamples.size() + samplesPerFrame - 1) / samplesPerFrame);
    const int dftSize = std::clamp(nextPowerOfTwo(samplesPerFrame * 2), 2048, 8192);
    const std::vector<float> hannWindow = makeHannWindow(samplesPerFrame);

    AudioAnalysisResult result;
    result.durationSeconds = static_cast<double>(monoSamples.size()) / static_cast<double>(outputSampleRate);
    result.sampleRate = outputSampleRate;
    result.channels = 2;
    result.frames.reserve(static_cast<std::size_t>(totalFrames));

    SourceSeparationResult neuralSeparation;
    bool hasNeuralSeparation = false;
    try {
        OnnxSourceSeparator separator;
        if (separator.isAvailable()) {
            neuralSeparation = separator.separate(stereoSamples, outputSampleRate);
            hasNeuralSeparation = !neuralSeparation.stems.empty();
        }
    } catch (const std::exception&) {
        hasNeuralSeparation = false;
    }

    std::vector<AudioFrameEnergy> rawFrames(static_cast<std::size_t>(totalFrames));
    float maxRms = 0.000001F;
    float maxBass = 0.000001F;
    float maxMid = 0.000001F;
    float maxTreble = 0.000001F;
    float maxVocal = 0.000001F;
    float maxPercussion = 0.000001F;
    float maxAmbience = 0.000001F;
    float maxTransient = 0.000001F;
    float maxSpectralFlux = 0.000001F;
    float maxBeatPulse = 0.000001F;
    float maxDropIntensity = 0.000001F;
    std::vector<float> maxBins(64, 0.000001F);
    std::vector<float> maxNeuralStemEnergy(hasNeuralSeparation ? neuralSeparation.stems.size() : 0U, 0.000001F);
    float previousRms = 0.0F;
    float previousBass = 0.0F;
    float previousPercussion = 0.0F;
    std::vector<float> previousSpectrumBins(64, 0.0F);

    for (int frameIndex = 0; frameIndex < totalFrames; ++frameIndex) {
        const int begin = frameIndex * samplesPerFrame;
        const int end = std::min<int>((frameIndex + 1) * samplesPerFrame, monoSamples.size());

        double squareSum = 0.0;
        cv::Mat dftInput = cv::Mat::zeros(dftSize, 1, CV_32F);
        for (int sampleIndex = begin; sampleIndex < end; ++sampleIndex) {
            const int localIndex = sampleIndex - begin;
            const float sample = monoSamples[static_cast<std::size_t>(sampleIndex)];
            squareSum += static_cast<double>(sample) * static_cast<double>(sample);
            dftInput.at<float>(localIndex, 0) = sample * hannWindow[static_cast<std::size_t>(localIndex)];
        }

        cv::Mat complexSpectrum;
        cv::dft(dftInput, complexSpectrum, cv::DFT_COMPLEX_OUTPUT);
        std::vector<cv::Mat> planes;
        cv::split(complexSpectrum, planes);
        cv::Mat magnitudes;
        cv::magnitude(planes[0].rowRange(0, dftSize / 2), planes[1].rowRange(0, dftSize / 2), magnitudes);

        AudioFrameEnergy energy;
        energy.timeSeconds = static_cast<double>(frameIndex) / static_cast<double>(targetFps);
        energy.rms = static_cast<float>(std::sqrt(squareSum / static_cast<double>(std::max(1, end - begin))));
        energy.bass = magnitudeBandEnergy(magnitudes, outputSampleRate, dftSize, 35.0F, 180.0F);
        energy.mid = magnitudeBandEnergy(magnitudes, outputSampleRate, dftSize, 220.0F, 2600.0F);
        energy.treble = magnitudeBandEnergy(magnitudes, outputSampleRate, dftSize, 3200.0F, 14000.0F);
        const float vocalFundamental = magnitudeBandEnergy(magnitudes, outputSampleRate, dftSize, 140.0F, 520.0F);
        const float vocalPresence = magnitudeBandEnergy(magnitudes, outputSampleRate, dftSize, 1200.0F, 4200.0F);
        const float airBand = magnitudeBandEnergy(magnitudes, outputSampleRate, dftSize, 7500.0F, 16000.0F);
        energy.vocal = vocalFundamental * 0.46F + vocalPresence * 0.54F;
        energy.percussion = energy.bass * 0.45F + energy.treble * 0.55F;
        energy.ambience = airBand * 0.72F + energy.treble * 0.28F;
        energy.transient = std::max(0.0F, energy.rms - previousRms);
        previousRms = energy.rms;

        double leftSquare = 0.0;
        double rightSquare = 0.0;
        double sideSquare = 0.0;
        double midSquare = 0.0;
        double correlationSum = 0.0;
        for (int sampleIndex = begin; sampleIndex < end; ++sampleIndex) {
            const std::size_t stereoIndex = static_cast<std::size_t>(sampleIndex) * 2U;
            const float left = stereoSamples[stereoIndex];
            const float right = stereoSamples[stereoIndex + 1U];
            const float mid = (left + right) * 0.5F;
            const float side = (left - right) * 0.5F;
            leftSquare += static_cast<double>(left) * static_cast<double>(left);
            rightSquare += static_cast<double>(right) * static_cast<double>(right);
            midSquare += static_cast<double>(mid) * static_cast<double>(mid);
            sideSquare += static_cast<double>(side) * static_cast<double>(side);
            correlationSum += static_cast<double>(left) * static_cast<double>(right);
        }
        const double leftRms = std::sqrt(leftSquare / static_cast<double>(std::max(1, end - begin)));
        const double rightRms = std::sqrt(rightSquare / static_cast<double>(std::max(1, end - begin)));
        const double correlation = correlationSum / (std::sqrt(leftSquare * rightSquare) + 0.000001);
        const double decorrelationWidth = std::sqrt(std::max(0.0, 1.0 - correlation)) * 0.72;
        energy.stereoWidth = static_cast<float>(std::clamp(std::sqrt(sideSquare) / (std::sqrt(midSquare) + 0.000001) * 0.72 + decorrelationWidth, 0.0, 2.2));
        energy.spectrumBins = makeSpectrumBins(magnitudes, outputSampleRate, dftSize);

        double centroidWeighted = 0.0;
        double centroidEnergy = 0.0;
        for (int bin = 1; bin < magnitudes.rows; ++bin) {
            const double magnitude = magnitudes.at<float>(bin, 0);
            const double frequency = static_cast<double>(bin) * static_cast<double>(outputSampleRate) / static_cast<double>(dftSize);
            centroidWeighted += frequency * magnitude;
            centroidEnergy += magnitude;
        }
        energy.spectralCentroid = static_cast<float>(std::clamp((centroidWeighted / (centroidEnergy + 0.000001)) / 9000.0, 0.0, 1.0));
        double positiveFlux = 0.0;
        double positiveLowFlux = 0.0;
        for (std::size_t bin = 0; bin < energy.spectrumBins.size(); ++bin) {
            const float binRise = std::max(0.0F, energy.spectrumBins[bin] - previousSpectrumBins[bin]);
            positiveFlux += binRise;
            if (bin < 14U) {
                positiveLowFlux += binRise * (1.0 + static_cast<double>(14U - bin) / 14.0);
            }
        }
        energy.spectralFlux = static_cast<float>(positiveFlux / static_cast<double>(std::max<std::size_t>(1U, energy.spectrumBins.size())));
        for (std::size_t bin = 0; bin < energy.spectrumBins.size(); ++bin) {
            previousSpectrumBins[bin] = energy.spectrumBins[bin];
        }
        const float bassRise = std::max(0.0F, energy.bass - previousBass);
        const float percussionRise = std::max(0.0F, energy.percussion - previousPercussion);
        const float lowFlux = static_cast<float>(positiveLowFlux / 14.0);
        previousBass = energy.bass;
        previousPercussion = energy.percussion;
        energy.beatPulse = std::max({energy.transient * 2.25F, energy.spectralFlux * 0.96F, lowFlux * 1.42F, bassRise * 1.55F, percussionRise * 1.18F});
        energy.dropIntensity = std::clamp(energy.bass * 0.72F + energy.percussion * 0.42F + lowFlux * 1.10F + energy.spectralFlux * 0.52F, 0.0F, 9.0F);
        energy.colorMood = std::clamp(energy.spectralCentroid * 0.50F + energy.vocal * 0.24F + energy.ambience * 0.26F, 0.0F, 1.0F);

        const float pan = static_cast<float>(std::clamp((rightRms - leftRms) / (leftRms + rightRms + 0.000001), -1.0, 1.0));
        const float stereoLift = std::clamp((energy.stereoWidth - 0.24F) * 1.45F, 0.0F, 1.0F);
        const float surroundPan = std::clamp(pan * 1.90F + (energy.stereoWidth - 0.35F) * 0.52F, -1.0F, 1.0F);
        const float rearDepth = std::clamp(0.36F + stereoLift * 0.50F + energy.ambience * 0.24F, 0.0F, 1.0F);
        energy.stems = {
            {"Lead Vocal", energy.vocal, vocalPresence, pan * 0.12F, 0.06F + energy.vocal * 0.08F, 0.38F, 0.22F},
            {"Harmony / Crowd", energy.mid * 0.50F + energy.ambience * 0.55F, energy.mid, -surroundPan * 0.70F, 0.50F + stereoLift * 0.22F, 0.66F, 0.88F},
            {"Bass / Kick", energy.bass, energy.bass, pan * 0.08F, 0.08F, -0.54F, 0.22F},
            {"Percussion", energy.percussion, energy.transient + energy.treble * 0.44F, surroundPan * 0.96F, 0.22F + energy.transient * 0.30F, 0.10F, 0.76F},
            {"Air / Reverb", energy.ambience, airBand, -surroundPan, rearDepth, 0.78F, 1.0F}
        };

        if (hasNeuralSeparation) {
            energy.stems.clear();
            for (std::size_t stemIndex = 0; stemIndex < neuralSeparation.stems.size(); ++stemIndex) {
                const auto& separatedStem = neuralSeparation.stems[stemIndex];
                const float stemEnergy = sampleRmsInRange(separatedStem.monoSamples, begin, end);
                maxNeuralStemEnergy[stemIndex] = std::max(maxNeuralStemEnergy[stemIndex], stemEnergy);

                float stemPan = surroundPan * 0.24F;
                float stemDepth = 0.42F;
                float stemHeight = 0.12F;
                float stemWidth = 0.50F;
                if (separatedStem.name == "Vocals") {
                    stemPan = pan * 0.16F;
                    stemDepth = 0.10F;
                    stemHeight = 0.34F;
                    stemWidth = 0.30F;
                    energy.vocal = stemEnergy;
                } else if (separatedStem.name == "Drums") {
                    stemPan = surroundPan * 0.84F;
                    stemDepth = 0.26F;
                    stemHeight = 0.08F;
                    stemWidth = 0.72F;
                    energy.percussion = stemEnergy;
                } else if (separatedStem.name == "Bass") {
                    stemPan = pan * 0.08F;
                    stemDepth = 0.08F;
                    stemHeight = -0.56F;
                    stemWidth = 0.28F;
                    energy.bass = stemEnergy;
                } else if (separatedStem.name == "Other") {
                    stemPan = -surroundPan * 0.56F;
                    stemDepth = 0.60F;
                    stemHeight = 0.38F;
                    stemWidth = 0.82F;
                    energy.mid = std::max(energy.mid, stemEnergy);
                } else if (separatedStem.name == "Accompaniment") {
                    stemPan = -surroundPan;
                    stemDepth = rearDepth;
                    stemHeight = 0.62F;
                    stemWidth = 1.0F;
                    energy.ambience = std::max(energy.ambience, stemEnergy);
                }
                energy.stems.push_back({separatedStem.name, stemEnergy, stemEnergy, stemPan, stemDepth, stemHeight, stemWidth});
            }
        }

        maxRms = std::max(maxRms, energy.rms);
        maxBass = std::max(maxBass, energy.bass);
        maxMid = std::max(maxMid, energy.mid);
        maxTreble = std::max(maxTreble, energy.treble);
        maxVocal = std::max(maxVocal, energy.vocal);
        maxPercussion = std::max(maxPercussion, energy.percussion);
        maxAmbience = std::max(maxAmbience, energy.ambience);
        maxTransient = std::max(maxTransient, energy.transient);
        maxSpectralFlux = std::max(maxSpectralFlux, energy.spectralFlux);
        maxBeatPulse = std::max(maxBeatPulse, energy.beatPulse);
        maxDropIntensity = std::max(maxDropIntensity, energy.dropIntensity);
        for (std::size_t bin = 0; bin < energy.spectrumBins.size(); ++bin) {
            maxBins[bin] = std::max(maxBins[bin], energy.spectrumBins[bin]);
        }
        rawFrames[static_cast<std::size_t>(frameIndex)] = std::move(energy);
    }

    float smoothRms = 0.0F;
    float smoothBass = 0.0F;
    float smoothMid = 0.0F;
    float smoothTreble = 0.0F;
    float smoothVocal = 0.0F;
    float smoothPercussion = 0.0F;
    float smoothAmbience = 0.0F;
    float smoothTransient = 0.0F;
    float smoothSpectralFlux = 0.0F;
    float smoothBeatPulse = 0.0F;
    float smoothDropIntensity = 0.0F;
    float smoothColorMood = 0.0F;
    std::vector<float> smoothBins(64, 0.0F);
    for (AudioFrameEnergy& energy : rawFrames) {
        smoothRms = smoothAttackRelease(smoothRms, normalizeEnergy(energy.rms, maxRms, 1.25F), 0.42F, 0.12F);
        smoothBass = smoothAttackRelease(smoothBass, normalizeEnergy(energy.bass, maxBass, 1.28F), 0.62F, 0.18F);
        smoothMid = smoothAttackRelease(smoothMid, normalizeEnergy(energy.mid, maxMid, 1.08F), 0.42F, 0.14F);
        smoothTreble = smoothAttackRelease(smoothTreble, normalizeEnergy(energy.treble, maxTreble, 1.12F), 0.48F, 0.18F);
        smoothVocal = smoothAttackRelease(smoothVocal, normalizeEnergy(energy.vocal, maxVocal, 1.14F), 0.46F, 0.13F);
        smoothPercussion = smoothAttackRelease(smoothPercussion, normalizeEnergy(energy.percussion, maxPercussion, 1.26F), 0.72F, 0.22F);
        smoothAmbience = smoothAttackRelease(smoothAmbience, normalizeEnergy(energy.ambience, maxAmbience, 1.18F), 0.42F, 0.12F);
        smoothTransient = smoothAttackRelease(smoothTransient, normalizeEnergy(energy.transient, maxTransient, 1.38F), 0.70F, 0.22F);
        smoothSpectralFlux = smoothAttackRelease(smoothSpectralFlux, normalizeEnergy(energy.spectralFlux, maxSpectralFlux, 1.32F), 0.68F, 0.18F);
        const float punchTarget = std::clamp(normalizeEnergy(energy.beatPulse, maxBeatPulse, 1.60F) * 0.78F + smoothBass * 0.15F + smoothPercussion * 0.18F, 0.0F, 1.0F);
        const float dropTarget = std::clamp(normalizeEnergy(energy.dropIntensity, maxDropIntensity, 1.42F) * 0.74F + smoothBass * 0.22F + smoothSpectralFlux * 0.18F, 0.0F, 1.0F);
        smoothBeatPulse = smoothAttackRelease(smoothBeatPulse, punchTarget, 0.92F, 0.10F);
        smoothDropIntensity = smoothAttackRelease(smoothDropIntensity, dropTarget, 0.82F, 0.09F);
        smoothColorMood = smoothAttackRelease(smoothColorMood, energy.colorMood, 0.18F, 0.08F);
        energy.rms = smoothRms;
        energy.bass = smoothBass;
        energy.mid = smoothMid;
        energy.treble = smoothTreble;
        energy.vocal = smoothVocal;
        energy.percussion = smoothPercussion;
        energy.ambience = smoothAmbience;
        energy.transient = smoothTransient;
        energy.spectralFlux = smoothSpectralFlux;
        energy.beatPulse = smoothBeatPulse;
        energy.dropIntensity = smoothDropIntensity;
        energy.colorMood = smoothColorMood;
        energy.stereoWidth = std::clamp(std::pow(std::clamp(energy.stereoWidth * 0.92F, 0.0F, 1.0F), 0.72F), 0.0F, 1.0F);
        for (std::size_t bin = 0; bin < energy.spectrumBins.size(); ++bin) {
            const float normalized = normalizeEnergy(energy.spectrumBins[bin], maxBins[bin], 1.08F);
            smoothBins[bin] = smoothAttackRelease(smoothBins[bin], normalized, 0.50F, 0.20F);
            energy.spectrumBins[bin] = smoothBins[bin];
        }
        for (AudioStemEnergy& stem : energy.stems) {
            if (hasNeuralSeparation) {
                const auto found = std::find_if(neuralSeparation.stems.begin(), neuralSeparation.stems.end(), [&](const SourceStemSamples& neuralStem) {
                    return neuralStem.name == stem.name;
                });
                if (found != neuralSeparation.stems.end()) {
                    const std::size_t stemIndex = static_cast<std::size_t>(std::distance(neuralSeparation.stems.begin(), found));
                    stem.energy = normalizeEnergy(stem.energy, maxNeuralStemEnergy[stemIndex], 1.24F);
                    stem.presence = smoothAttackRelease(stem.presence, stem.energy, 0.55F, 0.18F);
                }
                if (stem.name == "Vocals") {
                    stem.depth = 0.16F + energy.vocal * 0.12F;
                    stem.height = 0.32F;
                } else if (stem.name == "Drums") {
                    stem.presence = std::clamp(stem.energy * 0.76F + energy.transient * 0.62F, 0.0F, 1.0F);
                    stem.depth = 0.28F + energy.transient * 0.34F;
                } else if (stem.name == "Bass") {
                    stem.height = -0.50F;
                    stem.depth = 0.14F;
                } else if (stem.name == "Other") {
                    stem.width = std::clamp(0.68F + energy.stereoWidth * 0.28F, 0.0F, 1.0F);
                } else if (stem.name == "Accompaniment") {
                    stem.width = 1.0F;
                    stem.depth = std::clamp(0.76F + energy.ambience * 0.20F, 0.0F, 1.0F);
                }
            } else if (stem.name == "Lead Vocal") {
                stem.energy = energy.vocal;
                stem.presence = std::max(energy.vocal, energy.mid * 0.45F);
            } else if (stem.name == "Harmony / Crowd") {
                stem.energy = std::clamp(energy.mid * 0.62F + energy.ambience * 0.58F, 0.0F, 1.0F);
                stem.presence = energy.mid;
                stem.width = std::clamp(0.58F + energy.stereoWidth * 0.42F, 0.0F, 1.0F);
            } else if (stem.name == "Bass / Kick") {
                stem.energy = energy.bass;
                stem.presence = std::max(energy.bass, energy.transient * 0.4F);
            } else if (stem.name == "Percussion") {
                stem.energy = energy.percussion;
                stem.presence = std::clamp(energy.percussion * 0.74F + energy.transient * 0.62F, 0.0F, 1.0F);
                stem.depth = std::clamp(0.28F + energy.transient * 0.32F, 0.0F, 1.0F);
            } else if (stem.name == "Air / Reverb") {
                stem.energy = energy.ambience;
                stem.presence = std::max(energy.ambience, energy.stereoWidth);
                stem.width = 1.0F;
                stem.depth = std::clamp(0.72F + energy.ambience * 0.20F, 0.0F, 1.0F);
            }
        }
        result.frames.push_back(std::move(energy));
    }

    return result;
}

} // namespace lireal::audio
