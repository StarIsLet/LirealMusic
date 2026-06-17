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

#include "lireal/render/video_renderer.hpp"
#include "lireal/system/hardware_profile.hpp"

#include <algorithm>
#include <atomic>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

namespace {

constexpr const char* kReset = "\033[0m";
constexpr const char* kPink = "\033[38;2;255;142;188m";
constexpr const char* kMint = "\033[38;2;116;223;205m";
constexpr const char* kLavender = "\033[38;2;180;160;255m";
constexpr const char* kCream = "\033[38;2;255;242;214m";
constexpr const char* kDim = "\033[2m";

std::string trim(const std::string& value) {
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

std::string ask(const std::string& label, const std::string& fallback = {}) {
    std::cout << kMint << "  ✦ " << kReset << label;
    if (!fallback.empty()) {
        std::cout << kDim << " [" << fallback << "]" << kReset;
    }
    std::cout << "：";
    std::string input;
    std::getline(std::cin, input);
    input = trim(input);
    return input.empty() ? fallback : input;
}

std::string shellQuote(const std::string& value) {
    std::string quoted = "'";
    for (const char ch : value) {
        if (ch == '\'') {
            quoted += "'\\''";
        } else {
            quoted += ch;
        }
    }
    quoted += "'";
    return quoted;
}

bool commandExists(const std::string& command) {
    return std::system(("command -v " + command + " >/dev/null 2>&1").c_str()) == 0;
}

bool allowDesktopDialogs() {
    const char* forceDialogs = std::getenv("LIREAL_TUI_FILE_DIALOGS");
    if (forceDialogs != nullptr && std::string(forceDialogs) == "1") {
        return true;
    }
    return std::getenv("SNAP") == nullptr && std::getenv("SNAP_NAME") == nullptr;
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
    return trim(output);
}

std::string chooseFile(const std::string& label, const std::string& kdialogFilter, const std::string& zenityFilter) {
    std::cout << kCream << "\n  " << label << kDim << "：回车打开文件选择器；也可直接粘贴路径" << kReset << "\n";
    const std::string typed = ask("路径/回车选择");
    if (!typed.empty()) {
        return typed;
    }
    if (!allowDesktopDialogs()) {
        std::cout << kPink << "  当前像是在 Snap/Portal 环境里，文件对话框容易崩；请直接粘贴路径喵~\n" << kReset;
        return ask(label + "路径");
    }
    if (commandExists("kdialog")) {
        return readCommandOutput("kdialog --getopenfilename \"$HOME\" " + shellQuote(kdialogFilter) + " 2>/dev/null");
    }
    if (commandExists("zenity")) {
        return readCommandOutput("zenity --file-selection --title=" + shellQuote(label) + " --file-filter=" + shellQuote(zenityFilter) + " 2>/dev/null");
    }
    std::cout << kPink << "  没找到 kdialog/zenity，只能手动输入路径喵~\n" << kReset;
    return ask(label + "路径");
}

std::string chooseOutputFile(const std::string& fallback) {
    std::cout << kCream << "\n  输出视频：回车打开保存对话框；也可直接粘贴路径" << kReset << "\n";
    const std::string typed = ask("路径/回车选择");
    if (!typed.empty()) {
        return typed;
    }
    if (!allowDesktopDialogs()) {
        std::cout << kPink << "  当前像是在 Snap/Portal 环境里，保存对话框容易崩；请直接粘贴输出路径喵~\n" << kReset;
        return ask("输出视频路径", fallback);
    }
    if (commandExists("kdialog")) {
        const std::string selected = readCommandOutput("kdialog --getsavefilename \"$HOME/output_lireal.mp4\" " + shellQuote("MP4 Video (*.mp4)") + " 2>/dev/null");
        return selected.empty() ? fallback : selected;
    }
    if (commandExists("zenity")) {
        const std::string selected = readCommandOutput("zenity --file-selection --save --confirm-overwrite --title='输出MP4路径' --file-filter='MP4 video | *.mp4' 2>/dev/null");
        return selected.empty() ? fallback : selected;
    }
    return fallback;
}

int askInt(const std::string& label, int fallback, int minValue, int maxValue) {
    for (;;) {
        const std::string value = ask(label, std::to_string(fallback));
        try {
            return std::clamp(std::stoi(value), minValue, maxValue);
        } catch (...) {
            std::cout << kPink << "  输入数字喵~\n" << kReset;
        }
    }
}

void banner() {
    std::cout << "\033[2J\033[H";
    std::cout << kPink << "╭──────────────────────────────────────────────╮\n";
    std::cout << "│  🌸 LirealMusic TUI · 可爱高速MV生成器 喵~  │\n";
    std::cout << "╰──────────────────────────────────────────────╯\n" << kReset;
}

void progressBar(const lireal::render::RenderProgress& progress) {
    const int width = 36;
    const int filled = static_cast<int>(std::clamp(progress.progress, 0.0, 1.0) * width);
    std::cout << "\r" << kLavender << "  渲染 " << kReset << "[";
    for (int i = 0; i < width; ++i) {
        std::cout << (i < filled ? "♥" : "·");
    }
    std::cout << "] " << std::setw(3) << static_cast<int>(progress.progress * 100.0) << "%  " << progress.message << std::flush;
    if (progress.currentFrame >= progress.totalFrames) {
        std::cout << "\n";
    }
}

} // namespace

int main() {
    try {
        banner();
        lireal::render::RenderConfig config;
        config.backgroundImagePath = chooseFile("选择背景图片", "Images (*.png *.jpg *.jpeg *.webp *.bmp)", "Images | *.png *.jpg *.jpeg *.webp *.bmp");
        config.musicPath = chooseFile("选择音乐文件", "Audio (*.mp3 *.wav *.flac *.aac *.ogg *.m4a)", "Audio | *.mp3 *.wav *.flac *.aac *.ogg *.m4a");
        config.lyricPath = chooseFile("选择LRC/歌词", "Lyrics (*.lrc *.txt)", "Lyrics | *.lrc *.txt");
        config.outputVideoPath = chooseOutputFile("output_lireal.mp4");
        config.songTitle = ask("标题", "Lireal Music");
        config.artistName = ask("作者/歌手", "Unknown Artist");

        const int preset = askInt("画质 1=1080p 2=2K 3=4K", 3, 1, 3);
        if (preset == 1) {
            config.width = 1920;
            config.height = 1080;
        } else if (preset == 2) {
            config.width = 2560;
            config.height = 1440;
        } else {
            config.width = 3840;
            config.height = 2160;
        }
        config.fps = askInt("帧率", 60, 24, 120);
        config.encoderPreset = "veryfast";
        config.encoderCrf = 16;
        config.lyricLayoutMode = 0;

        const auto profile = lireal::system::detectHardwareProfile(config.width, config.height, config.fps);
        lireal::system::applyHardwareProfile(config, profile);
        std::cout << kCream << "\n  自动硬件配置：" << profile.summary << "\n" << kReset;
        std::cout << kDim << "  WebGPU：全局启用；运行库不可用时自动走兼容合成回退\n" << kReset;

        const int manualThreads = askInt("同时渲染线程，0=使用自动推荐", 0, 0, 96);
        if (manualThreads > 0) {
            config.renderThreads = manualThreads;
        }

        std::cout << kPink << "\n  开始生成啦，滚动歌词和高并发会一起跑喵~\n" << kReset;
        lireal::render::VideoRenderer renderer;
        std::atomic_bool cancel = false;
        renderer.render(config, progressBar, [&]() { return cancel.load(); });
        std::cout << kMint << "  完成：" << config.outputVideoPath.string() << "\n" << kReset;
    } catch (const std::exception& error) {
        std::cerr << "\n" << kPink << "  失败喵：" << error.what() << kReset << "\n";
        return 1;
    }
    return 0;
}
