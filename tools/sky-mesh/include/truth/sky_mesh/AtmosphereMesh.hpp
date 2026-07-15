// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace truth::sky_mesh {

struct GenerationOptions {
    std::filesystem::path outputRoot;
    bool emitObj = false;
};

struct GenerationResult {
    std::filesystem::path nifPath;
    std::filesystem::path manifestPath;
    std::filesystem::path objPath;
    std::string sha256;
    std::uintmax_t sizeBytes = 0;
    std::size_t vertices = 0;
    std::size_t triangles = 0;
};

GenerationResult generateAtmosphereFallback(const GenerationOptions& options);

} // namespace truth::sky_mesh
