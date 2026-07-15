#include "truth/render/Atmosphere.hpp"

#include <algorithm>
#include <cmath>

namespace truth::render {
namespace {

[[nodiscard]] bool InRange(const float value, const float minimum, const float maximum) noexcept {
  return value >= minimum && value <= maximum;
}

[[nodiscard]] AtmosphereEvaluation Reject(const AtmosphereDiagnostic diagnostic) noexcept {
  return {AtmosphereStatus::rejected, diagnostic};
}

[[nodiscard]] AtmosphereDiagnostic ValidateInput(const AtmosphereInput& input) noexcept {
  if (!std::isfinite(input.view_zenith_cosine)) {
    return AtmosphereDiagnostic::view_zenith_cosine_non_finite;
  }
  if (!InRange(input.view_zenith_cosine, kAtmosphereMinimumCosine, kAtmosphereMaximumCosine)) {
    return AtmosphereDiagnostic::view_zenith_cosine_out_of_range;
  }
  if (!std::isfinite(input.view_sun_cosine)) {
    return AtmosphereDiagnostic::view_sun_cosine_non_finite;
  }
  if (!InRange(input.view_sun_cosine, kAtmosphereMinimumCosine, kAtmosphereMaximumCosine)) {
    return AtmosphereDiagnostic::view_sun_cosine_out_of_range;
  }
  if (!std::isfinite(input.sun_elevation)) {
    return AtmosphereDiagnostic::sun_elevation_non_finite;
  }
  if (!InRange(input.sun_elevation, kAtmosphereMinimumCosine, kAtmosphereMaximumCosine)) {
    return AtmosphereDiagnostic::sun_elevation_out_of_range;
  }
  if (!std::isfinite(input.weather_density)) {
    return AtmosphereDiagnostic::weather_density_non_finite;
  }
  if (!InRange(input.weather_density, kAtmosphereMinimumControl, kAtmosphereMaximumControl)) {
    return AtmosphereDiagnostic::weather_density_out_of_range;
  }
  if (!std::isfinite(input.cloud_coverage)) {
    return AtmosphereDiagnostic::cloud_coverage_non_finite;
  }
  if (!InRange(input.cloud_coverage, kAtmosphereMinimumControl, kAtmosphereMaximumControl)) {
    return AtmosphereDiagnostic::cloud_coverage_out_of_range;
  }
  if (!std::isfinite(input.cloud_density)) {
    return AtmosphereDiagnostic::cloud_density_non_finite;
  }
  if (!InRange(input.cloud_density, kAtmosphereMinimumControl, kAtmosphereMaximumControl)) {
    return AtmosphereDiagnostic::cloud_density_out_of_range;
  }
  if (!std::isfinite(input.fog_density)) {
    return AtmosphereDiagnostic::fog_density_non_finite;
  }
  if (!InRange(input.fog_density, kAtmosphereMinimumControl, kAtmosphereMaximumControl)) {
    return AtmosphereDiagnostic::fog_density_out_of_range;
  }
  if (!std::isfinite(input.aurora_activity)) {
    return AtmosphereDiagnostic::aurora_activity_non_finite;
  }
  if (!InRange(input.aurora_activity, kAtmosphereMinimumControl, kAtmosphereMaximumControl)) {
    return AtmosphereDiagnostic::aurora_activity_out_of_range;
  }
  if (!std::isfinite(input.aurora_mask)) {
    return AtmosphereDiagnostic::aurora_mask_non_finite;
  }
  if (!InRange(input.aurora_mask, kAtmosphereMinimumControl, kAtmosphereMaximumControl)) {
    return AtmosphereDiagnostic::aurora_mask_out_of_range;
  }
  if (!std::isfinite(input.night_factor)) {
    return AtmosphereDiagnostic::night_factor_non_finite;
  }
  if (!InRange(input.night_factor, kAtmosphereMinimumControl, kAtmosphereMaximumControl)) {
    return AtmosphereDiagnostic::night_factor_out_of_range;
  }
  return AtmosphereDiagnostic::none;
}

[[nodiscard]] bool IsFinite(const RgbRadiance& value) noexcept {
  return std::isfinite(value.r) && std::isfinite(value.g) && std::isfinite(value.b);
}

[[nodiscard]] bool IsNonnegative(const RgbRadiance& value) noexcept {
  return value.r >= 0.0F && value.g >= 0.0F && value.b >= 0.0F;
}

[[nodiscard]] RgbRadiance Scale(const RgbRadiance& value, const float scale) noexcept {
  return {value.r * scale, value.g * scale, value.b * scale};
}

[[nodiscard]] RgbRadiance Add(const RgbRadiance& lhs, const RgbRadiance& rhs) noexcept {
  return {lhs.r + rhs.r, lhs.g + rhs.g, lhs.b + rhs.b};
}

}  // namespace

float RayleighPhase(const float view_sun_cosine) noexcept {
  if (!std::isfinite(view_sun_cosine)) {
    return 0.0F;
  }
  const float cosine = std::clamp(view_sun_cosine,
                                  kAtmosphereMinimumCosine,
                                  kAtmosphereMaximumCosine);
  return kAtmosphereRayleighScale * (1.0F + (cosine * cosine));
}

float MiePhase(const float view_sun_cosine) noexcept {
  if (!std::isfinite(view_sun_cosine)) {
    return 0.0F;
  }
  const float cosine = std::clamp(view_sun_cosine,
                                  kAtmosphereMinimumCosine,
                                  kAtmosphereMaximumCosine);
  constexpr float g_squared = kAtmosphereMieAnisotropy * kAtmosphereMieAnisotropy;
  const float denominator = std::max(1.0F + g_squared
                                         - (2.0F * kAtmosphereMieAnisotropy * cosine),
                                     kAtmosphereMieDenominatorFloor);
  const float phase = (1.0F - g_squared) / (denominator * std::sqrt(denominator));
  return std::min(phase, kAtmosphereMiePhaseMaximum);
}

AtmosphereEvaluation EvaluateAtmosphere(
    const AtmosphereInput& input,
    AtmosphereOutput& output) noexcept {
  const AtmosphereDiagnostic diagnostic = ValidateInput(input);
  if (diagnostic != AtmosphereDiagnostic::none) {
    return Reject(diagnostic);
  }

  const float rayleigh_phase = RayleighPhase(input.view_sun_cosine);
  const float mie_phase = MiePhase(input.view_sun_cosine);
  const float path_cosine = std::max(input.view_zenith_cosine,
                                     kAtmosphereHorizonCosineFloor);
  const float air_mass = 1.0F / path_cosine;
  const float horizon_boost = 1.0F + (0.04F * (air_mass - 1.0F));
  const float daylight = std::clamp((input.sun_elevation + 0.1F) / 0.2F, 0.0F, 1.0F);
  const float weather_attenuation = 1.0F - (0.55F * input.weather_density);

  AtmosphereOutput candidate{};
  candidate.sky_radiance = {
      ((daylight * ((0.18F * rayleigh_phase) + (0.035F * mie_phase)) * horizon_boost)
       + (0.00225F * input.night_factor)) * weather_attenuation,
      ((daylight * ((0.28F * rayleigh_phase) + (0.025F * mie_phase)) * horizon_boost)
       + (0.00525F * input.night_factor)) * weather_attenuation,
      ((daylight * ((0.52F * rayleigh_phase) + (0.015F * mie_phase)) * horizon_boost)
       + (0.01350F * input.night_factor)) * weather_attenuation,
  };

  const float cloud_optical_depth = kAtmosphereCloudOpticalDepthScale
      * input.cloud_coverage
      * input.cloud_density
      * (0.35F + (0.65F * input.weather_density));
  candidate.cloud_transmittance = std::exp(-cloud_optical_depth);

  const float fog_path = 1.0F + (0.15F * (air_mass - 1.0F));
  const float fog_optical_depth = kAtmosphereFogOpticalDepthScale * input.fog_density * fog_path;
  candidate.fog_transmittance = std::exp(-fog_optical_depth);

  const float aurora_view = 0.35F
      + (0.65F * std::clamp(input.view_zenith_cosine, 0.0F, 1.0F));
  const float aurora_strength = input.aurora_activity
      * input.aurora_mask
      * input.night_factor
      * aurora_view;
  const RgbRadiance intrinsic_aurora{
      0.10F * aurora_strength,
      0.80F * aurora_strength,
      0.55F * aurora_strength,
  };

  const float attenuation = candidate.cloud_transmittance * candidate.fog_transmittance;
  candidate.aurora_radiance = Scale(intrinsic_aurora, attenuation);
  candidate.composite_radiance = Add(Scale(candidate.sky_radiance, attenuation),
                                     candidate.aurora_radiance);

  if (!IsFinite(candidate.sky_radiance)
      || !std::isfinite(candidate.cloud_transmittance)
      || !std::isfinite(candidate.fog_transmittance)
      || !IsFinite(candidate.aurora_radiance)
      || !IsFinite(candidate.composite_radiance)) {
    return Reject(AtmosphereDiagnostic::calculation_non_finite);
  }
  if (!IsNonnegative(candidate.sky_radiance)
      || !InRange(candidate.cloud_transmittance, 0.0F, 1.0F)
      || !InRange(candidate.fog_transmittance, 0.0F, 1.0F)
      || !IsNonnegative(candidate.aurora_radiance)
      || !IsNonnegative(candidate.composite_radiance)) {
    return Reject(AtmosphereDiagnostic::calculation_out_of_range);
  }

  output = candidate;
  return {AtmosphereStatus::evaluated, AtmosphereDiagnostic::none};
}

}  // namespace truth::render
