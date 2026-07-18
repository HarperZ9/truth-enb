#include "truth/render/CloudVolume.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <iterator>
#include <limits>
#include <string_view>

namespace {

using truth::render::CloudVolumeDiagnostic;
using truth::render::CloudVolumeEvaluation;
using truth::render::CloudVolumeInput;
using truth::render::CloudVolumeInterleavedJitter;
using truth::render::CloudVolumeLayerIntersection;
using truth::render::CloudVolumeOutput;
using truth::render::CloudVolumeQuality;
using truth::render::CloudVolumeStatus;
using truth::render::CloudVolumeVector;
using truth::render::CloudVolumeVerticalProfile;
using truth::render::EvaluateCloudVolume;
using truth::render::IntersectCloudVolumeLayer;
using truth::render::SampleCloudVolumeDensity;

class TestFailure final : public std::exception {
public:
  explicit TestFailure(const std::string_view message) noexcept : message_(message) {}
  [[nodiscard]] const char* what() const noexcept override { return message_.data(); }

private:
  std::string_view message_;
};

struct TestContext {
  std::uint64_t assertions{};

  void expect(const bool condition, const std::string_view message) {
    ++assertions;
    if (!condition) {
      throw TestFailure{message};
    }
  }
};

[[nodiscard]] bool SameFloatBits(const float lhs, const float rhs) noexcept {
  return std::bit_cast<std::uint32_t>(lhs) == std::bit_cast<std::uint32_t>(rhs);
}

[[nodiscard]] bool SameVector(
    const CloudVolumeVector& lhs,
    const CloudVolumeVector& rhs) noexcept {
  return SameFloatBits(lhs.x, rhs.x)
      && SameFloatBits(lhs.y, rhs.y)
      && SameFloatBits(lhs.z, rhs.z);
}

[[nodiscard]] bool SameOutput(
    const CloudVolumeOutput& lhs,
    const CloudVolumeOutput& rhs) noexcept {
  return SameVector(lhs.radiance, rhs.radiance)
      && SameFloatBits(lhs.transmittance, rhs.transmittance)
      && SameFloatBits(lhs.optical_depth, rhs.optical_depth)
      && lhs.primary_steps == rhs.primary_steps
      && lhs.light_samples == rhs.light_samples;
}

[[nodiscard]] CloudVolumeInput ReferenceInput() noexcept {
  return {
      {0.0F, 0.0F, 0.20F},
      {0.25F, 0.15F, 0.9565563F},
      {0.35F, 0.15F, 0.9246621F},
      1.20F,
      3.80F,
      60.0F,
      0.37F,
      0.62F,
      -0.27F,
      0.58F,
      0.78F,
      0.32F,
      0.52F,
      0.0F,
      17U,
      9U,
      0U,
      CloudVolumeQuality::quality,
  };
}

void ExpectSucceeded(TestContext& context, const CloudVolumeEvaluation evaluation) {
  context.expect(evaluation.status == CloudVolumeStatus::evaluated,
                 "cloud-volume evaluation was rejected");
  context.expect(evaluation.diagnostic == CloudVolumeDiagnostic::none,
                 "successful cloud-volume evaluation returned a diagnostic");
}

void StableCodesAndBudgetsAreExplicit(TestContext& context) {
  context.expect(static_cast<std::uint32_t>(CloudVolumeStatus::evaluated) == 0U,
                 "evaluated status code changed");
  context.expect(static_cast<std::uint32_t>(CloudVolumeStatus::rejected) == 1U,
                 "rejected status code changed");
  context.expect(static_cast<std::uint32_t>(CloudVolumeDiagnostic::none) == 0U,
                 "none diagnostic code changed");
  context.expect(static_cast<std::uint32_t>(CloudVolumeDiagnostic::camera_non_finite) == 100U,
                 "camera diagnostic code changed");
  context.expect(static_cast<std::uint32_t>(CloudVolumeDiagnostic::calculation_out_of_range)
                     == 301U,
                 "calculation diagnostic code changed");
  context.expect(truth::render::CloudVolumePrimaryStepBudget(
                     CloudVolumeQuality::performance) == 0U,
                 "performance primary budget changed");
  context.expect(truth::render::CloudVolumePrimaryStepBudget(
                     CloudVolumeQuality::balanced) == 0U,
                 "balanced primary budget changed");
  context.expect(truth::render::CloudVolumePrimaryStepBudget(
                     CloudVolumeQuality::quality) == 8U,
                 "quality primary budget changed");
  context.expect(truth::render::CloudVolumePrimaryStepBudget(
                     CloudVolumeQuality::ultra) == 12U,
                 "ultra primary budget changed");
  context.expect(truth::render::CloudVolumePrimaryStepBudget(
                     CloudVolumeQuality::cinematic) == 16U,
                 "cinematic primary budget changed");
  context.expect(truth::render::CloudVolumeLightStepBudget(
                     CloudVolumeQuality::performance) == 0U,
                 "performance light budget changed");
  context.expect(truth::render::CloudVolumeLightStepBudget(
                     CloudVolumeQuality::balanced) == 0U,
                 "balanced light budget changed");
  context.expect(truth::render::CloudVolumeLightStepBudget(
                     CloudVolumeQuality::quality) == 2U,
                 "quality light budget changed");
  context.expect(truth::render::CloudVolumeLightStepBudget(
                     CloudVolumeQuality::ultra) == 3U,
                 "ultra light budget changed");
  context.expect(truth::render::CloudVolumeLightStepBudget(
                     CloudVolumeQuality::cinematic) == 4U,
                 "cinematic light budget changed");
}

void RayLayerIntersectionsAreAnalytic(TestContext& context) {
  CloudVolumeLayerIntersection intersection{-1.0F, -2.0F};
  context.expect(IntersectCloudVolumeLayer(0.0F, 0.5F, 1.0F, 3.0F, 10.0F,
                                           intersection),
                 "upward ray missed a bounded cloud slab");
  context.expect(std::fabs(intersection.near_distance - 2.0F) < 1.0e-6F,
                 "slab entry distance was wrong");
  context.expect(std::fabs(intersection.far_distance - 6.0F) < 1.0e-6F,
                 "slab exit distance was wrong");

  context.expect(IntersectCloudVolumeLayer(2.0F, 0.5F, 1.0F, 3.0F, 10.0F,
                                           intersection),
                 "camera inside cloud slab did not intersect");
  context.expect(SameFloatBits(intersection.near_distance, 0.0F),
                 "inside-slab entry was not exact zero");
  context.expect(std::fabs(intersection.far_distance - 2.0F) < 1.0e-6F,
                 "inside-slab exit distance was wrong");

  intersection = {-3.0F, -4.0F};
  context.expect(!IntersectCloudVolumeLayer(0.0F, 0.0F, 1.0F, 3.0F, 10.0F,
                                            intersection),
                 "horizontal ray entered a finite height slab");
  context.expect(SameFloatBits(intersection.near_distance, -3.0F)
                     && SameFloatBits(intersection.far_distance, -4.0F),
                 "missed slab intersection mutated output");
}

void JitterIsDeterministicAndDistributed(TestContext& context) {
  float minimum = 1.0F;
  float maximum = 0.0F;
  std::array<std::uint32_t, 16> bins{};
  for (std::uint32_t y = 0; y < 32U; ++y) {
    for (std::uint32_t x = 0; x < 32U; ++x) {
      const float first = CloudVolumeInterleavedJitter(x, y, 7U);
      const float second = CloudVolumeInterleavedJitter(x, y, 7U);
      context.expect(SameFloatBits(first, second), "ray jitter changed output bits");
      context.expect(std::isfinite(first) && first >= 0.0F && first < 1.0F,
                     "ray jitter left [0,1)");
      minimum = std::min(minimum, first);
      maximum = std::max(maximum, first);
      const auto bin = std::min(static_cast<std::uint32_t>(first * 16.0F), 15U);
      ++bins[bin];
    }
  }
  context.expect(maximum - minimum > 0.90F, "ray jitter did not span its domain");
  for (const auto count : bins) {
    context.expect(count > 32U, "ray jitter left a large distribution hole");
  }
  context.expect(!SameFloatBits(CloudVolumeInterleavedJitter(3U, 5U, 0U),
                                CloudVolumeInterleavedJitter(3U, 5U, 1U)),
                 "jitter frame did not decorrelate the sample");
}

void VerticalProfilesSpanCloudFamilies(TestContext& context) {
  for (std::uint32_t type_index = 0; type_index <= 20U; ++type_index) {
    const float type = static_cast<float>(type_index) / 20.0F;
    context.expect(SameFloatBits(CloudVolumeVerticalProfile(0.0F, type), 0.0F),
                   "vertical profile did not close at cloud base");
    context.expect(SameFloatBits(CloudVolumeVerticalProfile(1.0F, type), 0.0F),
                   "vertical profile did not close at cloud top");
    for (std::uint32_t height_index = 0; height_index <= 100U; ++height_index) {
      const float height = static_cast<float>(height_index) / 100.0F;
      const float value = CloudVolumeVerticalProfile(height, type);
      context.expect(std::isfinite(value) && value >= 0.0F && value <= 1.0F,
                     "vertical profile left [0,1]");
    }
  }
  const float stratus_upper = CloudVolumeVerticalProfile(0.84F, 0.0F);
  const float cumulus_middle = CloudVolumeVerticalProfile(0.52F, 0.5F);
  const float anvil_upper = CloudVolumeVerticalProfile(0.84F, 1.0F);
  context.expect(cumulus_middle > 0.45F, "cumulus profile had no deep middle body");
  context.expect(anvil_upper > stratus_upper + 0.25F,
                 "anvil profile did not preserve an upper shelf");
}

void InvalidInputNeverMutatesOutput(TestContext& context) {
  const CloudVolumeOutput sentinel{{-1.0F, 2.0F, -3.0F}, 4.0F, -5.0F, 6U, 7U};
  const auto reject = [&](const CloudVolumeInput& input,
                          const CloudVolumeDiagnostic expected) {
    CloudVolumeOutput output = sentinel;
    const auto evaluation = EvaluateCloudVolume(input, output);
    context.expect(evaluation.status == CloudVolumeStatus::rejected,
                   "invalid volume input was accepted");
    context.expect(evaluation.diagnostic == expected,
                   "invalid volume diagnostic was wrong");
    context.expect(SameOutput(output, sentinel), "invalid volume input mutated output");
  };

  CloudVolumeInput input = ReferenceInput();
  input.camera_position.x = std::numeric_limits<float>::quiet_NaN();
  reject(input, CloudVolumeDiagnostic::camera_non_finite);
  input = ReferenceInput(); input.view_direction = {0.5F, 0.5F, 0.5F};
  reject(input, CloudVolumeDiagnostic::view_direction_not_normalized);
  input = ReferenceInput(); input.sun_direction.z = 2.0F;
  reject(input, CloudVolumeDiagnostic::sun_direction_out_of_range);
  input = ReferenceInput(); input.phase = 1.01F;
  reject(input, CloudVolumeDiagnostic::phase_out_of_range);
  input = ReferenceInput(); input.cloud_coverage = -0.01F;
  reject(input, CloudVolumeDiagnostic::control_out_of_range);
  input = ReferenceInput(); input.cloud_top_height = input.cloud_base_height;
  reject(input, CloudVolumeDiagnostic::layer_out_of_range);
  input = ReferenceInput(); input.max_distance = 0.0F;
  reject(input, CloudVolumeDiagnostic::max_distance_out_of_range);
  input = ReferenceInput(); input.quality = static_cast<CloudVolumeQuality>(99U);
  reject(input, CloudVolumeDiagnostic::quality_out_of_range);
}

void ClearAndMissedVolumesAreExactIdentities(TestContext& context) {
  for (const bool zero_coverage : {false, true}) {
    CloudVolumeInput input = ReferenceInput();
    if (zero_coverage) {
      input.cloud_coverage = 0.0F;
    } else {
      input.cloud_density = 0.0F;
    }
    CloudVolumeOutput output{};
    ExpectSucceeded(context, EvaluateCloudVolume(input, output));
    context.expect(SameVector(output.radiance, {0.0F, 0.0F, 0.0F}),
                   "clear volume emitted radiance");
    context.expect(SameFloatBits(output.transmittance, 1.0F),
                   "clear volume transmittance was not exact one");
    context.expect(SameFloatBits(output.optical_depth, 0.0F),
                   "clear volume optical depth was not exact zero");
  }

  CloudVolumeInput missed = ReferenceInput();
  missed.view_direction = {0.0F, 0.6F, -0.8F};
  CloudVolumeOutput output{};
  ExpectSucceeded(context, EvaluateCloudVolume(missed, output));
  context.expect(SameVector(output.radiance, {0.0F, 0.0F, 0.0F})
                     && SameFloatBits(output.transmittance, 1.0F)
                     && SameFloatBits(output.optical_depth, 0.0F),
                 "ray missing cloud layer was not an exact identity");
}

[[nodiscard]] float MeanDensity(CloudVolumeInput input) {
  float sum{};
  constexpr std::uint32_t grid = 24U;
  for (std::uint32_t y = 0; y < grid; ++y) {
    for (std::uint32_t x = 0; x < grid; ++x) {
      const CloudVolumeVector position{
          -12.0F + static_cast<float>(x),
          -12.0F + static_cast<float>(y),
          input.cloud_base_height
              + (input.cloud_top_height - input.cloud_base_height)
                  * (0.15F + (0.70F * static_cast<float>((x + y) % grid)
                              / static_cast<float>(grid - 1U))),
      };
      float density = -1.0F;
      if (SampleCloudVolumeDensity(input, position, density).status
          != CloudVolumeStatus::evaluated) {
        throw TestFailure{"volume density sample was rejected"};
      }
      sum += density;
    }
  }
  return sum / static_cast<float>(grid * grid);
}

void DensityCoverageWeatherAndTypeRespond(TestContext& context) {
  CloudVolumeInput input = ReferenceInput();
  input.cloud_coverage = 0.22F;
  const float sparse = MeanDensity(input);
  input.cloud_coverage = 0.82F;
  const float covered = MeanDensity(input);
  context.expect(covered > sparse + 0.08F,
                 "coverage did not add volume occupancy");

  input.cloud_coverage = 0.62F;
  input.weather_density = 0.0F;
  const float clear_weather = MeanDensity(input);
  input.weather_density = 1.0F;
  const float storm_weather = MeanDensity(input);
  context.expect(storm_weather > clear_weather + 0.025F,
                 "weather did not deepen volume density");

  input.weather_density = 0.45F;
  input.cloud_type = 0.0F;
  const float stratus = MeanDensity(input);
  input.cloud_type = 1.0F;
  const float anvil = MeanDensity(input);
  context.expect(std::fabs(stratus - anvil) > 0.005F,
                 "cloud type did not reshape sampled density");
}

void PhaseLoopIsBitExactAndCameraTranslationCreatesParallax(TestContext& context) {
  CloudVolumeInput start = ReferenceInput();
  start.phase = 0.0F;
  CloudVolumeInput end = start;
  end.phase = 1.0F;
  CloudVolumeOutput start_output{};
  CloudVolumeOutput end_output{};
  ExpectSucceeded(context, EvaluateCloudVolume(start, start_output));
  ExpectSucceeded(context, EvaluateCloudVolume(end, end_output));
  context.expect(SameOutput(start_output, end_output),
                 "volume phase 0 and 1 did not share an exact seam");

  CloudVolumeInput translated = start;
  translated.camera_position.x += 0.65F;
  translated.camera_position.y -= 0.35F;
  CloudVolumeOutput translated_output{};
  ExpectSucceeded(context, EvaluateCloudVolume(translated, translated_output));
  context.expect(!SameOutput(start_output, translated_output),
                 "camera translation did not change the 3D volume result");
}

void TransmittanceAndLightingRespondPhysically(TestContext& context) {
  CloudVolumeInput thin = ReferenceInput();
  thin.cloud_density = 0.35F;
  CloudVolumeInput thick = thin;
  thick.cloud_density = 1.0F;
  CloudVolumeOutput thin_output{};
  CloudVolumeOutput thick_output{};
  ExpectSucceeded(context, EvaluateCloudVolume(thin, thin_output));
  ExpectSucceeded(context, EvaluateCloudVolume(thick, thick_output));
  context.expect(thick_output.transmittance < thin_output.transmittance,
                 "transmittance did not fall with density");
  context.expect(thick_output.optical_depth > thin_output.optical_depth,
                 "optical depth did not rise with density");
  context.expect(thick_output.light_samples > 0U,
                 "occupied volume took no sun-shadow samples");

  CloudVolumeInput forward = ReferenceInput();
  forward.sun_direction = forward.view_direction;
  CloudVolumeInput backward = forward;
  backward.sun_direction = {
      -forward.view_direction.x,
      -forward.view_direction.y,
      forward.view_direction.z,
  };
  const float backward_length = std::sqrt(
      (backward.sun_direction.x * backward.sun_direction.x)
      + (backward.sun_direction.y * backward.sun_direction.y)
      + (backward.sun_direction.z * backward.sun_direction.z));
  backward.sun_direction.x /= backward_length;
  backward.sun_direction.y /= backward_length;
  backward.sun_direction.z /= backward_length;
  CloudVolumeOutput forward_output{};
  CloudVolumeOutput backward_output{};
  ExpectSucceeded(context, EvaluateCloudVolume(forward, forward_output));
  ExpectSucceeded(context, EvaluateCloudVolume(backward, backward_output));
  const float forward_luma = (0.2126F * forward_output.radiance.x)
      + (0.7152F * forward_output.radiance.y)
      + (0.0722F * forward_output.radiance.z);
  const float backward_luma = (0.2126F * backward_output.radiance.x)
      + (0.7152F * backward_output.radiance.y)
      + (0.0722F * backward_output.radiance.z);
  context.expect(forward_luma > backward_luma,
                 "forward phase/silver lining did not respond to sun angle");
}

void WeatherAndNightLightingStayControlled(TestContext& context) {
  CloudVolumeInput night = ReferenceInput();
  night.night_factor = 1.0F;
  night.weather_density = 0.04F;
  CloudVolumeOutput night_output{};
  ExpectSucceeded(context, EvaluateCloudVolume(night, night_output));
  context.expect(night_output.radiance.z <= (2.0F * night_output.radiance.x),
                 "night cloud fill became electric blue");

  CloudVolumeInput storm_forward = ReferenceInput();
  storm_forward.view_direction = {0.0F, 0.8F, 0.6F};
  storm_forward.sun_direction = storm_forward.view_direction;
  storm_forward.weather_density = 0.88F;
  storm_forward.cloud_coverage = 0.62F;
  storm_forward.cloud_density = 0.92F;
  storm_forward.cloud_type = 0.88F;
  CloudVolumeInput storm_backward = storm_forward;
  storm_backward.sun_direction = {0.0F, -0.8F, 0.6F};
  CloudVolumeOutput forward_output{};
  CloudVolumeOutput backward_output{};
  ExpectSucceeded(context, EvaluateCloudVolume(storm_forward, forward_output));
  ExpectSucceeded(context, EvaluateCloudVolume(storm_backward, backward_output));
  const auto luminance = [](const CloudVolumeVector radiance) noexcept {
    return (0.2126F * radiance.x)
        + (0.7152F * radiance.y)
        + (0.0722F * radiance.z);
  };
  const float forward_luma = luminance(forward_output.radiance);
  const float backward_luma = luminance(backward_output.radiance);
  context.expect(forward_luma > backward_luma,
                 "weather removed all directional cloud lighting");
  context.expect(forward_luma < (2.0F * backward_luma),
                 "storm silver lining collapsed into a white spotlight");
}

void QualityBudgetsAndDenseGridStayBounded(TestContext& context) {
  constexpr float pi = 3.14159265358979323846F;
  for (const auto quality : {CloudVolumeQuality::quality,
                             CloudVolumeQuality::ultra,
                             CloudVolumeQuality::cinematic}) {
    for (std::uint32_t azimuth_index = 0; azimuth_index < 12U; ++azimuth_index) {
      const float azimuth = 2.0F * pi * static_cast<float>(azimuth_index) / 12.0F;
      for (std::uint32_t elevation_index = 1; elevation_index <= 5U; ++elevation_index) {
        const float elevation = 0.12F + (1.25F * static_cast<float>(elevation_index) / 5.0F);
        for (std::uint32_t control_index = 0; control_index <= 4U; ++control_index) {
          CloudVolumeInput input = ReferenceInput();
          input.view_direction = {
              std::cos(elevation) * std::sin(azimuth),
              std::cos(elevation) * std::cos(azimuth),
              std::sin(elevation),
          };
          input.phase = static_cast<float>((azimuth_index + control_index) % 5U) / 4.0F;
          input.cloud_coverage = static_cast<float>(control_index) / 4.0F;
          input.cloud_density = static_cast<float>((control_index + 2U) % 5U) / 4.0F;
          input.weather_density = static_cast<float>((control_index + 1U) % 5U) / 4.0F;
          input.cloud_type = static_cast<float>(elevation_index - 1U) / 4.0F;
          input.night_factor = static_cast<float>(azimuth_index % 5U) / 4.0F;
          input.pixel_x = azimuth_index;
          input.pixel_y = elevation_index;
          input.quality = quality;
          CloudVolumeOutput output{};
          ExpectSucceeded(context, EvaluateCloudVolume(input, output));
          context.expect(std::isfinite(output.radiance.x)
                             && std::isfinite(output.radiance.y)
                             && std::isfinite(output.radiance.z),
                         "volume radiance became non-finite");
          context.expect(output.radiance.x >= 0.0F
                             && output.radiance.x <= truth::render::kCloudVolumeMaximumRadiance
                             && output.radiance.y >= 0.0F
                             && output.radiance.y <= truth::render::kCloudVolumeMaximumRadiance
                             && output.radiance.z >= 0.0F
                             && output.radiance.z <= truth::render::kCloudVolumeMaximumRadiance,
                         "volume radiance left its energy bound");
          context.expect(std::isfinite(output.transmittance)
                             && output.transmittance >= 0.0F
                             && output.transmittance <= 1.0F,
                         "volume transmittance left [0,1]");
          context.expect(std::isfinite(output.optical_depth)
                             && output.optical_depth >= 0.0F
                             && output.optical_depth
                                 <= truth::render::kCloudVolumeMaximumOpticalDepth,
                         "volume optical depth left its bound");
          const auto primary_budget = truth::render::CloudVolumePrimaryStepBudget(quality);
          const auto light_budget = truth::render::CloudVolumeLightStepBudget(quality);
          context.expect(output.primary_steps <= primary_budget,
                         "volume exceeded primary step budget");
          context.expect(output.light_samples <= primary_budget * light_budget,
                         "volume exceeded light step budget");
        }
      }
    }
  }
}

using TestFunction = void (*)(TestContext&);
struct TestCase { std::string_view name; TestFunction function; };

constexpr TestCase kTests[]{
    {"stable codes and budgets are explicit", &StableCodesAndBudgetsAreExplicit},
    {"ray layer intersections are analytic", &RayLayerIntersectionsAreAnalytic},
    {"jitter is deterministic and distributed", &JitterIsDeterministicAndDistributed},
    {"vertical profiles span cloud families", &VerticalProfilesSpanCloudFamilies},
    {"invalid input never mutates output", &InvalidInputNeverMutatesOutput},
    {"clear and missed volumes are exact identities", &ClearAndMissedVolumesAreExactIdentities},
    {"density coverage weather and type respond", &DensityCoverageWeatherAndTypeRespond},
    {"phase loop is exact and camera translation creates parallax",
     &PhaseLoopIsBitExactAndCameraTranslationCreatesParallax},
    {"transmittance and lighting respond physically", &TransmittanceAndLightingRespondPhysically},
    {"weather and night lighting stay controlled", &WeatherAndNightLightingStayControlled},
    {"quality budgets and dense grid stay bounded", &QualityBudgetsAndDenseGridStayBounded},
};

}  // namespace

int main() {
  TestContext context;
  std::uint32_t passed{};
  for (const auto& test : kTests) {
    try {
      test.function(context);
      ++passed;
      std::cout << "[PASS] " << test.name << '\n';
    } catch (const std::exception& exception) {
      std::cerr << "[FAIL] " << test.name << ": " << exception.what() << '\n';
      return 1;
    }
  }
  std::cout << "Truth cloud-volume C++ cases: " << passed << '/' << std::size(kTests)
            << "; assertions: " << context.assertions << '\n';
  return 0;
}
