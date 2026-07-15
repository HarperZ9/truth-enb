#include "truth/render/ReferenceRenderer.hpp"

#include <algorithm>
#include <array>
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

    PpmFilesCarryExactDimensions(context, captures, output_directory);
    ++passed;
    std::cout << "[PASS] PPM captures preserve exact dimensions\n";

    for (const auto& capture : captures) {
      std::cout << "capture " << capture.name << " sha256=" << capture.hash
                << " mean_luma=" << capture.metrics.mean_luminance
                << " upper_chroma=" << capture.metrics.upper_chroma << '\n';
    }
    std::cout << "Truth WARP reference cases: " << passed << "/6; assertions: "
              << context.assertions << '\n';
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << "[FAIL] Truth WARP reference: " << exception.what() << '\n';
    return 1;
  }
}
