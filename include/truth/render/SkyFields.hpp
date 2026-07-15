#pragma once

#include <cstdint>

namespace truth::render {

inline constexpr float kSkyFieldMinimumDirection = -1.0F;
inline constexpr float kSkyFieldMaximumDirection = 1.0F;
inline constexpr float kSkyFieldDirectionLengthTolerance = 0.0025F;
inline constexpr float kSkyFieldMinimumPhase = 0.0F;
inline constexpr float kSkyFieldMaximumPhase = 1.0F;
inline constexpr float kSkyFieldMinimumWind = -1.0F;
inline constexpr float kSkyFieldMaximumWind = 1.0F;
inline constexpr float kSkyFieldMinimumControl = 0.0F;
inline constexpr float kSkyFieldMaximumControl = 1.0F;

struct SkyFieldRadiance {
  float r;
  float g;
  float b;
};

struct SkyFieldInput {
  float view_x;
  float view_y;
  float view_z;
  float phase;
  float wind_x;
  float wind_y;
  float cloud_coverage;
  float cloud_density;
  float weather_density;
  float aurora_activity;
  float night_factor;
};

struct SkyFieldOutput {
  float cloud_body;
  float cloud_detail_erosion;
  float cloud_density;
  float aurora_mask;
  SkyFieldRadiance aurora_intrinsic_radiance;
};

enum class SkyFieldStatus : std::uint32_t {
  evaluated = 0U,
  rejected = 1U,
};

enum class SkyFieldDiagnostic : std::uint32_t {
  none = 0U,
  view_x_non_finite = 100U,
  view_x_out_of_range = 101U,
  view_y_non_finite = 110U,
  view_y_out_of_range = 111U,
  view_z_non_finite = 120U,
  view_z_out_of_range = 121U,
  view_direction_not_normalized = 130U,
  phase_non_finite = 140U,
  phase_out_of_range = 141U,
  wind_x_non_finite = 150U,
  wind_x_out_of_range = 151U,
  wind_y_non_finite = 160U,
  wind_y_out_of_range = 161U,
  cloud_coverage_non_finite = 170U,
  cloud_coverage_out_of_range = 171U,
  cloud_density_non_finite = 180U,
  cloud_density_out_of_range = 181U,
  weather_density_non_finite = 190U,
  weather_density_out_of_range = 191U,
  aurora_activity_non_finite = 200U,
  aurora_activity_out_of_range = 201U,
  night_factor_non_finite = 210U,
  night_factor_out_of_range = 211U,
  calculation_non_finite = 300U,
  calculation_out_of_range = 301U,
};

struct SkyFieldEvaluation {
  SkyFieldStatus status;
  SkyFieldDiagnostic diagnostic;
};

[[nodiscard]] SkyFieldEvaluation EvaluateSkyFields(
    const SkyFieldInput& input,
    SkyFieldOutput& output) noexcept;

}  // namespace truth::render
