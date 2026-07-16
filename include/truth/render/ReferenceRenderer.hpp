#pragma once

#include "truth/render/AuroraCurtain.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace truth::render {

enum class ReferenceScene : std::uint8_t {
  day = 0,
  dusk = 1,
  quiet_clear_night = 2,
  active_clear_night = 3,
  clear_night_aurora = active_clear_night,
  cloudy_night_aurora = 4,
  storm = 5,
  translated_day_probe = 6,
};

enum class ReferenceRenderStatus : std::uint8_t {
  rendered = 0,
  invalid_request = 1,
  shader_compile_failed = 2,
  device_failed = 3,
  gpu_failed = 4,
  readback_failed = 5,
};

struct ReferenceImage {
  std::uint32_t width{};
  std::uint32_t height{};
  std::vector<std::uint8_t> rgba8;
};

struct ReferenceRenderResult {
  ReferenceRenderStatus status{ReferenceRenderStatus::invalid_request};
  std::string diagnostic;
  ReferenceImage image;
  std::string sha256_hex;
  double shader_compile_milliseconds{};
  double render_milliseconds{};
  double elapsed_milliseconds{};
};

[[nodiscard]] std::string_view ReferenceSceneName(ReferenceScene scene) noexcept;

[[nodiscard]] ReferenceRenderResult RenderWarpReference(
    ReferenceScene scene,
    const std::filesystem::path& shader_path,
    std::uint32_t width,
    std::uint32_t height) noexcept;

[[nodiscard]] ReferenceRenderResult RenderWarpSkyFieldScalars(
    ReferenceScene scene,
    const std::filesystem::path& shader_path,
    std::uint32_t width,
    std::uint32_t height) noexcept;

[[nodiscard]] ReferenceRenderResult RenderWarpSkyFieldScalars(
    ReferenceScene scene,
    const std::filesystem::path& shader_path,
    std::uint32_t width,
    std::uint32_t height,
    AuroraQuality quality) noexcept;

[[nodiscard]] ReferenceRenderResult RenderWarpSkyFieldRadiance(
    ReferenceScene scene,
    const std::filesystem::path& shader_path,
    std::uint32_t width,
    std::uint32_t height) noexcept;

[[nodiscard]] ReferenceRenderResult RenderWarpSkyFieldRadiance(
    ReferenceScene scene,
    const std::filesystem::path& shader_path,
    std::uint32_t width,
    std::uint32_t height,
    AuroraQuality quality) noexcept;

[[nodiscard]] ReferenceRenderResult RenderWarpCloudVolumeScalars(
    ReferenceScene scene,
    const std::filesystem::path& shader_path,
    std::uint32_t width,
    std::uint32_t height) noexcept;

[[nodiscard]] ReferenceRenderResult RenderWarpCloudVolumeRadiance(
    ReferenceScene scene,
    const std::filesystem::path& shader_path,
    std::uint32_t width,
    std::uint32_t height) noexcept;

[[nodiscard]] bool WriteBinaryPpm(
    const ReferenceImage& image,
    const std::filesystem::path& output_path,
    std::string& diagnostic) noexcept;

}  // namespace truth::render
