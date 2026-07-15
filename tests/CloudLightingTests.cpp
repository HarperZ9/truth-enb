#include "truth/render/CloudLighting.hpp"
#include "truth/render/SkyFields.hpp"

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

using truth::render::CloudLightingDiagnostic;
using truth::render::CloudLightingEvaluation;
using truth::render::CloudLightingInput;
using truth::render::CloudLightingOutput;
using truth::render::CloudLightingStatus;
using truth::render::EvaluateCloudLighting;
using truth::render::EvaluateSkyFields;
using truth::render::RgbRadiance;
using truth::render::SkyFieldInput;
using truth::render::SkyFieldOutput;
using truth::render::SkyFieldStatus;

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

[[nodiscard]] bool SameRadiance(const RgbRadiance& lhs, const RgbRadiance& rhs) noexcept {
  return SameFloatBits(lhs.r, rhs.r)
      && SameFloatBits(lhs.g, rhs.g)
      && SameFloatBits(lhs.b, rhs.b);
}

[[nodiscard]] bool SameOutput(
    const CloudLightingOutput& lhs,
    const CloudLightingOutput& rhs) noexcept {
  return SameFloatBits(lhs.cloud_optical_depth, rhs.cloud_optical_depth)
      && SameFloatBits(lhs.cloud_transmittance, rhs.cloud_transmittance)
      && SameFloatBits(lhs.forward_scattering, rhs.forward_scattering)
      && SameFloatBits(lhs.silver_lining, rhs.silver_lining)
      && SameFloatBits(lhs.direct_scattering, rhs.direct_scattering)
      && SameFloatBits(lhs.ambient_scattering, rhs.ambient_scattering)
      && SameFloatBits(lhs.multiple_scattering, rhs.multiple_scattering)
      && SameFloatBits(lhs.self_shadow, rhs.self_shadow)
      && SameFloatBits(lhs.powder_response, rhs.powder_response)
      && SameRadiance(lhs.cloud_tint, rhs.cloud_tint)
      && SameRadiance(lhs.cloud_radiance, rhs.cloud_radiance)
      && SameRadiance(lhs.aurora_radiance, rhs.aurora_radiance)
      && SameRadiance(lhs.composite_radiance, rhs.composite_radiance);
}

[[nodiscard]] CloudLightingInput ReferenceInput() noexcept {
  return {
      {0.18F, 0.32F, 0.68F},
      {0.12F, 0.62F, 0.34F},
      0.65F,
      0.40F,
      0.35F,
      0.55F,
      0.18F,
      0.30F,
      0.0F,
      0.90F,
  };
}

void ExpectSucceeded(TestContext& context, const CloudLightingEvaluation evaluation) {
  context.expect(evaluation.status == CloudLightingStatus::evaluated,
                 "cloud-lighting evaluation was rejected");
  context.expect(evaluation.diagnostic == CloudLightingDiagnostic::none,
                 "successful cloud-lighting evaluation returned a diagnostic");
}

void ExpectNear(
    TestContext& context,
    const float actual,
    const float expected,
    const float tolerance,
    const std::string_view message) {
  context.expect(std::isfinite(actual), "near comparison received non-finite actual value");
  context.expect(std::fabs(actual - expected) <= tolerance, message);
}

void ExpectRange(
    TestContext& context,
    const float value,
    const float minimum,
    const float maximum,
    const std::string_view message) {
  context.expect(std::isfinite(value), "bounded cloud-lighting value was non-finite");
  context.expect(value >= minimum && value <= maximum, message);
}

void ExpectRadianceRange(
    TestContext& context,
    const RgbRadiance& value,
    const float maximum,
    const std::string_view message) {
  ExpectRange(context, value.r, 0.0F, maximum, message);
  ExpectRange(context, value.g, 0.0F, maximum, message);
  ExpectRange(context, value.b, 0.0F, maximum, message);
}

