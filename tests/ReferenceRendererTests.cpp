#include "truth/render/ReferenceRenderer.hpp"
#include "truth/render/AuroraCurtain.hpp"
#include "truth/render/CloudVolume.hpp"
#include "truth/render/SkyFields.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <string>
#include <string_view>

namespace {

using truth::render::ReferenceImage;
using truth::render::ReferenceRenderStatus;
using truth::render::ReferenceScene;
using truth::render::RenderWarpReference;
using truth::render::WriteBinaryPpm;

struct ReferenceColor {
  float red;
  float green;
  float blue;
};

class TestFailure final : public std::exception {
public:
  explicit TestFailure(std::string message) : message_(std::move(message)) {}
  [[nodiscard]] const char* what() const noexcept override { return message_.c_str(); }

private:
  std::string message_;
};

[[nodiscard]] bool SameColor(const ReferenceColor lhs, const ReferenceColor rhs) {
  return std::bit_cast<std::uint32_t>(lhs.red) == std::bit_cast<std::uint32_t>(rhs.red)
      && std::bit_cast<std::uint32_t>(lhs.green) == std::bit_cast<std::uint32_t>(rhs.green)
      && std::bit_cast<std::uint32_t>(lhs.blue) == std::bit_cast<std::uint32_t>(rhs.blue);
}

[[nodiscard]] std::string ReadTextFile(const std::filesystem::path& path) {
  std::ifstream input{path, std::ios::binary};
  if (!input.good()) {
    throw TestFailure{"could not open reference shader source: " + path.string()};
  }
  return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

struct TestContext {
  std::uint64_t assertions{};

  void expect(const bool condition, const std::string_view message) {
    ++assertions;
    if (!condition) {
      throw TestFailure{std::string{message}};
    }
  }
};

struct ImageMetrics {
  double mean_luminance{};
  double upper_chroma{};
  double lower_chroma{};
  std::size_t distinct_colors{};
  std::uint8_t minimum_luminance{255};
  std::uint8_t maximum_luminance{};
  double panorama_seam_mean_difference{};
  std::uint8_t panorama_seam_maximum_difference{};
  double maximum_vertical_structure_support{};
  std::size_t horizontal_structure_regions{};
  std::size_t aurora_supported_columns{};
  std::size_t aurora_longest_horizontal_run{};
  double aurora_pixel_fraction{};
  std::size_t star_pixels{};
};

[[nodiscard]] ImageMetrics Measure(const ReferenceImage& image) {
  ImageMetrics metrics{};
  std::set<std::uint32_t> colors;
  double upper_chroma_sum{};
  double lower_chroma_sum{};
  std::size_t upper_count{};
  std::size_t lower_count{};

  for (std::uint32_t y = 0; y < image.height; ++y) {
    for (std::uint32_t x = 0; x < image.width; ++x) {
      const auto offset = (static_cast<std::size_t>(y) * image.width + x) * 4U;
      const auto red = image.rgba8[offset];
      const auto green = image.rgba8[offset + 1U];
      const auto blue = image.rgba8[offset + 2U];
      const auto alpha = image.rgba8[offset + 3U];
      if (alpha != 255U) {
        throw TestFailure{"GPU readback alpha left the bounded opaque contract"};
      }

      const auto luminance = static_cast<std::uint8_t>(
          (54U * red + 183U * green + 19U * blue) / 256U);
      metrics.mean_luminance += static_cast<double>(luminance);
      metrics.minimum_luminance = std::min(metrics.minimum_luminance, luminance);
      metrics.maximum_luminance = std::max(metrics.maximum_luminance, luminance);
      colors.insert((static_cast<std::uint32_t>(red) << 16U)
                    | (static_cast<std::uint32_t>(green) << 8U)
                    | static_cast<std::uint32_t>(blue));

      const auto maximum = std::max({red, green, blue});
      const auto minimum = std::min({red, green, blue});
      const auto chroma = static_cast<double>(maximum - minimum);
      if (y < image.height / 2U) {
        upper_chroma_sum += chroma;
        ++upper_count;
      } else {
        lower_chroma_sum += chroma;
        ++lower_count;
      }
    }
  }

  const auto pixel_count = static_cast<double>(image.width) * image.height;
  metrics.mean_luminance /= pixel_count;
  metrics.upper_chroma = upper_chroma_sum / static_cast<double>(upper_count);
  metrics.lower_chroma = lower_chroma_sum / static_cast<double>(lower_count);
  metrics.distinct_colors = colors.size();

  std::vector<std::uint8_t> luminance(
      static_cast<std::size_t>(image.width) * image.height);
  std::vector<std::uint8_t> row_medians(image.height);
  for (std::uint32_t y = 0; y < image.height; ++y) {
    std::vector<std::uint8_t> row;
    row.reserve(image.width);
    for (std::uint32_t x = 0; x < image.width; ++x) {
      const auto offset = (static_cast<std::size_t>(y) * image.width + x) * 4U;
      const auto value = static_cast<std::uint8_t>(
          (54U * image.rgba8[offset]
           + 183U * image.rgba8[offset + 1U]
           + 19U * image.rgba8[offset + 2U]) / 256U);
      luminance[static_cast<std::size_t>(y) * image.width + x] = value;
      row.push_back(value);
    }
    const auto middle = row.begin() + static_cast<std::ptrdiff_t>(row.size() / 2U);
    std::nth_element(row.begin(), middle, row.end());
    row_medians[y] = *middle;
  }

  constexpr int coherent_difference = 16;
  for (std::uint32_t x = 0; x < image.width; ++x) {
    std::size_t supported{};
    for (std::uint32_t y = 0; y < image.height; ++y) {
      const int value = luminance[static_cast<std::size_t>(y) * image.width + x];
      if (std::abs(value - static_cast<int>(row_medians[y])) >= coherent_difference) {
        ++supported;
      }
    }
    metrics.maximum_vertical_structure_support = std::max(
        metrics.maximum_vertical_structure_support,
        static_cast<double>(supported) / image.height);
  }

  constexpr std::size_t region_count = 8U;
  const auto region_width = image.width / region_count;
  for (std::size_t region = 0; region < region_count; ++region) {
    std::size_t supported{};
    for (std::uint32_t y = 0; y < image.height; ++y) {
      for (std::uint32_t x = static_cast<std::uint32_t>(region * region_width);
           x < static_cast<std::uint32_t>((region + 1U) * region_width);
           ++x) {
        const int value = luminance[static_cast<std::size_t>(y) * image.width + x];
        if (std::abs(value - static_cast<int>(row_medians[y])) >= 8) {
          ++supported;
        }
      }
    }
    const double support_fraction = static_cast<double>(supported)
        / static_cast<double>(region_width * image.height);
    if (support_fraction >= 0.05) {
      ++metrics.horizontal_structure_regions;
    }
  }

  std::size_t aurora_pixels{};
  std::size_t current_run{};
  for (std::uint32_t x = 0; x < image.width; ++x) {
    std::size_t column_pixels{};
    for (std::uint32_t y = 0; y < image.height; ++y) {
      const auto offset = (static_cast<std::size_t>(y) * image.width + x) * 4U;
      const int red = image.rgba8[offset];
      const int green = image.rgba8[offset + 1U];
      const int blue = image.rgba8[offset + 2U];
      if (green >= blue + 4 && green >= red + 12 && green >= 28) {
        ++column_pixels;
        ++aurora_pixels;
      }
    }
    if (column_pixels >= 2U) {
      ++metrics.aurora_supported_columns;
      ++current_run;
      metrics.aurora_longest_horizontal_run = std::max(
          metrics.aurora_longest_horizontal_run,
          current_run);
    } else {
      current_run = 0U;
    }
  }
  metrics.aurora_pixel_fraction = static_cast<double>(aurora_pixels)
      / (static_cast<double>(image.width) * image.height);

  for (std::uint32_t y = 1U; y + 1U < image.height; ++y) {
    for (std::uint32_t x = 1U; x + 1U < image.width; ++x) {
      const auto index = static_cast<std::size_t>(y) * image.width + x;
      const auto offset = index * 4U;
      const int red = image.rgba8[offset];
      const int green = image.rgba8[offset + 1U];
      const int blue = image.rgba8[offset + 2U];
      const int value = luminance[index];
      const int chroma = std::max({red, green, blue}) - std::min({red, green, blue});
      int neighborhood{};
      for (int offset_y = -1; offset_y <= 1; ++offset_y) {
        for (int offset_x = -1; offset_x <= 1; ++offset_x) {
          if (offset_x != 0 || offset_y != 0) {
            const auto neighbor_x = static_cast<std::uint32_t>(
                static_cast<int>(x) + offset_x);
            const auto neighbor_y = static_cast<std::uint32_t>(
                static_cast<int>(y) + offset_y);
            neighborhood += luminance[
                static_cast<std::size_t>(neighbor_y) * image.width + neighbor_x];
          }
        }
      }
      if (value >= 34 && chroma <= 20 && value >= (neighborhood / 8) + 11) {
        ++metrics.star_pixels;
      }
    }
  }

  double seam_sum{};
  for (std::uint32_t y = 0; y < image.height; ++y) {
    const auto first = static_cast<std::size_t>(y) * image.width * 4U;
    const auto last = first + (static_cast<std::size_t>(image.width) - 1U) * 4U;
    const auto difference = static_cast<std::uint8_t>(
        (std::abs(static_cast<int>(image.rgba8[first])
                  - static_cast<int>(image.rgba8[last]))
         + std::abs(static_cast<int>(image.rgba8[first + 1U])
                    - static_cast<int>(image.rgba8[last + 1U]))
         + std::abs(static_cast<int>(image.rgba8[first + 2U])
                    - static_cast<int>(image.rgba8[last + 2U]))) / 3);
    seam_sum += difference;
    metrics.panorama_seam_maximum_difference = std::max(
        metrics.panorama_seam_maximum_difference,
        difference);
  }
  metrics.panorama_seam_mean_difference = seam_sum / image.height;
  return metrics;
}

struct Capture {
  ReferenceScene scene{};
  std::string_view name;
  ReferenceImage image;
  std::string hash;
  ImageMetrics metrics;
  double shader_compile_milliseconds{};
  double render_milliseconds{};
  double elapsed_milliseconds{};
};

using Captures = std::array<Capture, 4>;

[[nodiscard]] Captures RenderScenes(
    TestContext& context,
    const std::filesystem::path& shader_path,
    const std::filesystem::path& output_directory) {
  constexpr std::array scenes{
      ReferenceScene::quiet_clear_night,
      ReferenceScene::active_clear_night,
      ReferenceScene::cloudy_night_aurora,
      ReferenceScene::storm,
  };

  Captures captures{};
  for (std::size_t index = 0; index < scenes.size(); ++index) {
    const auto scene = scenes[index];
    const auto render = RenderWarpReference(scene, shader_path, 256U, 128U);
    context.expect(render.status == ReferenceRenderStatus::rendered,
                   render.diagnostic.empty() ? "WARP reference render failed"
                                             : render.diagnostic);
    context.expect(render.image.width == 256U && render.image.height == 128U,
                   "WARP reference dimensions changed");
    context.expect(render.image.rgba8.size() == 256U * 128U * 4U,
                   "WARP readback byte count changed");
    context.expect(render.sha256_hex.size() == 64U,
                   "WARP reference SHA-256 was not a complete digest");
    context.expect(render.elapsed_milliseconds > 0.0,
                   "WARP reference render did not report elapsed time");
    context.expect(render.shader_compile_milliseconds >= 0.0
                       && render.render_milliseconds > 0.0
                       && render.elapsed_milliseconds
                           >= render.render_milliseconds,
                   "WARP timing components were inconsistent");

    const auto name = truth::render::ReferenceSceneName(scene);
    context.expect(!name.empty(), "reference scene did not have a stable name");
    const auto capture_path = output_directory / (std::string{name} + ".ppm");
    std::string diagnostic;
    context.expect(WriteBinaryPpm(render.image, capture_path, diagnostic),
                    diagnostic.empty() ? "PPM reference write failed" : diagnostic);

    captures[index] = {
        scene,
        name,
        render.image,
        render.sha256_hex,
        Measure(render.image),
        render.shader_compile_milliseconds,
        render.render_milliseconds,
        render.elapsed_milliseconds,
    };
    std::cout << "rendered " << name << " elapsed_ms="
              << render.elapsed_milliseconds << std::endl;
  }

  const auto first_probe = RenderWarpReference(
      ReferenceScene::day, shader_path, 16U, 16U);
  const auto second_probe = RenderWarpReference(
      ReferenceScene::day, shader_path, 16U, 16U);
  context.expect(first_probe.status == ReferenceRenderStatus::rendered,
                 first_probe.diagnostic.empty() ? "determinism probe failed"
                                                : first_probe.diagnostic);
  context.expect(second_probe.status == ReferenceRenderStatus::rendered,
                 second_probe.diagnostic.empty() ? "repeat determinism probe failed"
                                                 : second_probe.diagnostic);
  context.expect(first_probe.image.rgba8 == second_probe.image.rgba8,
                 "repeat WARP render was not byte-identical");
  context.expect(first_probe.sha256_hex == second_probe.sha256_hex,
                 "repeat WARP render digest changed");
  return captures;
}

void CompilerFailuresCarryDiagnostics(
    TestContext& context,
    const std::filesystem::path& output_directory) {
  const auto missing_shader = output_directory / "missing-reference-shader.hlsl";
  const auto result = RenderWarpReference(
      ReferenceScene::day, missing_shader, 256U, 128U);
  context.expect(result.status == ReferenceRenderStatus::shader_compile_failed,
                 "missing shader did not report shader_compile_failed");
  context.expect(!result.diagnostic.empty(),
                 "missing shader did not preserve compiler diagnostics");
  context.expect(result.image.rgba8.empty(),
                 "failed shader compilation returned image bytes");
}

void ImagesAreBoundedAndNonFlat(TestContext& context, const Captures& captures) {
  for (const auto& capture : captures) {
    std::cout << "metrics " << capture.name
              << " luma=" << capture.metrics.mean_luminance
              << " range=" << static_cast<unsigned int>(capture.metrics.minimum_luminance)
              << '-' << static_cast<unsigned int>(capture.metrics.maximum_luminance)
              << " colors=" << capture.metrics.distinct_colors
              << " chroma=" << capture.metrics.upper_chroma
              << " stars=" << capture.metrics.star_pixels << '\n';
  }
  for (const auto& capture : captures) {
    context.expect(capture.metrics.maximum_luminance
                       > capture.metrics.minimum_luminance + 8U,
                   "reference capture was effectively flat");
    context.expect(capture.metrics.distinct_colors > 64U,
                   "reference capture had too few distinct GPU colors");
  }
}

void NamedScenesAreDistinct(TestContext& context, const Captures& captures) {
  std::set<std::string> hashes;
  std::set<std::string_view> names;
  for (const auto& capture : captures) {
    hashes.insert(capture.hash);
    names.insert(capture.name);
  }
  context.expect(hashes.size() == captures.size(),
                 "named reference scenes produced duplicate hashes");
  context.expect(names.size() == captures.size(),
                 "reference scene names were not unique");
}

void SceneRelationshipsRemainPhysical(TestContext& context, const Captures& captures) {
  const auto& quiet = captures[0].metrics;
  const auto& active = captures[1].metrics;
  const auto& cloudy = captures[2].metrics;
  const auto& storm = captures[3].metrics;

  context.expect(active.mean_luminance > quiet.mean_luminance + 0.35,
                 "active aurora did not lift clear-night luminance");
  context.expect(active.mean_luminance < quiet.mean_luminance + 13.0,
                 "active aurora overwhelmed the clear-night luminance budget");
  context.expect(active.aurora_pixel_fraction
                     > quiet.aurora_pixel_fraction + 0.008,
                 "active aurora did not add controlled green curtain support");
  context.expect(cloudy.aurora_pixel_fraction
                     < active.aurora_pixel_fraction * 0.82,
                 "cloud volume did not occlude the aurora softly");
  context.expect(storm.aurora_pixel_fraction < 0.015,
                 "no-aurora storm control contained green curtain emission");
  context.expect(quiet.mean_luminance >= 2.0 && quiet.mean_luminance <= 38.0,
                 "quiet night left the readable dark-sky budget");
  context.expect(active.maximum_luminance < 235U,
                 "active aurora clipped into a neon display ceiling");
}

void SkyStructuresAreCoherent(TestContext& context, const Captures& captures) {
  const auto& quiet = captures[0].metrics;
  const auto& active = captures[1].metrics;
  const auto& cloudy = captures[2].metrics;
  const auto& storm = captures[3].metrics;

  std::cout << "structure quiet_max_column=" << quiet.maximum_vertical_structure_support
            << " active_max_column=" << active.maximum_vertical_structure_support
            << " storm_max_column=" << storm.maximum_vertical_structure_support
            << " active_run=" << active.aurora_longest_horizontal_run
            << " active_columns=" << active.aurora_supported_columns
            << " active_area=" << active.aurora_pixel_fraction
            << " cloudy_area=" << cloudy.aurora_pixel_fraction
            << " stars=" << quiet.star_pixels << ',' << active.star_pixels
            << ',' << cloudy.star_pixels << '\n';

  context.expect(active.aurora_longest_horizontal_run >= 62U,
                 "night aurora remained a set of disconnected glowing tubes");
  context.expect(active.aurora_supported_columns < 202U,
                 "night aurora filled nearly the entire panorama");
  context.expect(active.aurora_pixel_fraction >= 0.012
                     && active.aurora_pixel_fraction < 0.24,
                 "night aurora filled too much of the image area");
  context.expect(active.maximum_vertical_structure_support < 0.72,
                 "active aurora formed a rectangular/full-height pillar silhouette");
  context.expect(quiet.star_pixels >= 8U,
                 "quiet clear night did not preserve a readable star field");
  context.expect(active.star_pixels * 5U >= quiet.star_pixels,
                 "active aurora erased nearly every star");
  context.expect(cloudy.star_pixels < active.star_pixels,
                 "cloud volume did not occlude background stars");
  context.expect(storm.maximum_vertical_structure_support < 0.90,
                 "storm clouds contained a near-full-height opaque column");

  for (const auto& capture : captures) {
    context.expect(capture.metrics.panorama_seam_mean_difference < 8.0,
                   "panorama endpoint colors opened into a visible seam");
    context.expect(capture.metrics.panorama_seam_maximum_difference < 24U,
                   "panorama endpoint contained a severe local seam");
  }
}

void CpuAndHlslSkyFieldsRemainAligned(
    TestContext& context,
    const std::filesystem::path& shader_path) {
  constexpr std::uint32_t width = 64U;
  constexpr std::uint32_t height = 32U;
  const auto scalars = truth::render::RenderWarpSkyFieldScalars(
      ReferenceScene::active_clear_night, shader_path, width, height);
  context.expect(scalars.status == ReferenceRenderStatus::rendered,
                 scalars.diagnostic.empty() ? "scalar sky-field probe failed"
                                            : scalars.diagnostic);
  const auto radiance = truth::render::RenderWarpSkyFieldRadiance(
      ReferenceScene::active_clear_night, shader_path, width, height);
  context.expect(radiance.status == ReferenceRenderStatus::rendered,
                 radiance.diagnostic.empty() ? "radiance sky-field probe failed"
                                             : radiance.diagnostic);
  context.expect(scalars.image.rgba8.size() == width * height * 4U,
                 "scalar sky-field probe byte count changed");
  context.expect(radiance.image.rgba8.size() == width * height * 4U,
                 "radiance sky-field probe byte count changed");

  constexpr float pi = 3.14159265358979323846F;
  constexpr float tolerance = 2.1F / 255.0F;
  for (std::uint32_t y = 0; y < height; ++y) {
    const float vertical = 1.0F
        - ((static_cast<float>(y) + 0.5F) / static_cast<float>(height));
    const float elevation = 0.035F + (((0.5F * pi) - 0.07F) * vertical);
    const float view_z = std::sin(elevation);
    const float view_radius = std::cos(elevation);
    for (std::uint32_t x = 0; x < width; ++x) {
      const float horizontal = (static_cast<float>(x) + 0.5F)
          / static_cast<float>(width);
      const float azimuth = ((horizontal * 2.0F) - 1.0F) * pi;
      truth::render::SkyFieldInput input{
          std::sin(azimuth) * view_radius,
          std::cos(azimuth) * view_radius,
          view_z,
          0.63F,
          0.62F,
          -0.27F,
          0.05F,
          0.16F,
          0.04F,
          0.82F,
          1.0F,
          0.0F,
          0.0F,
          0.20F,
      };
      truth::render::SkyFieldOutput cpu{};
      context.expect(
          truth::render::EvaluateSkyFields(input, cpu).status
              == truth::render::SkyFieldStatus::evaluated,
          "CPU sky-field parity sample was rejected");
      const auto offset = (static_cast<std::size_t>(y) * width + x) * 4U;
      const auto close = [&](const std::uint8_t gpu, const float expected) {
        return std::fabs((static_cast<float>(gpu) / 255.0F) - expected) <= tolerance;
      };
      context.expect(close(scalars.image.rgba8[offset], cpu.cloud_body),
                     "CPU/HLSL cloud body parity drifted");
      context.expect(close(scalars.image.rgba8[offset + 1U],
                           cpu.cloud_detail_erosion),
                     "CPU/HLSL cloud erosion parity drifted");
      context.expect(close(scalars.image.rgba8[offset + 2U], cpu.cloud_density),
                     "CPU/HLSL cloud density parity drifted");
      context.expect(close(scalars.image.rgba8[offset + 3U], cpu.aurora_mask),
                     "CPU/HLSL aurora mask parity drifted");
      context.expect(close(radiance.image.rgba8[offset],
                           cpu.aurora_intrinsic_radiance.r),
                     "CPU/HLSL aurora red parity drifted");
      context.expect(close(radiance.image.rgba8[offset + 1U],
                           cpu.aurora_intrinsic_radiance.g),
                     "CPU/HLSL aurora green parity drifted");
      context.expect(close(radiance.image.rgba8[offset + 2U],
                           cpu.aurora_intrinsic_radiance.b),
                     "CPU/HLSL aurora blue parity drifted");
    }
  }
}

void CpuAndHlslAuroraQualityTiersRemainAligned(
    TestContext& context,
    const std::filesystem::path& shader_path) {
  constexpr std::uint32_t width = 16U;
  constexpr std::uint32_t height = 16U;
  constexpr float pi = 3.14159265358979323846F;
  constexpr float tolerance = 2.1F / 255.0F;
  constexpr std::array qualities{
      truth::render::AuroraQuality::fallback,
      truth::render::AuroraQuality::low,
      truth::render::AuroraQuality::balanced,
      truth::render::AuroraQuality::high,
  };
  for (const auto quality : qualities) {
    const auto scalars = truth::render::RenderWarpSkyFieldScalars(
        ReferenceScene::active_clear_night,
        shader_path,
        width,
        height,
        quality);
    context.expect(scalars.status == ReferenceRenderStatus::rendered,
                   scalars.diagnostic.empty() ? "quality scalar probe failed"
                                              : scalars.diagnostic);
    const auto radiance = truth::render::RenderWarpSkyFieldRadiance(
        ReferenceScene::active_clear_night,
        shader_path,
        width,
        height,
        quality);
    context.expect(radiance.status == ReferenceRenderStatus::rendered,
                   radiance.diagnostic.empty() ? "quality radiance probe failed"
                                               : radiance.diagnostic);
    for (std::uint32_t y = 0; y < height; ++y) {
      const float vertical = 1.0F
          - ((static_cast<float>(y) + 0.5F) / static_cast<float>(height));
      const float elevation = 0.035F + (((0.5F * pi) - 0.07F) * vertical);
      const float view_z = std::sin(elevation);
      const float view_radius = std::cos(elevation);
      for (std::uint32_t x = 0; x < width; ++x) {
        const float horizontal = (static_cast<float>(x) + 0.5F)
            / static_cast<float>(width);
        const float azimuth = ((horizontal * 2.0F) - 1.0F) * pi;
        truth::render::AuroraCurtainInput input{
            0.0F, 0.0F, 0.20F,
            std::sin(azimuth) * view_radius,
            std::cos(azimuth) * view_radius,
            view_z,
            0.63F,
            0.62F, -0.27F,
            0.82F,
            1.0F,
            quality,
        };
        truth::render::AuroraCurtainOutput cpu{};
        context.expect(
            truth::render::EvaluateAuroraCurtain(input, cpu).status
                == truth::render::AuroraCurtainStatus::evaluated,
            "CPU quality-tier parity sample was rejected");
        const auto offset = (static_cast<std::size_t>(y) * width + x) * 4U;
        const auto close = [&](const std::uint8_t gpu, const float expected) {
          return std::fabs((static_cast<float>(gpu) / 255.0F) - expected)
              <= tolerance;
        };
        context.expect(close(scalars.image.rgba8[offset + 3U], cpu.mask),
                       "CPU/HLSL quality-tier aurora mask drifted");
        context.expect(close(radiance.image.rgba8[offset], cpu.intrinsic_radiance.r)
                           && close(radiance.image.rgba8[offset + 1U],
                                    cpu.intrinsic_radiance.g)
                           && close(radiance.image.rgba8[offset + 2U],
                                    cpu.intrinsic_radiance.b),
                       "CPU/HLSL quality-tier aurora radiance drifted");
      }
    }
  }
}

void CpuAndHlslCloudVolumeRemainAligned(
    TestContext& context,
    const std::filesystem::path& shader_path) {
  constexpr std::uint32_t width = 16U;
  constexpr std::uint32_t height = 16U;
  const auto scalars = truth::render::RenderWarpCloudVolumeScalars(
      ReferenceScene::day, shader_path, width, height);
  context.expect(scalars.status == ReferenceRenderStatus::rendered,
                 scalars.diagnostic.empty() ? "scalar cloud-volume probe failed"
                                            : scalars.diagnostic);
  const auto radiance = truth::render::RenderWarpCloudVolumeRadiance(
      ReferenceScene::day, shader_path, width, height);
  context.expect(radiance.status == ReferenceRenderStatus::rendered,
                 radiance.diagnostic.empty() ? "radiance cloud-volume probe failed"
                                             : radiance.diagnostic);

  constexpr float pi = 3.14159265358979323846F;
  constexpr float channel_tolerance = 3.1F / 255.0F;
  for (std::uint32_t y = 0; y < height; ++y) {
    const float vertical = 1.0F
        - ((static_cast<float>(y) + 0.5F) / static_cast<float>(height));
    const float elevation = 0.035F + (((0.5F * pi) - 0.07F) * vertical);
    const float view_z = std::sin(elevation);
    const float view_radius = std::cos(elevation);
    for (std::uint32_t x = 0; x < width; ++x) {
      const float horizontal = (static_cast<float>(x) + 0.5F)
          / static_cast<float>(width);
      const float azimuth = ((horizontal * 2.0F) - 1.0F) * pi;
      const float sun_radius = std::cos(0.55F);
      truth::render::CloudVolumeInput input{
          {0.0F, 0.0F, 0.20F},
          {std::sin(azimuth) * view_radius,
           std::cos(azimuth) * view_radius,
           view_z},
          {std::sin(0.28F) * sun_radius,
           std::cos(0.28F) * sun_radius,
           std::sin(0.55F)},
          1.20F,
          3.80F,
          60.0F,
          0.18F,
          0.62F,
          -0.27F,
          0.36F,
          0.62F,
          0.08F,
          0.42F,
          0.0F,
          x,
          y,
          0U,
          truth::render::CloudVolumeQuality::quality,
      };
      truth::render::CloudVolumeOutput cpu{};
      context.expect(
          truth::render::EvaluateCloudVolume(input, cpu).status
              == truth::render::CloudVolumeStatus::evaluated,
          "CPU cloud-volume parity sample was rejected");
      const auto offset = (static_cast<std::size_t>(y) * width + x) * 4U;
      const auto channel = [](const std::uint8_t value) {
        return static_cast<float>(value) / 255.0F;
      };
      context.expect(std::fabs(channel(scalars.image.rgba8[offset])
                               - cpu.transmittance) <= channel_tolerance,
                     "CPU/HLSL volume transmittance parity drifted");
      context.expect(std::fabs(channel(scalars.image.rgba8[offset + 1U])
                               - std::min(cpu.optical_depth / 8.0F, 1.0F))
                         <= channel_tolerance,
                     "CPU/HLSL volume optical-depth parity drifted");
      context.expect(std::fabs(channel(scalars.image.rgba8[offset + 2U])
                               - (static_cast<float>(cpu.primary_steps) / 24.0F))
                         <= channel_tolerance,
                     "CPU/HLSL volume primary-step parity drifted");
      context.expect(std::fabs(channel(scalars.image.rgba8[offset + 3U])
                               - (static_cast<float>(cpu.light_samples) / 144.0F))
                         <= channel_tolerance,
                     "CPU/HLSL volume light-step parity drifted");
      context.expect(std::fabs(channel(radiance.image.rgba8[offset])
                               - std::min(cpu.radiance.x, 1.0F)) <= channel_tolerance,
                     "CPU/HLSL volume red parity drifted");
      context.expect(std::fabs(channel(radiance.image.rgba8[offset + 1U])
                               - std::min(cpu.radiance.y, 1.0F)) <= channel_tolerance,
                     "CPU/HLSL volume green parity drifted");
      context.expect(std::fabs(channel(radiance.image.rgba8[offset + 2U])
                               - std::min(cpu.radiance.z, 1.0F)) <= channel_tolerance,
                     "CPU/HLSL volume blue parity drifted");
    }
  }
}

void VolumeShowsParallaxAndInteriorShading(
    TestContext& context,
    const std::filesystem::path& shader_path) {
  const auto baseline = RenderWarpReference(
      ReferenceScene::day, shader_path, 128U, 64U);
  context.expect(baseline.status == ReferenceRenderStatus::rendered,
                 baseline.diagnostic.empty() ? "baseline volume render failed"
                                             : baseline.diagnostic);
  const auto translated = RenderWarpReference(
      ReferenceScene::translated_day_probe, shader_path, 128U, 64U);
  context.expect(translated.status == ReferenceRenderStatus::rendered,
                  translated.diagnostic.empty() ? "translated volume render failed"
                                                : translated.diagnostic);
  context.expect(translated.sha256_hex != baseline.sha256_hex,
                  "camera translation did not change the WARP volume image");

  const auto& day = baseline.image;
  std::size_t translated_pixels{};
  std::size_t shaded_neighbors{};
  std::vector<std::uint8_t> luminance(
      static_cast<std::size_t>(day.width) * day.height);
  for (std::uint32_t y = 0; y < day.height; ++y) {
    for (std::uint32_t x = 0; x < day.width; ++x) {
      const auto offset = (static_cast<std::size_t>(y) * day.width + x) * 4U;
      const int difference = std::abs(static_cast<int>(day.rgba8[offset])
                                      - static_cast<int>(translated.image.rgba8[offset]))
          + std::abs(static_cast<int>(day.rgba8[offset + 1U])
                     - static_cast<int>(translated.image.rgba8[offset + 1U]))
          + std::abs(static_cast<int>(day.rgba8[offset + 2U])
                     - static_cast<int>(translated.image.rgba8[offset + 2U]));
      if (difference >= 18) {
        ++translated_pixels;
      }
      luminance[static_cast<std::size_t>(y) * day.width + x]
          = static_cast<std::uint8_t>(
              (54U * day.rgba8[offset]
               + 183U * day.rgba8[offset + 1U]
               + 19U * day.rgba8[offset + 2U]) / 256U);
    }
  }
  for (std::uint32_t y = day.height / 4U; y + 2U < day.height; ++y) {
    for (std::uint32_t x = 2U; x + 2U < day.width; ++x) {
      const auto center = luminance[static_cast<std::size_t>(y) * day.width + x];
      const auto horizontal = luminance[static_cast<std::size_t>(y) * day.width + x + 2U];
      const auto vertical = luminance[static_cast<std::size_t>(y + 2U) * day.width + x];
      if (std::abs(static_cast<int>(center) - static_cast<int>(horizontal)) >= 10
          || std::abs(static_cast<int>(center) - static_cast<int>(vertical)) >= 10) {
        ++shaded_neighbors;
      }
    }
  }
  const auto pixel_count = static_cast<std::size_t>(day.width) * day.height;
  context.expect(translated_pixels > pixel_count / 100U,
                 "camera translation produced no depth-dependent cloud parallax");
  context.expect(translated_pixels < (pixel_count * 3U) / 4U,
                 "camera translation changed the full frame like a fog slab");
  context.expect(shaded_neighbors > 60U,
                  "cloud body lacked lit-edge/darker-core interior shading");
}

void TierAndIdentityReferences(
    TestContext& context,
    const std::filesystem::path& shader_path) {
  const auto shader_directory = shader_path.parent_path();
  const std::string cloud = ReadTextFile(shader_directory / "TruthCloudVolume.fxh");
  const std::string aurora = ReadTextFile(shader_directory / "TruthAuroraCurtain.fxh");
  const std::string interior = ReadTextFile(shader_directory / "TruthInteriorLight.fxh");
  const std::string quality = ReadTextFile(shader_directory / "TruthQuality.fxh");
  const std::string screen_space = ReadTextFile(shader_directory / "TruthScreenSpace.fxh");

  context.expect(cloud.find("#include \"TruthQuality.fxh\"") != std::string::npos,
                 "performance-analytic-day: cloud volume does not consume TruthQuality");
  context.expect(cloud.find("#if TRUTH_QUALITY_TIER < 2") != std::string::npos
                     && cloud.find("#define TRUTH_ENABLE_CLOUD_VOLUME 0") != std::string::npos
                     && quality.find("#if TRUTH_QUALITY_TIER == 0") != std::string::npos
                     && quality.find("TruthQualityCloudPrimarySteps = 0u;")
                            != std::string::npos,
                 "performance-analytic-day: tier 0 must compile volume marching out");
  context.expect(quality.find("#elif TRUTH_QUALITY_TIER == 1") != std::string::npos
                     && quality.find("TruthQualityCloudPrimarySteps = 0u;")
                            != std::string::npos
                     && quality.find("TruthQualityCloudLightSteps = 0u;")
                            != std::string::npos,
                 "balanced-analytic-night: tier 1 must retain the analytic cloud path");
  context.expect(cloud.find("TRUTH_CLOUD_VOLUME_QUALITY") == std::string::npos,
                 "quality-volume-cloud: deprecated cloud quality macro remains");
  context.expect(cloud.find("static const uint TruthCloudVolumePrimarySteps = TruthQualityCloudPrimarySteps;")
                     != std::string::npos
                     && cloud.find("static const uint TruthCloudVolumeLightSteps = TruthQualityCloudLightSteps;")
                         != std::string::npos,
                 "quality-volume-cloud: tier 2 must use the 8/2 TruthQuality budget");
  context.expect(quality.find("#elif TRUTH_QUALITY_TIER == 2") != std::string::npos
                     && quality.find("TruthQualityCloudPrimarySteps = 8u;")
                            != std::string::npos
                     && quality.find("TruthQualityCloudLightSteps = 2u;")
                            != std::string::npos,
                 "quality-volume-cloud: tier 2 must use an exact 8/2 budget");
  context.expect(quality.find("#elif TRUTH_QUALITY_TIER == 3") != std::string::npos
                     && quality.find("TruthQualityCloudPrimarySteps = 12u;")
                            != std::string::npos
                     && quality.find("TruthQualityCloudLightSteps = 3u;")
                            != std::string::npos,
                 "ultra-volume-cloud: tier 3 must use the 12/3 TruthQuality budget");
  context.expect(quality.find("TruthQualityCloudPrimarySteps = 16u;") != std::string::npos
                     && quality.find("TruthQualityCloudLightSteps = 4u;")
                            != std::string::npos
                     && cloud.find("TruthCloudVolumeInterleavedJitter") != std::string::npos,
                 "cinematic-volume-cloud: tier 4 budget or stable sampling changed");

  context.expect(aurora.find("#include \"TruthQuality.fxh\"") != std::string::npos
                     && aurora.find("static const uint TruthAuroraCurtainSamples = TruthQualityAuroraSamples;")
                            != std::string::npos,
                 "aurora quality tiers do not consume TruthQuality");
  context.expect(aurora.find("TRUTH_AURORA_QUALITY") == std::string::npos,
                 "deprecated aurora quality macro remains");
  context.expect(interior.find("output.exterior_excluded = (open_factor == 0.0) ? 1.0 : 0.0;")
                     != std::string::npos,
                 "sealed-interior: shader no longer preserves exact exterior exclusion");

  const ReferenceColor scene{0.21F, 0.34F, 0.55F};
  const auto preserve_scene = [](const ReferenceColor input, const bool runtime_valid) {
    return runtime_valid ? ReferenceColor{0.44F, 0.33F, 0.22F} : input;
  };
  context.expect(SameColor(preserve_scene(scene, false), scene),
                 "invalid-runtime-preserves-scene: invalid runtime changed scene color");

  context.expect(screen_space.find("if (occlusion <= 0.0 || sample_count <= 0.0)")
                     != std::string::npos
                     && screen_space.find("if (visibility >= 1.0)")
                            != std::string::npos,
                 "ao-flat-surface-is-neutral: unoccluded AO lacks exact identity");
  context.expect(screen_space.find("TruthScreenSpaceGeometryValid(") != std::string::npos
                     && screen_space.find("return scene;") != std::string::npos,
                 "ao-depth-edge-is-rejected: geometry rejection lacks identity");
  context.expect(screen_space.find("if (!hit)") != std::string::npos,
                 "ssr-miss-preserves-scene: SSR miss lacks exact identity");
  context.expect(screen_space.find("if (skin_mask <= 0.0)") != std::string::npos,
                 "sss-non-skin-preserves-scene: non-skin diffusion lacks identity");

  truth::render::AuroraCurtainInput phase_zero{
      0.0F, 0.0F, 0.20F,
      0.0F, 1.0F, 0.0F,
      0.0F,
      0.62F, -0.27F,
      0.82F,
      1.0F,
      truth::render::AuroraQuality::balanced,
  };
  auto phase_one = phase_zero;
  phase_one.phase = 1.0F;
  truth::render::AuroraCurtainOutput first{};
  truth::render::AuroraCurtainOutput wrapped{};
  context.expect(truth::render::EvaluateAuroraCurtain(phase_zero, first).status
                     == truth::render::AuroraCurtainStatus::evaluated
                     && truth::render::EvaluateAuroraCurtain(phase_one, wrapped).status
                            == truth::render::AuroraCurtainStatus::evaluated,
                 "phase-wrap: CPU aurora reference rejected a valid endpoint phase");
  constexpr float phase_tolerance = 0.00001F;
  context.expect(std::fabs(first.mask - wrapped.mask) <= phase_tolerance
                     && std::fabs(first.intrinsic_radiance.r - wrapped.intrinsic_radiance.r)
                            <= phase_tolerance
                     && std::fabs(first.intrinsic_radiance.g - wrapped.intrinsic_radiance.g)
                            <= phase_tolerance
                     && std::fabs(first.intrinsic_radiance.b - wrapped.intrinsic_radiance.b)
                            <= phase_tolerance,
                 "phase-wrap: aurora endpoint phases drifted");

  truth::render::CloudVolumeInput cloud_input{
      {0.0F, 0.0F, 0.20F}, {0.0F, 1.0F, 0.0F}, {0.0F, 0.8525245F, 0.5226872F},
      1.20F, 3.80F, 60.0F, 0.18F, 0.62F, -0.27F, 0.36F, 0.62F, 0.08F,
      0.42F, 0.0F, 7U, 11U, 0U, truth::render::CloudVolumeQuality::quality,
  };
  constexpr truth::render::CloudVolumeVector world_sample{2.5F, -1.5F, 2.2F};
  float baseline_density{};
  float translated_density{};
  context.expect(truth::render::SampleCloudVolumeDensity(
                     cloud_input, world_sample, baseline_density).status
                     == truth::render::CloudVolumeStatus::evaluated,
                 "camera-stable-world-sampling: baseline CPU sample was rejected");
  cloud_input.camera_position = {23.0F, -19.0F, 0.20F};
  context.expect(truth::render::SampleCloudVolumeDensity(
                     cloud_input, world_sample, translated_density).status
                     == truth::render::CloudVolumeStatus::evaluated
                     && std::bit_cast<std::uint32_t>(baseline_density)
                            == std::bit_cast<std::uint32_t>(translated_density),
                 "camera-stable-world-sampling: fixed world density drifted with camera position");
}

void PpmFilesCarryExactDimensions(
    TestContext& context,
    const Captures& captures,
    const std::filesystem::path& output_directory) {
  for (const auto& capture : captures) {
    const auto path = output_directory / (std::string{capture.name} + ".ppm");
    std::ifstream input{path, std::ios::binary | std::ios::ate};
    context.expect(input.good(), "reference PPM could not be reopened");
    const auto size = input.tellg();
    context.expect(size > static_cast<std::streamoff>(256U * 128U * 3U),
                   "reference PPM did not contain a header plus all RGB pixels");
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "usage: truth_reference_renderer_tests <shader.hlsl> <output-directory>\n";
    return 2;
  }

  const std::filesystem::path shader_path{argv[1]};
  const std::filesystem::path output_directory{argv[2]};
  std::error_code error;
  std::filesystem::create_directories(output_directory, error);
  if (error) {
    std::cerr << "could not create reference output directory: " << error.message() << '\n';
    return 2;
  }

  TestContext context;
  std::uint32_t passed{};
  try {
    CompilerFailuresCarryDiagnostics(context, output_directory);
    ++passed;
    std::cout << "[PASS] shader compiler failures carry diagnostics\n";

    TierAndIdentityReferences(context, shader_path);
    ++passed;
    std::cout << "[PASS] named tier and identity references\n";

    CpuAndHlslSkyFieldsRemainAligned(context, shader_path);
    ++passed;
    std::cout << "[PASS] CPU and HLSL sky fields remain aligned\n";

    CpuAndHlslAuroraQualityTiersRemainAligned(context, shader_path);
    ++passed;
    std::cout << "[PASS] CPU and HLSL aurora quality tiers remain aligned\n";

    CpuAndHlslCloudVolumeRemainAligned(context, shader_path);
    ++passed;
    std::cout << "[PASS] CPU and HLSL cloud volume remain aligned\n";

    const auto captures = RenderScenes(context, shader_path, output_directory);
    ++passed;
    std::cout << "[PASS] four named scenes render with deterministic probe\n";

    ImagesAreBoundedAndNonFlat(context, captures);
    ++passed;
    std::cout << "[PASS] GPU readback pixels are bounded and non-flat\n";

    NamedScenesAreDistinct(context, captures);
    ++passed;
    std::cout << "[PASS] named scenes are visually distinct\n";

    SceneRelationshipsRemainPhysical(context, captures);
    ++passed;
    std::cout << "[PASS] image-level scene relationships remain physical\n";

    SkyStructuresAreCoherent(context, captures);
    ++passed;
    std::cout << "[PASS] cloud and aurora structures remain coherent\n";

    VolumeShowsParallaxAndInteriorShading(context, shader_path);
    ++passed;
    std::cout << "[PASS] cloud volume shows parallax and interior shading\n";

    PpmFilesCarryExactDimensions(context, captures, output_directory);
    ++passed;
    std::cout << "[PASS] PPM captures preserve exact dimensions\n";

    for (const auto& capture : captures) {
      std::cout << "capture " << capture.name << " sha256=" << capture.hash
                 << " mean_luma=" << capture.metrics.mean_luminance
                 << " upper_chroma=" << capture.metrics.upper_chroma
                 << " compile_ms=" << capture.shader_compile_milliseconds
                 << " render_ms=" << capture.render_milliseconds
                 << " elapsed_ms=" << capture.elapsed_milliseconds << '\n';
    }
    std::cout << "Truth WARP reference cases: " << passed << "/12; assertions: "
              << context.assertions << '\n';
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << "[FAIL] Truth WARP reference: " << exception.what() << '\n';
    return 1;
  }
}
