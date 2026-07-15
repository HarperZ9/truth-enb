#include "truth/render/CloudLighting.hpp"

#include <algorithm>
#include <cmath>

namespace truth::render {
namespace {

inline constexpr float kCloudOpticalDepthScale = 3.2F;
inline constexpr float kForwardAnisotropy = 0.72F;
inline constexpr float kForwardDenominatorFloor = 0.05F;

[[nodiscard]] bool InRange(const float value, const float minimum, const float maximum) noexcept {
  return value >= minimum && value <= maximum;
}

[[nodiscard]] bool IsFinite(const RgbRadiance& value) noexcept {
  return std::isfinite(value.r) && std::isfinite(value.g) && std::isfinite(value.b);
}

[[nodiscard]] bool IsRadianceInRange(
    const RgbRadiance& value,
    const float maximum) noexcept {
  return InRange(value.r, 0.0F, maximum)
      && InRange(value.g, 0.0F, maximum)
      && InRange(value.b, 0.0F, maximum);
}

[[nodiscard]] CloudLightingEvaluation Reject(
    const CloudLightingDiagnostic diagnostic) noexcept {
  return {CloudLightingStatus::rejected, diagnostic};
}

[[nodiscard]] CloudLightingDiagnostic ValidateInput(
    const CloudLightingInput& input) noexcept {
  if (!IsFinite(input.sky_radiance)) {
    return CloudLightingDiagnostic::sky_radiance_non_finite;
  }
  if (!IsRadianceInRange(input.sky_radiance, kCloudLightingMaximumInputRadiance)) {
    return CloudLightingDiagnostic::sky_radiance_out_of_range;
  }
  if (!IsFinite(input.aurora_intrinsic_radiance)) {
    return CloudLightingDiagnostic::aurora_radiance_non_finite;
  }
  if (!IsRadianceInRange(input.aurora_intrinsic_radiance,
                         kCloudLightingMaximumInputRadiance)) {
    return CloudLightingDiagnostic::aurora_radiance_out_of_range;
  }
  if (!std::isfinite(input.view_zenith_cosine)) {
    return CloudLightingDiagnostic::view_zenith_cosine_non_finite;
  }
  if (!InRange(input.view_zenith_cosine,
               kCloudLightingMinimumCosine,
               kCloudLightingMaximumCosine)) {
    return CloudLightingDiagnostic::view_zenith_cosine_out_of_range;
  }
  if (!std::isfinite(input.view_sun_cosine)) {
    return CloudLightingDiagnostic::view_sun_cosine_non_finite;
  }
  if (!InRange(input.view_sun_cosine,
               kCloudLightingMinimumCosine,
               kCloudLightingMaximumCosine)) {
    return CloudLightingDiagnostic::view_sun_cosine_out_of_range;
  }
  if (!std::isfinite(input.sun_elevation)) {
    return CloudLightingDiagnostic::sun_elevation_non_finite;
  }
  if (!InRange(input.sun_elevation,
               kCloudLightingMinimumCosine,
               kCloudLightingMaximumCosine)) {
    return CloudLightingDiagnostic::sun_elevation_out_of_range;
  }
  if (!std::isfinite(input.cloud_density)) {
    return CloudLightingDiagnostic::cloud_density_non_finite;
  }
  if (!InRange(input.cloud_density,
               kCloudLightingMinimumControl,
               kCloudLightingMaximumControl)) {
    return CloudLightingDiagnostic::cloud_density_out_of_range;
  }
  if (!std::isfinite(input.cloud_detail_erosion)) {
    return CloudLightingDiagnostic::cloud_detail_erosion_non_finite;
  }
  if (!InRange(input.cloud_detail_erosion,
               kCloudLightingMinimumControl,
               kCloudLightingMaximumControl)) {
    return CloudLightingDiagnostic::cloud_detail_erosion_out_of_range;
  }
  if (!std::isfinite(input.weather_density)) {
    return CloudLightingDiagnostic::weather_density_non_finite;
  }
  if (!InRange(input.weather_density,
               kCloudLightingMinimumControl,
               kCloudLightingMaximumControl)) {
    return CloudLightingDiagnostic::weather_density_out_of_range;
  }
  if (!std::isfinite(input.night_factor)) {
    return CloudLightingDiagnostic::night_factor_non_finite;
  }
  if (!InRange(input.night_factor,
               kCloudLightingMinimumControl,
               kCloudLightingMaximumControl)) {
    return CloudLightingDiagnostic::night_factor_out_of_range;
  }
  if (!std::isfinite(input.fog_transmittance)) {
    return CloudLightingDiagnostic::fog_transmittance_non_finite;
  }
  if (!InRange(input.fog_transmittance,
               kCloudLightingMinimumControl,
               kCloudLightingMaximumControl)) {
    return CloudLightingDiagnostic::fog_transmittance_out_of_range;
  }
  return CloudLightingDiagnostic::none;
}

[[nodiscard]] float Smooth(const float value) noexcept {
  return value * value * (3.0F - (2.0F * value));
}

[[nodiscard]] float SmoothStep(
    const float lower,
    const float upper,
    const float value) noexcept {
  return Smooth(std::clamp((value - lower) / (upper - lower), 0.0F, 1.0F));
}

[[nodiscard]] float LinearInterpolate(
    const float lhs,
    const float rhs,
    const float amount) noexcept {
  return lhs + ((rhs - lhs) * amount);
}

[[nodiscard]] RgbRadiance Scale(const RgbRadiance& value, const float scale) noexcept {
  return {value.r * scale, value.g * scale, value.b * scale};
}

[[nodiscard]] RgbRadiance Add(
    const RgbRadiance& first,
    const RgbRadiance& second,
    const RgbRadiance& third) noexcept {
  return {
      first.r + second.r + third.r,
      first.g + second.g + third.g,
      first.b + second.b + third.b,
  };
}

[[nodiscard]] bool ValidateOutput(const CloudLightingOutput& output) noexcept {
  return std::isfinite(output.cloud_optical_depth)
      && std::isfinite(output.cloud_transmittance)
      && std::isfinite(output.forward_scattering)
      && std::isfinite(output.silver_lining)
      && std::isfinite(output.direct_scattering)
      && std::isfinite(output.ambient_scattering)
      && std::isfinite(output.multiple_scattering)
      && std::isfinite(output.self_shadow)
      && std::isfinite(output.powder_response)
      && IsFinite(output.cloud_tint)
      && IsFinite(output.cloud_radiance)
      && IsFinite(output.aurora_radiance)
      && IsFinite(output.composite_radiance);
}

[[nodiscard]] bool OutputIsInRange(const CloudLightingOutput& output) noexcept {
  return InRange(output.cloud_optical_depth, 0.0F, kCloudLightingMaximumOpticalDepth)
      && InRange(output.cloud_transmittance, 0.0F, 1.0F)
      && InRange(output.forward_scattering,
                 0.0F,
                 kCloudLightingMaximumForwardScattering)
      && InRange(output.silver_lining, 0.0F, 1.0F)
      && InRange(output.direct_scattering,
                 0.0F,
                 kCloudLightingMaximumDirectScattering)
      && InRange(output.ambient_scattering, 0.0F, 1.0F)
      && InRange(output.multiple_scattering, 0.0F, 1.0F)
      && InRange(output.self_shadow, 0.0F, 1.0F)
      && InRange(output.powder_response, 0.0F, 1.0F)
      && IsRadianceInRange(output.cloud_tint, 1.0F)
      && IsRadianceInRange(output.cloud_radiance,
                           kCloudLightingMaximumCloudRadiance)
      && IsRadianceInRange(output.aurora_radiance,
                           kCloudLightingMaximumInputRadiance)
      && IsRadianceInRange(output.composite_radiance,
                           kCloudLightingMaximumCompositeRadiance);
}

}  // namespace

CloudLightingEvaluation EvaluateCloudLighting(
    const CloudLightingInput& input,
    CloudLightingOutput& output) noexcept {
  const CloudLightingDiagnostic diagnostic = ValidateInput(input);
  if (diagnostic != CloudLightingDiagnostic::none) {
    return Reject(diagnostic);
  }

  const float path_cosine = std::max(input.view_zenith_cosine,
                                     kCloudLightingHorizonCosineFloor);
  const float air_mass = 1.0F / path_cosine;
  const float weather_extinction = LinearInterpolate(0.70F, 1.30F,
                                                      input.weather_density);

  CloudLightingOutput candidate{};
  if (input.cloud_density == 0.0F) {
    candidate.cloud_optical_depth = 0.0F;
    candidate.cloud_transmittance = 1.0F;
  } else {
    candidate.cloud_optical_depth = std::min(
        input.cloud_density * kCloudOpticalDepthScale * air_mass * weather_extinction,
        kCloudLightingMaximumOpticalDepth);
    candidate.cloud_transmittance = std::exp(-candidate.cloud_optical_depth);
  }

  const float daylight = SmoothStep(kCloudLightingDaylightStart,
                                    kCloudLightingDaylightEnd,
                                    input.sun_elevation);
  constexpr float anisotropy_squared = kForwardAnisotropy * kForwardAnisotropy;
  const float forward_denominator = std::max(
      1.0F + anisotropy_squared
          - (2.0F * kForwardAnisotropy * input.view_sun_cosine),
      kForwardDenominatorFloor);
  candidate.forward_scattering = std::min(
      0.25F * (1.0F - anisotropy_squared)
          / (forward_denominator * std::sqrt(forward_denominator)),
      kCloudLightingMaximumForwardScattering);

  const float sun_edge = SmoothStep(kCloudLightingSilverLiningStart,
                                    kCloudLightingSilverLiningEnd,
                                    input.view_sun_cosine);
  const float detail_edge = LinearInterpolate(0.35F,
                                               1.0F,
                                               input.cloud_detail_erosion);
  const float density_edge = 1.0F - (0.45F * input.cloud_density);
  candidate.silver_lining = std::clamp(sun_edge * detail_edge * density_edge,
                                       0.0F,
                                       1.0F);
  candidate.direct_scattering = std::clamp(
      daylight * ((0.32F * candidate.forward_scattering)
                  + (1.35F * candidate.silver_lining)),
      0.0F,
      kCloudLightingMaximumDirectScattering);

  candidate.self_shadow = std::exp(-0.62F * candidate.cloud_optical_depth);
  candidate.powder_response = 1.0F - std::exp(
      -2.4F * input.cloud_density * LinearInterpolate(0.85F, 1.15F,
                                                       input.weather_density));
  candidate.multiple_scattering = std::clamp(
      0.055F + (0.22F * candidate.powder_response
                * (1.0F - (0.35F * input.weather_density))),
      0.0F,
      1.0F);
  const float ambient_base = LinearInterpolate(
      0.07F + (0.05F * input.night_factor),
      0.22F,
      daylight);
  candidate.ambient_scattering = std::clamp(ambient_base + candidate.multiple_scattering,
                                            0.0F,
                                            1.0F);

  const RgbRadiance night_tint{0.18F, 0.28F, 0.52F};
  const RgbRadiance day_tint{1.00F, 0.92F, 0.78F};
  const RgbRadiance base_tint{
      LinearInterpolate(night_tint.r, day_tint.r, daylight),
      LinearInterpolate(night_tint.g, day_tint.g, daylight),
      LinearInterpolate(night_tint.b, day_tint.b, daylight),
  };
  candidate.cloud_tint = {
      base_tint.r * LinearInterpolate(1.0F, 0.62F, input.weather_density),
      base_tint.g * LinearInterpolate(1.0F, 0.72F, input.weather_density),
      base_tint.b * LinearInterpolate(1.0F, 0.88F, input.weather_density),
  };

  const float cloud_opacity = 1.0F - candidate.cloud_transmittance;
  const float direct_visibility = candidate.self_shadow
      * LinearInterpolate(0.55F, 1.0F, candidate.powder_response);
  const float lighting = std::min(
      candidate.ambient_scattering
          + (candidate.direct_scattering * direct_visibility),
      kCloudLightingMaximumCloudRadiance);
  candidate.cloud_radiance = {
      candidate.cloud_tint.r * cloud_opacity * lighting,
      candidate.cloud_tint.g * cloud_opacity * lighting,
      candidate.cloud_tint.b * cloud_opacity * lighting,
  };

  const float combined_transmittance = candidate.cloud_transmittance
      * input.fog_transmittance;
  const float aurora_transmittance = (input.night_factor == 0.0F || daylight >= 1.0F)
      ? 0.0F
      : combined_transmittance;
  candidate.aurora_radiance = Scale(input.aurora_intrinsic_radiance,
                                    aurora_transmittance);
  candidate.composite_radiance = Add(
      Scale(input.sky_radiance, combined_transmittance),
      Scale(candidate.cloud_radiance, input.fog_transmittance),
      candidate.aurora_radiance);

  if (!ValidateOutput(candidate)) {
    return Reject(CloudLightingDiagnostic::calculation_non_finite);
  }
  if (!OutputIsInRange(candidate)) {
    return Reject(CloudLightingDiagnostic::calculation_out_of_range);
  }

  output = candidate;
  return {CloudLightingStatus::evaluated, CloudLightingDiagnostic::none};
}

}  // namespace truth::render
