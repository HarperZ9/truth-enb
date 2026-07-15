#include "truth/render/ReferenceRenderer.hpp"
#include "truth/render/SkyFields.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <string_view>

namespace {

using truth::render::ReferenceImage;
using truth::render::ReferenceRenderStatus;
using truth::render::ReferenceScene;
using truth::render::RenderWarpReference;
using truth::render::WriteBinaryPpm;

class TestFailure final : public std::exception {
public:
  explicit TestFailure(std::string message) : message_(std::move(message)) {}
  [[nodiscard]] const char* what() const noexcept override { return message_.c_str(); }

private:
  std::string message_;
};

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

  constexpr int coherent_difference = 14;
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
      if (std::max(green, blue) >= red + 12
          && ((green + blue) / 2) >= red + 12) {
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
};

using Captures = std::array<Capture, 4>;

[[nodiscard]] Captures RenderScenes(
    TestContext& context,
    const std::filesystem::path& shader_path,
    const std::filesystem::path& output_directory) {
  constexpr std::array scenes{
      ReferenceScene::day,
      ReferenceScene::dusk,
      ReferenceScene::clear_night_aurora,
      ReferenceScene::storm,
  };

  Captures captures{};
  for (std::size_t index = 0; index < scenes.size(); ++index) {
    const auto scene = scenes[index];
    const auto first = RenderWarpReference(scene, shader_path, 256U, 128U);
    context.expect(first.status == ReferenceRenderStatus::rendered,
                   first.diagnostic.empty() ? "WARP reference render failed" : first.diagnostic);
    context.expect(first.image.width == 256U && first.image.height == 128U,
                   "WARP reference dimensions changed");
    context.expect(first.image.rgba8.size() == 256U * 128U * 4U,
                   "WARP readback byte count changed");
    context.expect(first.sha256_hex.size() == 64U,
                   "WARP reference SHA-256 was not a complete digest");

    const auto second = RenderWarpReference(scene, shader_path, 256U, 128U);
    context.expect(second.status == ReferenceRenderStatus::rendered,
                   second.diagnostic.empty() ? "repeat WARP reference render failed"
                                             : second.diagnostic);
    context.expect(first.image.rgba8 == second.image.rgba8,
                   "repeat WARP render was not byte-identical");
    context.expect(first.sha256_hex == second.sha256_hex,
                   "repeat WARP render digest changed");

    const auto name = truth::render::ReferenceSceneName(scene);
    context.expect(!name.empty(), "reference scene did not have a stable name");
    const auto capture_path = output_directory / (std::string{name} + ".ppm");
    std::string diagnostic;
    context.expect(WriteBinaryPpm(first.image, capture_path, diagnostic),
                   diagnostic.empty() ? "PPM reference write failed" : diagnostic);

    captures[index] = {scene, name, first.image, first.sha256_hex, Measure(first.image)};
  }
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
    context.expect(capture.metrics.maximum_luminance
                       > capture.metrics.minimum_luminance + 8U,
                   "reference capture was effectively flat");
    context.expect(capture.metrics.distinct_colors > 128U,
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
  const auto& day = captures[0].metrics;
  const auto& dusk = captures[1].metrics;
  const auto& night = captures[2].metrics;
  const auto& storm = captures[3].metrics;

  context.expect(day.mean_luminance > dusk.mean_luminance,
                 "day was not brighter than dusk");
  context.expect(dusk.mean_luminance > night.mean_luminance,
                 "dusk was not brighter than clear night");
  context.expect(day.mean_luminance > storm.mean_luminance,
                 "storm was not darker than day");
  context.expect(night.upper_chroma > night.lower_chroma + 3.0,
                 "night aurora did not add upper-sky chroma");
}

void SkyStructuresAreCoherent(TestContext& context, const Captures& captures) {
  const auto& day = captures[0].metrics;
  const auto& dusk = captures[1].metrics;
  const auto& night = captures[2].metrics;
  const auto& storm = captures[3].metrics;

  std::cout << "structure day_max_column=" << day.maximum_vertical_structure_support
            << " dusk_max_column=" << dusk.maximum_vertical_structure_support
            << " storm_max_column=" << storm.maximum_vertical_structure_support
            << " day_regions=" << day.horizontal_structure_regions
            << " storm_regions=" << storm.horizontal_structure_regions
            << " aurora_run=" << night.aurora_longest_horizontal_run
            << " aurora_columns=" << night.aurora_supported_columns
            << " aurora_area=" << night.aurora_pixel_fraction << '\n';

  context.expect(night.aurora_longest_horizontal_run >= 112U,
                 "night aurora remained a set of disconnected glowing tubes");
  context.expect(night.aurora_supported_columns < 232U,
                 "night aurora filled nearly the entire panorama");
  context.expect(night.aurora_pixel_fraction < 0.55,
                 "night aurora filled too much of the image area");

  context.expect(day.maximum_vertical_structure_support < 0.90,
                 "day clouds contained a near-full-height opaque column");
  context.expect(dusk.maximum_vertical_structure_support < 0.90,
                 "dusk clouds contained a near-full-height opaque column");
  context.expect(storm.maximum_vertical_structure_support < 0.90,
                 "storm clouds contained a near-full-height opaque column");
  context.expect(day.horizontal_structure_regions >= 6U,
                 "day cloud support was not distributed across the panorama");
  context.expect(storm.horizontal_structure_regions >= 4U,
                 "storm cloud support was not distributed across the panorama");

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
      ReferenceScene::clear_night_aurora, shader_path, width, height);
  context.expect(scalars.status == ReferenceRenderStatus::rendered,
                 scalars.diagnostic.empty() ? "scalar sky-field probe failed"
                                            : scalars.diagnostic);
  const auto radiance = truth::render::RenderWarpSkyFieldRadiance(
      ReferenceScene::clear_night_aurora, shader_path, width, height);
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
          0.17F,
          0.44F,
          0.04F,
          1.0F,
          1.0F,
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

    const auto captures = RenderScenes(context, shader_path, output_directory);
    ++passed;
    std::cout << "[PASS] four named scenes render twice deterministically\n";

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

    CpuAndHlslSkyFieldsRemainAligned(context, shader_path);
    ++passed;
    std::cout << "[PASS] CPU and HLSL sky fields remain aligned\n";

    PpmFilesCarryExactDimensions(context, captures, output_directory);
    ++passed;
    std::cout << "[PASS] PPM captures preserve exact dimensions\n";

    for (const auto& capture : captures) {
      std::cout << "capture " << capture.name << " sha256=" << capture.hash
                << " mean_luma=" << capture.metrics.mean_luminance
                << " upper_chroma=" << capture.metrics.upper_chroma << '\n';
    }
    std::cout << "Truth WARP reference cases: " << passed << "/8; assertions: "
              << context.assertions << '\n';
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << "[FAIL] Truth WARP reference: " << exception.what() << '\n';
    return 1;
  }
}