void StableCodesAndBoundsAreExplicit(TestContext& context) {
  context.expect(static_cast<std::uint32_t>(CloudLightingStatus::evaluated) == 0U,
                 "evaluated status code changed");
  context.expect(static_cast<std::uint32_t>(CloudLightingStatus::rejected) == 1U,
                 "rejected status code changed");
  context.expect(static_cast<std::uint32_t>(CloudLightingDiagnostic::none) == 0U,
                 "none diagnostic code changed");
  context.expect(static_cast<std::uint32_t>(CloudLightingDiagnostic::sky_radiance_non_finite)
                     == 100U,
                 "sky-radiance diagnostic code changed");
  context.expect(static_cast<std::uint32_t>(CloudLightingDiagnostic::fog_transmittance_out_of_range)
                     == 191U,
                 "fog-transmittance diagnostic code changed");
  context.expect(static_cast<std::uint32_t>(CloudLightingDiagnostic::calculation_out_of_range)
                     == 301U,
                 "calculation diagnostic code changed");
  context.expect(truth::render::kCloudLightingMaximumOpticalDepth > 0.0F,
                 "optical-depth bound was not positive");
  context.expect(truth::render::kCloudLightingMaximumCloudRadiance > 0.0F,
                 "cloud-radiance bound was not positive");
}

void InvalidInputNeverMutatesOutput(TestContext& context) {
  const CloudLightingOutput sentinel{
      -1.0F, 2.0F, -3.0F, 4.0F, -5.0F, 6.0F, -7.0F, 8.0F, -9.0F,
      {10.0F, -11.0F, 12.0F},
      {-13.0F, 14.0F, -15.0F},
      {16.0F, -17.0F, 18.0F},
      {-19.0F, 20.0F, -21.0F},
  };
  const CloudLightingInput valid = ReferenceInput();
  const auto reject = [&](const CloudLightingInput input,
                          const CloudLightingDiagnostic expected) {
    CloudLightingOutput output = sentinel;
    const CloudLightingEvaluation evaluation = EvaluateCloudLighting(input, output);
    context.expect(evaluation.status == CloudLightingStatus::rejected,
                   "invalid cloud-lighting input was accepted");
    context.expect(evaluation.diagnostic == expected,
                   "invalid cloud-lighting diagnostic was wrong");
    context.expect(SameOutput(output, sentinel),
                   "invalid cloud-lighting input mutated output");
  };

  const float nan = std::numeric_limits<float>::quiet_NaN();
  const float infinity = std::numeric_limits<float>::infinity();
  CloudLightingInput input = valid;
  input.sky_radiance.r = nan;
  reject(input, CloudLightingDiagnostic::sky_radiance_non_finite);
  input = valid; input.sky_radiance.b = truth::render::kCloudLightingMaximumInputRadiance + 1.0F;
  reject(input, CloudLightingDiagnostic::sky_radiance_out_of_range);
  input = valid; input.aurora_intrinsic_radiance.g = infinity;
  reject(input, CloudLightingDiagnostic::aurora_radiance_non_finite);
  input = valid; input.aurora_intrinsic_radiance.r = -0.01F;
  reject(input, CloudLightingDiagnostic::aurora_radiance_out_of_range);
  input = valid; input.view_zenith_cosine = nan;
  reject(input, CloudLightingDiagnostic::view_zenith_cosine_non_finite);
  input = valid; input.view_zenith_cosine = 1.01F;
  reject(input, CloudLightingDiagnostic::view_zenith_cosine_out_of_range);
  input = valid; input.view_sun_cosine = infinity;
  reject(input, CloudLightingDiagnostic::view_sun_cosine_non_finite);
  input = valid; input.view_sun_cosine = -1.01F;
  reject(input, CloudLightingDiagnostic::view_sun_cosine_out_of_range);
  input = valid; input.sun_elevation = nan;
  reject(input, CloudLightingDiagnostic::sun_elevation_non_finite);
  input = valid; input.sun_elevation = 1.01F;
  reject(input, CloudLightingDiagnostic::sun_elevation_out_of_range);
  input = valid; input.cloud_density = infinity;
  reject(input, CloudLightingDiagnostic::cloud_density_non_finite);
  input = valid; input.cloud_density = -0.01F;
  reject(input, CloudLightingDiagnostic::cloud_density_out_of_range);
  input = valid; input.cloud_detail_erosion = nan;
  reject(input, CloudLightingDiagnostic::cloud_detail_erosion_non_finite);
  input = valid; input.cloud_detail_erosion = 1.01F;
  reject(input, CloudLightingDiagnostic::cloud_detail_erosion_out_of_range);
  input = valid; input.weather_density = infinity;
  reject(input, CloudLightingDiagnostic::weather_density_non_finite);
  input = valid; input.weather_density = -0.01F;
  reject(input, CloudLightingDiagnostic::weather_density_out_of_range);
  input = valid; input.night_factor = nan;
  reject(input, CloudLightingDiagnostic::night_factor_non_finite);
  input = valid; input.night_factor = 1.01F;
  reject(input, CloudLightingDiagnostic::night_factor_out_of_range);
  input = valid; input.fog_transmittance = infinity;
  reject(input, CloudLightingDiagnostic::fog_transmittance_non_finite);
  input = valid; input.fog_transmittance = -0.01F;
  reject(input, CloudLightingDiagnostic::fog_transmittance_out_of_range);
}

