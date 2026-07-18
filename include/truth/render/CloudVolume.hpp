#pragma once

#include <cstdint>

namespace truth::render {

inline constexpr float kCloudVolumeMaximumRadiance = 8.0F;
inline constexpr float kCloudVolumeMaximumOpticalDepth = 64.0F;

struct CloudVolumeVector {
  float x;
  float y;
  float z;
};

struct CloudVolumeLayerIntersection {
  float near_distance;
  float far_distance;
};

enum class CloudVolumeQuality : std::uint32_t {
  performance = 0U,
  balanced = 1U,
  quality = 2U,
  ultra = 3U,
  cinematic = 4U,
};

struct CloudVolumeInput {
  CloudVolumeVector camera_position;
  CloudVolumeVector view_direction;
  CloudVolumeVector sun_direction;
  float cloud_base_height;
  float cloud_top_height;
  float max_distance;
  float phase;
  float wind_x;
  float wind_y;
  float cloud_coverage;
  float cloud_density;
  float weather_density;
  float cloud_type;
  float night_factor;
  std::uint32_t pixel_x;
  std::uint32_t pixel_y;
  std::uint32_t jitter_frame;
  CloudVolumeQuality quality;
};

struct CloudVolumeOutput {
  CloudVolumeVector radiance;
  float transmittance;
  float optical_depth;
  std::uint32_t primary_steps;
  std::uint32_t light_samples;
};

enum class CloudVolumeStatus : std::uint32_t {
  evaluated = 0U,
  rejected = 1U,
};

enum class CloudVolumeDiagnostic : std::uint32_t {
  none = 0U,
  camera_non_finite = 100U,
  camera_out_of_range = 101U,
  view_direction_non_finite = 110U,
  view_direction_out_of_range = 111U,
  view_direction_not_normalized = 112U,
  sun_direction_non_finite = 120U,
  sun_direction_out_of_range = 121U,
  sun_direction_not_normalized = 122U,
  phase_non_finite = 130U,
  phase_out_of_range = 131U,
  wind_non_finite = 140U,
  wind_out_of_range = 141U,
  control_non_finite = 150U,
  control_out_of_range = 151U,
  layer_non_finite = 160U,
  layer_out_of_range = 161U,
  max_distance_non_finite = 170U,
  max_distance_out_of_range = 171U,
  quality_out_of_range = 180U,
  calculation_non_finite = 300U,
  calculation_out_of_range = 301U,
};

struct CloudVolumeEvaluation {
  CloudVolumeStatus status;
  CloudVolumeDiagnostic diagnostic;
};

[[nodiscard]] constexpr std::uint32_t CloudVolumePrimaryStepBudget(
    const CloudVolumeQuality quality) noexcept {
  switch (quality) {
    case CloudVolumeQuality::performance:
      return 0U;
    case CloudVolumeQuality::balanced:
      return 0U;
    case CloudVolumeQuality::quality:
      return 8U;
    case CloudVolumeQuality::ultra:
      return 12U;
    case CloudVolumeQuality::cinematic:
      return 16U;
  }
  return 0U;
}

[[nodiscard]] constexpr std::uint32_t CloudVolumeLightStepBudget(
    const CloudVolumeQuality quality) noexcept {
  switch (quality) {
    case CloudVolumeQuality::performance:
      return 0U;
    case CloudVolumeQuality::balanced:
      return 0U;
    case CloudVolumeQuality::quality:
      return 2U;
    case CloudVolumeQuality::ultra:
      return 3U;
    case CloudVolumeQuality::cinematic:
      return 4U;
  }
  return 0U;
}

[[nodiscard]] bool IntersectCloudVolumeLayer(
    float camera_height,
    float view_vertical,
    float cloud_base_height,
    float cloud_top_height,
    float max_distance,
    CloudVolumeLayerIntersection& intersection) noexcept;

[[nodiscard]] float CloudVolumeInterleavedJitter(
    std::uint32_t pixel_x,
    std::uint32_t pixel_y,
    std::uint32_t frame) noexcept;

[[nodiscard]] float CloudVolumeVerticalProfile(
    float normalized_height,
    float cloud_type) noexcept;

[[nodiscard]] CloudVolumeEvaluation SampleCloudVolumeDensity(
    const CloudVolumeInput& input,
    const CloudVolumeVector& position,
    float& density) noexcept;

[[nodiscard]] CloudVolumeEvaluation EvaluateCloudVolume(
    const CloudVolumeInput& input,
    CloudVolumeOutput& output) noexcept;

}  // namespace truth::render
