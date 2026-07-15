#pragma once

#include <cstdint>

namespace truth::render {

inline constexpr float kAuroraMinimumCameraCoordinate = -4096.0F;
inline constexpr float kAuroraMaximumCameraCoordinate = 4096.0F;
inline constexpr float kAuroraMinimumDirection = -1.0F;
inline constexpr float kAuroraMaximumDirection = 1.0F;
inline constexpr float kAuroraDirectionLengthTolerance = 0.0025F;
inline constexpr float kAuroraMinimumPhase = 0.0F;
inline constexpr float kAuroraMaximumPhase = 1.0F;
inline constexpr float kAuroraMinimumWind = -1.0F;
inline constexpr float kAuroraMaximumWind = 1.0F;
inline constexpr float kAuroraMinimumControl = 0.0F;
inline constexpr float kAuroraMaximumControl = 1.0F;
inline constexpr float kAuroraBaseHeight = 8.0F;
inline constexpr float kAuroraTopHeight = 16.0F;
inline constexpr float kAuroraMaximumRayDistance = 180.0F;

enum class AuroraQuality : std::uint32_t {
  fallback = 0U,
  low = 1U,
  balanced = 2U,
  high = 3U,
};

[[nodiscard]] constexpr std::uint32_t AuroraSampleCount(
    const AuroraQuality quality) noexcept {
  switch (quality) {
    case AuroraQuality::fallback:
      return 1U;
    case AuroraQuality::low:
      return 4U;
    case AuroraQuality::balanced:
      return 7U;
    case AuroraQuality::high:
      return 10U;
  }
  return 0U;
}

struct AuroraRadiance {
  float r;
  float g;
  float b;
};

struct AuroraDepositionProfile {
  float r;
  float g;
  float b;
};

struct AuroraCurtainInput {
  float camera_x;
  float camera_y;
  float camera_z;
  float view_x;
  float view_y;
  float view_z;
  float phase;
  float wind_x;
  float wind_y;
  float activity;
  float night_factor;
  AuroraQuality quality;
};

struct AuroraCurtainOutput {
  float mask;
  AuroraRadiance intrinsic_radiance;
  std::uint32_t samples;
};

enum class AuroraCurtainStatus : std::uint32_t {
  evaluated = 0U,
  rejected = 1U,
};

enum class AuroraCurtainDiagnostic : std::uint32_t {
  none = 0U,
  camera_x_non_finite = 100U,
  camera_x_out_of_range = 101U,
  camera_y_non_finite = 110U,
  camera_y_out_of_range = 111U,
  camera_z_non_finite = 120U,
  camera_z_out_of_range = 121U,
  view_x_non_finite = 130U,
  view_x_out_of_range = 131U,
  view_y_non_finite = 140U,
  view_y_out_of_range = 141U,
  view_z_non_finite = 150U,
  view_z_out_of_range = 151U,
  view_direction_not_normalized = 160U,
  phase_non_finite = 170U,
  phase_out_of_range = 171U,
  wind_x_non_finite = 180U,
  wind_x_out_of_range = 181U,
  wind_y_non_finite = 190U,
  wind_y_out_of_range = 191U,
  activity_non_finite = 200U,
  activity_out_of_range = 201U,
  night_factor_non_finite = 210U,
  night_factor_out_of_range = 211U,
  quality_invalid = 250U,
  calculation_non_finite = 300U,
  calculation_out_of_range = 301U,
};

struct AuroraCurtainEvaluation {
  AuroraCurtainStatus status;
  AuroraCurtainDiagnostic diagnostic;
};

// Normalized-height artist scale. Green and blue share the lower deposition
// band while the red profile is intentionally higher and broader.
[[nodiscard]] AuroraDepositionProfile EvaluateAuroraDepositionProfile(
    float normalized_height) noexcept;

[[nodiscard]] AuroraCurtainEvaluation EvaluateAuroraCurtain(
    const AuroraCurtainInput& input,
    AuroraCurtainOutput& output) noexcept;

}  // namespace truth::render