void ClearSkyIsAnExactIdentity(TestContext& context) {
  CloudLightingInput input = ReferenceInput();
  input.cloud_density = 0.0F;
  input.fog_transmittance = 1.0F;
  input.aurora_intrinsic_radiance = {0.0F, 0.0F, 0.0F};
  CloudLightingOutput output{};
  ExpectSucceeded(context, EvaluateCloudLighting(input, output));
  context.expect(SameFloatBits(output.cloud_optical_depth, 0.0F),
                 "clear sky optical depth was not exact +0");
  context.expect(SameFloatBits(output.cloud_transmittance, 1.0F),
                 "clear sky transmittance was not exact 1");
  context.expect(SameRadiance(output.cloud_radiance, {0.0F, 0.0F, 0.0F}),
                 "clear sky emitted cloud radiance");
  context.expect(SameRadiance(output.composite_radiance, input.sky_radiance),
                 "clear cloud/fog/aurora path changed sky radiance bits");
}

void TransmittanceFallsMonotonicallyWithDensity(TestContext& context) {
  CloudLightingInput input = ReferenceInput();
  input.view_zenith_cosine = 0.12F;
  float previous = 1.0F;
  for (std::uint32_t index = 0; index <= 100U; ++index) {
    input.cloud_density = static_cast<float>(index) / 100.0F;
    CloudLightingOutput output{};
    ExpectSucceeded(context, EvaluateCloudLighting(input, output));
    context.expect(output.cloud_transmittance <= previous,
                   "cloud transmittance increased with density");
    if (index > 0U) {
      context.expect(output.cloud_transmittance < previous,
                     "cloud transmittance did not strictly respond to density");
    }
    previous = output.cloud_transmittance;
  }
}

void SunAndEdgeTermsAreVisibleAndBounded(TestContext& context) {
  CloudLightingInput broad = ReferenceInput();
  broad.view_sun_cosine = 0.0F;
  broad.cloud_detail_erosion = 0.0F;
  CloudLightingInput forward = broad;
  forward.view_sun_cosine = 0.995F;
  forward.cloud_detail_erosion = 1.0F;
  CloudLightingOutput broad_output{};
  CloudLightingOutput forward_output{};
  ExpectSucceeded(context, EvaluateCloudLighting(broad, broad_output));
  ExpectSucceeded(context, EvaluateCloudLighting(forward, forward_output));
  context.expect(forward_output.forward_scattering > broad_output.forward_scattering,
                 "forward phase did not brighten toward the sun");
  context.expect(forward_output.silver_lining > broad_output.silver_lining,
                 "edge term did not form a silver lining");
  context.expect(forward_output.direct_scattering > broad_output.direct_scattering,
                 "sun-facing edge did not increase direct scattering");
  ExpectRange(context, forward_output.forward_scattering, 0.0F,
              truth::render::kCloudLightingMaximumForwardScattering,
              "forward scattering exceeded its bound");
  ExpectRange(context, forward_output.silver_lining, 0.0F, 1.0F,
              "silver lining exceeded [0,1]");
  ExpectRange(context, forward_output.direct_scattering, 0.0F,
              truth::render::kCloudLightingMaximumDirectScattering,
              "direct scattering exceeded its bound");
}

