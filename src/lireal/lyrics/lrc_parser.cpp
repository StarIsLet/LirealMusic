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

#include "lireal/lyrics/lrc_parser.hpp"

#include <algorithm>
#include <fstream>
#include <regex>
#include <stdexcept>

namespace lireal::lyrics {
namespace {

double parseTimestamp(const std::string& minutesText, const std::string& secondsText, const std::string& fractionText) {
    const double minutes = std::stod(minutesText);
    const double seconds = std::stod(secondsText);
    double fraction = 0.0;
    if (!fractionText.empty()) {
        const double scale = fractionText.size() == 2 ? 100.0 : 1000.0;
        fraction = std::stod(fractionText) / scale;
    }
    return minutes * 60.0 + seconds + fraction;
}

std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

} // namespace

std::vector<LyricLine> LrcParser::parse(const std::filesystem::path& lrcPath) const {
    std::ifstream input(lrcPath);
    if (!input.is_open()) {
        throw std::runtime_error("无法打开歌词文件: " + lrcPath.string());
    }

    const std::regex timeRegex(R"(\[(\d{1,3}):(\d{2})(?:\.(\d{2,3}))?\])");
    std::vector<LyricLine> lines;
    std::string rawLine;

    while (std::getline(input, rawLine)) {
        std::vector<double> timestamps;
        for (std::sregex_iterator it(rawLine.begin(), rawLine.end(), timeRegex), end; it != end; ++it) {
            timestamps.push_back(parseTimestamp((*it)[1].str(), (*it)[2].str(), (*it)[3].str()));
        }

        std::string text = std::regex_replace(rawLine, timeRegex, "");
        text = trim(text);
        if (timestamps.empty() || text.empty()) {
            continue;
        }

        for (const double timestamp : timestamps) {
            lines.push_back({timestamp, timestamp + 4.0, text});
        }
    }

    std::sort(lines.begin(), lines.end(), [](const LyricLine& a, const LyricLine& b) {
        return a.startSeconds < b.startSeconds;
    });

    for (std::size_t index = 0; index + 1 < lines.size(); ++index) {
        lines[index].endSeconds = std::max(lines[index].startSeconds + 0.35, lines[index + 1].startSeconds);
    }

    return lines;
}

} // namespace lireal::lyrics
