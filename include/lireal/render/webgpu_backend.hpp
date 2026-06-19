/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 * Lireal Music - experimental WebGPU render backend.
 * Copyright (C) 2026 Lireal contributors
 */

#pragma once

#include "lireal/render/render_config.hpp"

#include <string>

namespace lireal::render::gpu {

struct WebGpuBackendStatus {
    bool available = false;
    bool shaderEffectsEnabled = false;
    std::string backendName = "cpu-opencv";
    std::string message = "WebGPU backend is not compiled; CPU/OpenCV compositor is active.";
};

WebGpuBackendStatus queryWebGpuBackend(const RenderConfig& config);

} // namespace lireal::render::gpu
