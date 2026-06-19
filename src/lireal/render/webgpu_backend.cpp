/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 * Lireal Music - experimental WebGPU render backend.
 * Copyright (C) 2026 Lireal contributors
 */

#include "lireal/render/webgpu_backend.hpp"

namespace lireal::render::gpu {

WebGpuBackendStatus queryWebGpuBackend(const RenderConfig& config) {
    WebGpuBackendStatus status;
    if (config.renderBackend.find("webgpu") == std::string::npos) {
        status.message = "WebGPU is not selected; CPU/OpenCV compositor is active.";
        return status;
    }

#if LIREAL_HAS_WEBGPU
    status.available = true;
    status.shaderEffectsEnabled = true;
    status.backendName = "webgpu-wgsl";
    status.message = "WebGPU compositor is compiled and shader effects are enabled.";
#else
    status.available = false;
    status.shaderEffectsEnabled = false;
    status.backendName = "cpu-opencv-webgpu-fallback";
    status.message = "WebGPU requested but no Dawn/wgpu-native binding was found; using CPU/OpenCV compositor and GPU encoder only.";
#endif
    return status;
}

} // namespace lireal::render::gpu