void ShadowPowderAndTintsRespondPhysically(TestContext& context) {
  CloudLightingInput thin = ReferenceInput();
  thin.cloud_density = 0.15F;
  CloudLightingInput thick = thin;
  thick.cloud_density = 0.90F;
  CloudLightingOutput thin_output{};
  CloudLightingOutput thick_output{};
  ExpectSucceeded(context, EvaluateCloudLighting(thin, thin_output));
  ExpectSucceeded(context, EvaluateCloudLighting(thick, thick_output));
  context.expect(thick_output.self_shadow < thin_output.self_shadow,
                 "self-shadow did not strengthen with optical depth");
  context.expect(thick_output.powder_response > thin_output.powder_response,
                 "powder response did not strengthen with density");
  context.expect(thick_output.multiple_scattering > thin_output.multiple_scattering,
                 "multiple-scattering floor did not respond to powder");
  context.expect(thick_output.ambient_scattering >= thick_output.multiple_scattering,
                 "ambient floor excluded multiple scattering");

  CloudLightingInput day = ReferenceInput();
  day.weather_density = 0.0F;
  day.sun_elevation = 1.0F;
  day.night_factor = 0.0F;
  CloudLightingInput storm = day;
  storm.weather_density = 1.0F;
  CloudLightingInput night = day;
  night.sun_elevation = -1.0F;
  night.night_factor = 1.0F;
  CloudLightingOutput day_output{};
  CloudLightingOutput storm_output{};
  CloudLightingOutput night_output{};
  ExpectSucceeded(context, EvaluateCloudLighting(day, day_output));
  ExpectSucceeded(context, EvaluateCloudLighting(storm, storm_output));
  ExpectSucceeded(context, EvaluateCloudLighting(night, night_output));
  context.expect(day_output.cloud_tint.r > day_output.cloud_tint.b,
                 "day cloud tint was not warm");
  context.expect(storm_output.cloud_tint.r < day_output.cloud_tint.r,
                 "weather did not cool/dim the red cloud tint");
  context.expect((storm_output.cloud_tint.b / storm_output.cloud_tint.r)
                     > (day_output.cloud_tint.b / day_output.cloud_tint.r),
                 "weather tint did not become relatively cooler");
  context.expect(night_output.cloud_tint.b > night_output.cloud_tint.r,
                 "night cloud tint was not blue-weighted");
  context.expect(day_output.cloud_radiance.r > 0.0F
                     && night_output.cloud_radiance.b > 0.0F,
                 "lit clouds were invisible in day or night conditions");
}

