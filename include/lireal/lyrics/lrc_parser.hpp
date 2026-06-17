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

namespace lireal::lyrics {

struct LyricLine {
    double startSeconds = 0.0;
    double endSeconds = 0.0;
    std::string text;
};

class LrcParser {
public:
    std::vector<LyricLine> parse(const std::filesystem::path& lrcPath) const;
};

} // namespace lireal::lyrics
