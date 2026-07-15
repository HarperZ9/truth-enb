#pragma once

#include <cstdint>

namespace truth::render {

inline constexpr float kAtmosphereMinimumCosine = -1.0F;
inline constexpr float kAtmosphereMaximumCosine = 1.0F;
inline constexpr float kAtmosphereMinimumControl = 0.0F;
inline constexpr float kAtmosphereMaximumControl = 1.0F;
inline constexpr float kAtmosphereRayleighScale = 0.75F;
inline constexpr float kAtmosphereMieAnisotropy = 0.65F;
inline constexpr float kAtmosphereMieDenominatorFloor = 0.05F;
inline constexpr float kAtmosphereMiePhaseMaximum = 12.0F;
inline constexpr float kAtmosphereHorizonCosineFloor = 0.05F;
inline constexpr float kAtmosphereCloudOpticalDepthScale = 4.0F;
inline constexpr float kAtmosphereFogOpticalDepthScale = 3.0F;

struct RgbRadiance {
  float r;
  float g;
  float b;
};

struct AtmosphereInput {
  float view_zenith_cosine;
  float view_sun_cosine;
  float sun_elevation;
  float weather_density;
  float cloud_coverage;
  float cloud_density;
  float fog_density;
  float aurora_activity;
  float aurora_mask;
  float night_factor;
};

struct AtmosphereOutput {
  RgbRadiance sky_radiance;
  float cloud_transmittance;
  float fog_transmittance;
  RgbRadiance aurora_radiance;
  RgbRadiance composite_radiance;
};

enum class AtmosphereStatus : std::uint32_t {
  evaluated = 0U,
  rejected = 1U,
};

enum class AtmosphereDiagnostic : std::uint32_t {
  none = 0U,
  view_zenith_cosine_non_finite = 100U,
  view_zenith_cosine_out_of_range = 101U,
  view_sun_cosine_non_finite = 110U,
  view_sun_cosine_out_of_range = 111U,
  sun_elevation_non_finite = 120U,
  sun_elevation_out_of_range = 121U,
  weather_density_non_finite = 130U,
  weather_density_out_of_range = 131U,
  cloud_coverage_non_finite = 140U,
  cloud_coverage_out_of_range = 141U,
  cloud_density_non_finite = 150U,
  cloud_density_out_of_range = 151U,
  fog_density_non_finite = 160U,
  fog_density_out_of_range = 161U,
  aurora_activity_non_finite = 170U,
  aurora_activity_out_of_range = 171U,
  aurora_mask_non_finite = 180U,
  aurora_mask_out_of_range = 181U,
  night_factor_non_finite = 190U,
  night_factor_out_of_range = 191U,
  calculation_non_finite = 200U,
  calculation_out_of_range = 201U,
};

struct AtmosphereEvaluation {
  AtmosphereStatus status;
  AtmosphereDiagnostic diagnostic;
};

[[nodiscard]] float RayleighPhase(float view_sun_cosine) noexcept;
[[nodiscard]] float MiePhase(float view_sun_cosine) noexcept;
[[nodiscard]] AtmosphereEvaluation EvaluateAtmosphere(
    const AtmosphereInput& input,
    AtmosphereOutput& output) noexcept;

}  // namespace truth::render