void ProceduralAuroraHueIsUsedAndAttenuatedOnce(TestContext& context) {
  CloudLightingInput input = ReferenceInput();
  input.sun_elevation = -1.0F;
  input.night_factor = 1.0F;
  input.cloud_density = 0.42F;
  input.fog_transmittance = 0.73F;
  input.aurora_intrinsic_radiance = {0.21F, 0.67F, 0.39F};
  CloudLightingOutput output{};
  ExpectSucceeded(context, EvaluateCloudLighting(input, output));
  const float attenuation = output.cloud_transmittance * input.fog_transmittance;
  ExpectNear(context, output.aurora_radiance.r,
             input.aurora_intrinsic_radiance.r * attenuation, 1.0e-6F,
             "aurora red was not attenuated exactly once");
  ExpectNear(context, output.aurora_radiance.g,
             input.aurora_intrinsic_radiance.g * attenuation, 1.0e-6F,
             "aurora green was not attenuated exactly once");
  ExpectNear(context, output.aurora_radiance.b,
             input.aurora_intrinsic_radiance.b * attenuation, 1.0e-6F,
             "aurora blue was not attenuated exactly once");
  ExpectNear(context, output.aurora_radiance.r / output.aurora_radiance.g,
             input.aurora_intrinsic_radiance.r / input.aurora_intrinsic_radiance.g,
             1.0e-6F, "aurora hue ratio was reconstructed instead of preserved");

  CloudLightingInput partial_night = input;
  partial_night.night_factor = 0.5F;
  CloudLightingOutput partial_night_output{};
  ExpectSucceeded(context, EvaluateCloudLighting(partial_night, partial_night_output));
  ExpectNear(context, partial_night_output.aurora_radiance.g,
             partial_night.aurora_intrinsic_radiance.g
                 * partial_night_output.cloud_transmittance
                 * partial_night.fog_transmittance,
             1.0e-6F,
             "precomputed procedural aurora was scaled by night factor a second time");

  CloudLightingInput no_night = input;
  no_night.night_factor = 0.0F;
  CloudLightingOutput no_night_output{};
  ExpectSucceeded(context, EvaluateCloudLighting(no_night, no_night_output));
  context.expect(SameRadiance(no_night_output.aurora_radiance, {0.0F, 0.0F, 0.0F}),
                 "zero night factor did not produce exact zero aurora");

  CloudLightingInput daylight = input;
  daylight.sun_elevation = 1.0F;
  daylight.night_factor = 1.0F;
  CloudLightingOutput daylight_output{};
  ExpectSucceeded(context, EvaluateCloudLighting(daylight, daylight_output));
  context.expect(SameRadiance(daylight_output.aurora_radiance, {0.0F, 0.0F, 0.0F}),
                 "full daylight did not produce exact zero aurora");
}

void CompositionUsesOneExplicitEnergyPath(TestContext& context) {
  CloudLightingInput input = ReferenceInput();
  input.sun_elevation = -0.4F;
  input.night_factor = 0.8F;
  CloudLightingOutput output{};
  ExpectSucceeded(context, EvaluateCloudLighting(input, output));
  const float sky_attenuation = output.cloud_transmittance * input.fog_transmittance;
  const RgbRadiance expected{
      (input.sky_radiance.r * sky_attenuation)
          + (output.cloud_radiance.r * input.fog_transmittance)
          + output.aurora_radiance.r,
      (input.sky_radiance.g * sky_attenuation)
          + (output.cloud_radiance.g * input.fog_transmittance)
          + output.aurora_radiance.g,
      (input.sky_radiance.b * sky_attenuation)
          + (output.cloud_radiance.b * input.fog_transmittance)
          + output.aurora_radiance.b,
  };
  ExpectNear(context, output.composite_radiance.r, expected.r, 1.0e-6F,
             "composite red did not use the declared energy path");
  ExpectNear(context, output.composite_radiance.g, expected.g, 1.0e-6F,
             "composite green did not use the declared energy path");
  ExpectNear(context, output.composite_radiance.b, expected.b, 1.0e-6F,
             "composite blue did not use the declared energy path");
}

void ProceduralSkyAuroraFeedsLightingWithoutHueLoss(TestContext& context) {
  SkyFieldInput sky_input{
      0.6F, 0.0F, 0.8F,
      0.25F,
      0.4F, -0.25F,
      0.78F, 0.85F, 0.45F,
      1.0F, 1.0F,
  };
  SkyFieldOutput sky_output{};
  const auto sky_evaluation = EvaluateSkyFields(sky_input, sky_output);
  context.expect(sky_evaluation.status == SkyFieldStatus::evaluated,
                 "procedural sky input failed before cloud lighting");
  context.expect(sky_output.aurora_intrinsic_radiance.g > 0.0F,
                 "procedural sky fixture emitted no aurora");

  CloudLightingInput input = ReferenceInput();
  input.cloud_density = sky_output.cloud_density;
  input.cloud_detail_erosion = sky_output.cloud_detail_erosion;
  input.sun_elevation = -1.0F;
  input.night_factor = 1.0F;
  input.aurora_intrinsic_radiance = {
      sky_output.aurora_intrinsic_radiance.r,
      sky_output.aurora_intrinsic_radiance.g,
      sky_output.aurora_intrinsic_radiance.b,
  };
  CloudLightingOutput output{};
  ExpectSucceeded(context, EvaluateCloudLighting(input, output));
  const float attenuation = output.cloud_transmittance * input.fog_transmittance;
  ExpectNear(context, output.aurora_radiance.r,
             sky_output.aurora_intrinsic_radiance.r * attenuation, 1.0e-6F,
             "procedural aurora red was discarded");
  ExpectNear(context, output.aurora_radiance.g,
             sky_output.aurora_intrinsic_radiance.g * attenuation, 1.0e-6F,
             "procedural aurora green was discarded");
  ExpectNear(context, output.aurora_radiance.b,
             sky_output.aurora_intrinsic_radiance.b * attenuation, 1.0e-6F,
             "procedural aurora blue was discarded");
}

