#pragma once

#include "truth/render/AuroraCurtain.hpp"

#include <cstdint>

namespace truth::render {

inline constexpr float kSkyViewMinimumTexcoord = 0.0F;
inline constexpr float kSkyViewMaximumTexcoord = 1.0F;
inline constexpr float kSkyViewMinimumWorldCoordinate = -100000000.0F;
inline constexpr float kSkyViewMaximumWorldCoordinate = 100000000.0F;
inline constexpr float kSkyViewMinimumWorldScale = 0.0001F;
inline constexpr float kSkyViewMaximumWorldScale = 1000000.0F;
inline constexpr float kSkyViewMinimumHomogeneousW = 0.000001F;
inline constexpr float kSkyViewMinimumDirectionLengthSquared = 0.00000001F;

// Row-major inverse view-projection matrix. It transforms a D3D clip-space
// column vector (x, y, z, w) into an engine-world homogeneous position.
struct SkyViewMatrix {
  float m00; float m01; float m02; float m03;
  float m10; float m11; float m12; float m13;
  float m20; float m21; float m22; float m23;
  float m30; float m31; float m32; float m33;
};

struct SkyViewAdapterInput {
  float texcoord_x;
  float texcoord_y;
  SkyViewMatrix inverse_view_projection;
  float camera_world_x;
  float camera_world_y;
  float camera_world_z;
  float aurora_origin_world_x;
  float aurora_origin_world_y;
  float aurora_origin_world_z;
  float engine_world_units_per_aurora_unit;
};

struct SkyViewAdapterOutput {
  float view_world_x;
  float view_world_y;
  float view_world_z;
  float camera_aurora_x;
  float camera_aurora_y;
  float camera_aurora_z;
};

enum class SkyViewAdapterStatus : std::uint32_t {
  evaluated = 0U,
  rejected = 1U,
};

enum class SkyViewAdapterDiagnostic : std::uint32_t {
  none = 0U,
  texcoord_x_non_finite = 100U,
  texcoord_x_out_of_range = 101U,
  texcoord_y_non_finite = 110U,
  texcoord_y_out_of_range = 111U,
  inverse_view_projection_non_finite = 120U,
  camera_world_non_finite = 130U,
  camera_world_out_of_range = 131U,
  aurora_origin_non_finite = 140U,
  aurora_origin_out_of_range = 141U,
  world_scale_non_finite = 150U,
  world_scale_out_of_range = 151U,
  unproject_w_invalid = 200U,
  world_ray_invalid = 201U,
  camera_aurora_non_finite = 210U,
  camera_aurora_out_of_range = 211U,
};

struct SkyViewAdapterEvaluation {
  SkyViewAdapterStatus status;
  SkyViewAdapterDiagnostic diagnostic;
};

[[nodiscard]] SkyViewAdapterEvaluation EvaluateSkyViewAdapter(
    const SkyViewAdapterInput& input,
    SkyViewAdapterOutput& output) noexcept;

}  // namespace truth::render
