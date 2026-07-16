#include "truth/render/SkyViewAdapter.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace truth::render {
namespace {

[[nodiscard]] bool InRange(
    const float value,
    const float minimum,
    const float maximum) noexcept {
  return value >= minimum && value <= maximum;
}

[[nodiscard]] SkyViewAdapterEvaluation Reject(
    const SkyViewAdapterDiagnostic diagnostic) noexcept {
  return {SkyViewAdapterStatus::rejected, diagnostic};
}

[[nodiscard]] bool WorldCoordinate(const float value) noexcept {
  return InRange(value,
                 kSkyViewMinimumWorldCoordinate,
                 kSkyViewMaximumWorldCoordinate);
}

[[nodiscard]] bool MatrixIsFinite(const SkyViewMatrix& matrix) noexcept {
  const std::array values{
      matrix.m00, matrix.m01, matrix.m02, matrix.m03,
      matrix.m10, matrix.m11, matrix.m12, matrix.m13,
      matrix.m20, matrix.m21, matrix.m22, matrix.m23,
      matrix.m30, matrix.m31, matrix.m32, matrix.m33,
  };
  return std::all_of(values.begin(), values.end(), [](const float value) {
    return std::isfinite(value);
  });
}

[[nodiscard]] SkyViewAdapterDiagnostic ValidateInput(
    const SkyViewAdapterInput& input) noexcept {
  if (!std::isfinite(input.texcoord_x)) {
    return SkyViewAdapterDiagnostic::texcoord_x_non_finite;
  }
  if (!InRange(input.texcoord_x,
               kSkyViewMinimumTexcoord,
               kSkyViewMaximumTexcoord)) {
    return SkyViewAdapterDiagnostic::texcoord_x_out_of_range;
  }
  if (!std::isfinite(input.texcoord_y)) {
    return SkyViewAdapterDiagnostic::texcoord_y_non_finite;
  }
  if (!InRange(input.texcoord_y,
               kSkyViewMinimumTexcoord,
               kSkyViewMaximumTexcoord)) {
    return SkyViewAdapterDiagnostic::texcoord_y_out_of_range;
  }
  if (!MatrixIsFinite(input.inverse_view_projection)) {
    return SkyViewAdapterDiagnostic::inverse_view_projection_non_finite;
  }
  if (!std::isfinite(input.camera_world_x)
      || !std::isfinite(input.camera_world_y)
      || !std::isfinite(input.camera_world_z)) {
    return SkyViewAdapterDiagnostic::camera_world_non_finite;
  }
  if (!WorldCoordinate(input.camera_world_x)
      || !WorldCoordinate(input.camera_world_y)
      || !WorldCoordinate(input.camera_world_z)) {
    return SkyViewAdapterDiagnostic::camera_world_out_of_range;
  }
  if (!std::isfinite(input.aurora_origin_world_x)
      || !std::isfinite(input.aurora_origin_world_y)
      || !std::isfinite(input.aurora_origin_world_z)) {
    return SkyViewAdapterDiagnostic::aurora_origin_non_finite;
  }
  if (!WorldCoordinate(input.aurora_origin_world_x)
      || !WorldCoordinate(input.aurora_origin_world_y)
      || !WorldCoordinate(input.aurora_origin_world_z)) {
    return SkyViewAdapterDiagnostic::aurora_origin_out_of_range;
  }
  if (!std::isfinite(input.engine_world_units_per_aurora_unit)) {
    return SkyViewAdapterDiagnostic::world_scale_non_finite;
  }
  if (!InRange(input.engine_world_units_per_aurora_unit,
               kSkyViewMinimumWorldScale,
               kSkyViewMaximumWorldScale)) {
    return SkyViewAdapterDiagnostic::world_scale_out_of_range;
  }
  return SkyViewAdapterDiagnostic::none;
}

}  // namespace

SkyViewAdapterEvaluation EvaluateSkyViewAdapter(
    const SkyViewAdapterInput& input,
    SkyViewAdapterOutput& output) noexcept {
  const SkyViewAdapterDiagnostic diagnostic = ValidateInput(input);
  if (diagnostic != SkyViewAdapterDiagnostic::none) {
    return Reject(diagnostic);
  }

  const float clip_x = (2.0F * input.texcoord_x) - 1.0F;
  const float clip_y = 1.0F - (2.0F * input.texcoord_y);
  constexpr float clip_z = 1.0F;
  constexpr float clip_w = 1.0F;
  const auto& matrix = input.inverse_view_projection;
  const float world_h_x = (matrix.m00 * clip_x) + (matrix.m01 * clip_y)
      + (matrix.m02 * clip_z) + (matrix.m03 * clip_w);
  const float world_h_y = (matrix.m10 * clip_x) + (matrix.m11 * clip_y)
      + (matrix.m12 * clip_z) + (matrix.m13 * clip_w);
  const float world_h_z = (matrix.m20 * clip_x) + (matrix.m21 * clip_y)
      + (matrix.m22 * clip_z) + (matrix.m23 * clip_w);
  const float world_h_w = (matrix.m30 * clip_x) + (matrix.m31 * clip_y)
      + (matrix.m32 * clip_z) + (matrix.m33 * clip_w);
  if (!std::isfinite(world_h_w)
      || std::fabs(world_h_w) < kSkyViewMinimumHomogeneousW) {
    return Reject(SkyViewAdapterDiagnostic::unproject_w_invalid);
  }

  const float inverse_w = 1.0F / world_h_w;
  const float ray_x = (world_h_x * inverse_w) - input.camera_world_x;
  const float ray_y = (world_h_y * inverse_w) - input.camera_world_y;
  const float ray_z = (world_h_z * inverse_w) - input.camera_world_z;
  const float direction_length_squared = (ray_x * ray_x)
      + (ray_y * ray_y)
      + (ray_z * ray_z);
  if (!std::isfinite(direction_length_squared)
      || direction_length_squared < kSkyViewMinimumDirectionLengthSquared) {
    return Reject(SkyViewAdapterDiagnostic::world_ray_invalid);
  }

  const float inverse_direction_length = 1.0F / std::sqrt(direction_length_squared);
  const float inverse_scale = 1.0F / input.engine_world_units_per_aurora_unit;
  SkyViewAdapterOutput candidate{
      ray_x * inverse_direction_length,
      ray_y * inverse_direction_length,
      ray_z * inverse_direction_length,
      (input.camera_world_x - input.aurora_origin_world_x) * inverse_scale,
      (input.camera_world_y - input.aurora_origin_world_y) * inverse_scale,
      (input.camera_world_z - input.aurora_origin_world_z) * inverse_scale,
  };
  if (!std::isfinite(candidate.camera_aurora_x)
      || !std::isfinite(candidate.camera_aurora_y)
      || !std::isfinite(candidate.camera_aurora_z)) {
    return Reject(SkyViewAdapterDiagnostic::camera_aurora_non_finite);
  }
  if (!InRange(candidate.camera_aurora_x,
               kAuroraMinimumCameraCoordinate,
               kAuroraMaximumCameraCoordinate)
      || !InRange(candidate.camera_aurora_y,
                  kAuroraMinimumCameraCoordinate,
                  kAuroraMaximumCameraCoordinate)
      || !InRange(candidate.camera_aurora_z,
                  kAuroraMinimumCameraCoordinate,
                  kAuroraMaximumCameraCoordinate)) {
    return Reject(SkyViewAdapterDiagnostic::camera_aurora_out_of_range);
  }

  output = candidate;
  return {SkyViewAdapterStatus::evaluated, SkyViewAdapterDiagnostic::none};
}

}  // namespace truth::render