void DenseGridStaysFiniteAndBounded(TestContext& context) {
  constexpr std::array zeniths{-1.0F, -0.1F, 0.0F, 0.1F, 0.45F, 1.0F};
  constexpr std::array sun_cosines{-1.0F, -0.5F, 0.0F, 0.72F, 0.98F, 1.0F};
  constexpr std::array sun_elevations{-1.0F, -0.08F, 0.0F, 0.12F, 1.0F};
  for (const float zenith : zeniths) {
    for (const float sun_cosine : sun_cosines) {
      for (const float sun_elevation : sun_elevations) {
        for (std::uint32_t density_index = 0; density_index <= 5U; ++density_index) {
          for (std::uint32_t weather_index = 0; weather_index <= 2U; ++weather_index) {
            for (std::uint32_t night_index = 0; night_index <= 2U; ++night_index) {
              CloudLightingInput input = ReferenceInput();
              input.view_zenith_cosine = zenith;
              input.view_sun_cosine = sun_cosine;
              input.sun_elevation = sun_elevation;
              input.cloud_density = static_cast<float>(density_index) / 5.0F;
              input.cloud_detail_erosion = static_cast<float>(
                  (density_index + weather_index) % 6U) / 5.0F;
              input.weather_density = static_cast<float>(weather_index) / 2.0F;
              input.night_factor = static_cast<float>(night_index) / 2.0F;
              input.fog_transmittance = static_cast<float>(
                  (density_index + night_index) % 6U) / 5.0F;
              CloudLightingOutput output{};
              ExpectSucceeded(context, EvaluateCloudLighting(input, output));
              ExpectRange(context, output.cloud_optical_depth, 0.0F,
                          truth::render::kCloudLightingMaximumOpticalDepth,
                          "cloud optical depth exceeded its bound");
              ExpectRange(context, output.cloud_transmittance, 0.0F, 1.0F,
                          "cloud transmittance exceeded [0,1]");
              ExpectRange(context, output.forward_scattering, 0.0F,
                          truth::render::kCloudLightingMaximumForwardScattering,
                          "forward scattering exceeded its bound");
              ExpectRange(context, output.silver_lining, 0.0F, 1.0F,
                          "silver lining exceeded [0,1]");
              ExpectRange(context, output.direct_scattering, 0.0F,
                          truth::render::kCloudLightingMaximumDirectScattering,
                          "direct scattering exceeded its bound");
              ExpectRange(context, output.ambient_scattering, 0.0F, 1.0F,
                          "ambient scattering exceeded [0,1]");
              ExpectRange(context, output.multiple_scattering, 0.0F, 1.0F,
                          "multiple scattering exceeded [0,1]");
              ExpectRange(context, output.self_shadow, 0.0F, 1.0F,
                          "self shadow exceeded [0,1]");
              ExpectRange(context, output.powder_response, 0.0F, 1.0F,
                          "powder response exceeded [0,1]");
              ExpectRadianceRange(context, output.cloud_tint, 1.0F,
                                  "cloud tint exceeded [0,1]");
              ExpectRadianceRange(context, output.cloud_radiance,
                                  truth::render::kCloudLightingMaximumCloudRadiance,
                                  "cloud radiance exceeded its bound");
              ExpectRadianceRange(context, output.aurora_radiance,
                                  truth::render::kCloudLightingMaximumInputRadiance,
                                  "aurora radiance exceeded its bound");
              ExpectRadianceRange(context, output.composite_radiance,
                                  truth::render::kCloudLightingMaximumCompositeRadiance,
                                  "composite radiance exceeded its bound");
            }
          }
        }
      }
    }
  }
}

