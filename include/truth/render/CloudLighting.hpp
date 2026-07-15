#pragma once

#include "truth/render/Atmosphere.hpp"

#include <cstdint>

namespace truth::render {

inline constexpr float kCloudLightingMinimumCosine = -1.0F;
inline constexpr float kCloudLightingMaximumCosine = 1.0F;
inline constexpr float kCloudLightingMinimumControl = 0.0F;
inline constexpr float kCloudLightingMaximumControl = 1.0F;
inline constexpr float kCloudLightingMaximumInputRadiance = 64.0F;
inline constexpr float kCloudLightingHorizonCosineFloor = 0.10F;
inline constexpr float kCloudLightingDaylightStart = -0.08F;
inline constexpr float kCloudLightingDaylightEnd = 0.12F;
inline constexpr float kCloudLightingSilverLiningStart = 0.72F;
inline constexpr float kCloudLightingSilverLiningEnd = 0.98F;
inline constexpr float kCloudLightingMaximumOpticalDepth = 42.0F;
inline constexpr float kCloudLightingMaximumForwardScattering = 6.0F;
inline constexpr float kCloudLightingMaximumDirectScattering = 6.0F;
inline constexpr float kCloudLightingMaximumCloudRadiance = 6.0F;
inline constexpr float kCloudLightingMaximumCompositeRadiance = 134.0F;

struct CloudLightingInput {
  RgbRadiance sky_radiance;
  RgbRadiance aurora_intrinsic_radiance;
  float view_zenith_cosine;
  float view_sun_cosine;
  float sun_elevation;
  float cloud_density;
  float cloud_detail_erosion;
  float weather_density;
  float night_factor;
  float fog_transmittance;
};

struct CloudLightingOutput {
  float cloud_optical_depth;
  float cloud_transmittance;
  float forward_scattering;
  float silver_lining;
  float direct_scattering;
  float ambient_scattering;
  float multiple_scattering;
  float self_shadow;
  float powder_response;
  RgbRadiance cloud_tint;
  RgbRadiance cloud_radiance;
  RgbRadiance aurora_radiance;
  RgbRadiance composite_radiance;
};

enum class CloudLightingStatus : std::uint32_t {
  evaluated = 0U,
  rejected = 1U,
};

enum class CloudLightingDiagnostic : std::uint32_t {
  none = 0U,
  sky_radiance_non_finite = 100U,
  sky_radiance_out_of_range = 101U,
  aurora_radiance_non_finite = 110U,
  aurora_radiance_out_of_range = 111U,
  view_zenith_cosine_non_finite = 120U,
  view_zenith_cosine_out_of_range = 121U,
  view_sun_cosine_non_finite = 130U,
  view_sun_cosine_out_of_range = 131U,
  sun_elevation_non_finite = 140U,
  sun_elevation_out_of_range = 141U,
  cloud_density_non_finite = 150U,
  cloud_density_out_of_range = 151U,
  cloud_detail_erosion_non_finite = 160U,
  cloud_detail_erosion_out_of_range = 161U,
  weather_density_non_finite = 170U,
  weather_density_out_of_range = 171U,
  night_factor_non_finite = 180U,
  night_factor_out_of_range = 181U,
  fog_transmittance_non_finite = 190U,
  fog_transmittance_out_of_range = 191U,
  calculation_non_finite = 300U,
  calculation_out_of_range = 301U,
};

struct CloudLightingEvaluation {
  CloudLightingStatus status;
  CloudLightingDiagnostic diagnostic;
};

[[nodiscard]] CloudLightingEvaluation EvaluateCloudLighting(
    const CloudLightingInput& input,
    CloudLightingOutput& output) noexcept;

}  // namespace truth::render