void ClampNeighborhoodsRemainContinuous(TestContext& context) {
  const auto evaluate = [&](CloudLightingInput input) {
    CloudLightingOutput output{};
    ExpectSucceeded(context, EvaluateCloudLighting(input, output));
    return output;
  };

  CloudLightingInput lower = ReferenceInput();
  lower.view_zenith_cosine = truth::render::kCloudLightingHorizonCosineFloor - 0.00001F;
  CloudLightingInput upper = lower;
  upper.view_zenith_cosine = truth::render::kCloudLightingHorizonCosineFloor + 0.00001F;
  const auto lower_horizon = evaluate(lower);
  const auto upper_horizon = evaluate(upper);
  context.expect(std::fabs(lower_horizon.cloud_transmittance
                           - upper_horizon.cloud_transmittance) < 0.001F,
                 "cloud transmittance jumped at the horizon clamp");
  context.expect(std::fabs(lower_horizon.composite_radiance.b
                           - upper_horizon.composite_radiance.b) < 0.001F,
                 "composite radiance jumped at the horizon clamp");

  lower = ReferenceInput();
  lower.sun_elevation = truth::render::kCloudLightingDaylightStart - 0.00001F;
  upper = lower;
  upper.sun_elevation = truth::render::kCloudLightingDaylightStart + 0.00001F;
  const auto lower_dawn = evaluate(lower);
  const auto upper_dawn = evaluate(upper);
  context.expect(std::fabs(lower_dawn.direct_scattering - upper_dawn.direct_scattering) < 0.001F,
                 "direct scattering jumped at the dawn clamp");

  lower = ReferenceInput();
  lower.view_sun_cosine = truth::render::kCloudLightingSilverLiningEnd - 0.00001F;
  upper = lower;
  upper.view_sun_cosine = truth::render::kCloudLightingSilverLiningEnd + 0.00001F;
  const auto lower_sun = evaluate(lower);
  const auto upper_sun = evaluate(upper);
  context.expect(std::fabs(lower_sun.silver_lining - upper_sun.silver_lining) < 0.001F,
                 "silver lining jumped at its sun clamp");
  context.expect(std::fabs(lower_sun.cloud_radiance.r - upper_sun.cloud_radiance.r) < 0.001F,
                 "cloud radiance jumped at its sun clamp");
}

using TestFunction = void (*)(TestContext&);
struct TestCase { std::string_view name; TestFunction function; };

constexpr TestCase kTests[] = {
    {"stable codes and bounds are explicit", &StableCodesAndBoundsAreExplicit},
    {"invalid input never mutates output", &InvalidInputNeverMutatesOutput},
    {"clear sky is an exact identity", &ClearSkyIsAnExactIdentity},
    {"transmittance falls monotonically with density", &TransmittanceFallsMonotonicallyWithDensity},
    {"sun and edge terms are visible and bounded", &SunAndEdgeTermsAreVisibleAndBounded},
    {"shadow powder and tints respond physically", &ShadowPowderAndTintsRespondPhysically},
    {"procedural aurora hue is used and attenuated once", &ProceduralAuroraHueIsUsedAndAttenuatedOnce},
    {"composition uses one explicit energy path", &CompositionUsesOneExplicitEnergyPath},
    {"procedural sky aurora feeds lighting without hue loss", &ProceduralSkyAuroraFeedsLightingWithoutHueLoss},
    {"dense grid stays finite and bounded", &DenseGridStaysFiniteAndBounded},
    {"clamp neighborhoods remain continuous", &ClampNeighborhoodsRemainContinuous},
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
  std::cout << "Truth cloud-lighting C++ cases: " << passed << '/' << std::size(kTests)
            << "; assertions: " << context.assertions << '\n';
  return 0;
}
