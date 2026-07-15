#include "truth/render/SkyFields.hpp"
#include "truth/render/AuroraCurtain.hpp"
#include "truth/render/detail/SkyFieldNoise.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>

namespace truth::render {
namespace {

inline constexpr float kPi = 3.14159265358979323846F;
inline constexpr float kTwoPi = 2.0F * kPi;
inline constexpr float kCloudErosionScale = 0.22F;

[[nodiscard]] bool InRange(const float value, const float minimum, const float maximum) noexcept {
  return value >= minimum && value <= maximum;
}

[[nodiscard]] SkyFieldEvaluation Reject(const SkyFieldDiagnostic diagnostic) noexcept {
  return {SkyFieldStatus::rejected, diagnostic};
}

[[nodiscard]] SkyFieldDiagnostic ValidateInput(const SkyFieldInput& input) noexcept {
  if (!std::isfinite(input.view_x)) {
    return SkyFieldDiagnostic::view_x_non_finite;
  }
  if (!InRange(input.view_x, kSkyFieldMinimumDirection, kSkyFieldMaximumDirection)) {
    return SkyFieldDiagnostic::view_x_out_of_range;
  }
  if (!std::isfinite(input.view_y)) {
    return SkyFieldDiagnostic::view_y_non_finite;
  }
  if (!InRange(input.view_y, kSkyFieldMinimumDirection, kSkyFieldMaximumDirection)) {
    return SkyFieldDiagnostic::view_y_out_of_range;
  }
  if (!std::isfinite(input.view_z)) {
    return SkyFieldDiagnostic::view_z_non_finite;
  }
  if (!InRange(input.view_z, kSkyFieldMinimumDirection, kSkyFieldMaximumDirection)) {
    return SkyFieldDiagnostic::view_z_out_of_range;
  }
  const float direction_length_squared = (input.view_x * input.view_x)
      + (input.view_y * input.view_y)
      + (input.view_z * input.view_z);
  if (!std::isfinite(direction_length_squared)
      || std::fabs(direction_length_squared - 1.0F) > kSkyFieldDirectionLengthTolerance) {
    return SkyFieldDiagnostic::view_direction_not_normalized;
  }
  if (!std::isfinite(input.phase)) {
    return SkyFieldDiagnostic::phase_non_finite;
  }
  if (!InRange(input.phase, kSkyFieldMinimumPhase, kSkyFieldMaximumPhase)) {
    return SkyFieldDiagnostic::phase_out_of_range;
  }
  if (!std::isfinite(input.wind_x)) {
    return SkyFieldDiagnostic::wind_x_non_finite;
  }
  if (!InRange(input.wind_x, kSkyFieldMinimumWind, kSkyFieldMaximumWind)) {
    return SkyFieldDiagnostic::wind_x_out_of_range;
  }
  if (!std::isfinite(input.wind_y)) {
    return SkyFieldDiagnostic::wind_y_non_finite;
  }
  if (!InRange(input.wind_y, kSkyFieldMinimumWind, kSkyFieldMaximumWind)) {
    return SkyFieldDiagnostic::wind_y_out_of_range;
  }
  if (!std::isfinite(input.cloud_coverage)) {
    return SkyFieldDiagnostic::cloud_coverage_non_finite;
  }
  if (!InRange(input.cloud_coverage, kSkyFieldMinimumControl, kSkyFieldMaximumControl)) {
    return SkyFieldDiagnostic::cloud_coverage_out_of_range;
  }
  if (!std::isfinite(input.cloud_density)) {
    return SkyFieldDiagnostic::cloud_density_non_finite;
  }
  if (!InRange(input.cloud_density, kSkyFieldMinimumControl, kSkyFieldMaximumControl)) {
    return SkyFieldDiagnostic::cloud_density_out_of_range;
  }
  if (!std::isfinite(input.weather_density)) {
    return SkyFieldDiagnostic::weather_density_non_finite;
  }
  if (!InRange(input.weather_density, kSkyFieldMinimumControl, kSkyFieldMaximumControl)) {
    return SkyFieldDiagnostic::weather_density_out_of_range;
  }
  if (!std::isfinite(input.aurora_activity)) {
    return SkyFieldDiagnostic::aurora_activity_non_finite;
  }
  if (!InRange(input.aurora_activity, kSkyFieldMinimumControl, kSkyFieldMaximumControl)) {
    return SkyFieldDiagnostic::aurora_activity_out_of_range;
  }
  if (!std::isfinite(input.night_factor)) {
    return SkyFieldDiagnostic::night_factor_non_finite;
  }
  if (!InRange(input.night_factor, kSkyFieldMinimumControl, kSkyFieldMaximumControl)) {
    return SkyFieldDiagnostic::night_factor_out_of_range;
  }
  if (!std::isfinite(input.camera_x)) {
    return SkyFieldDiagnostic::camera_x_non_finite;
  }
  if (!InRange(input.camera_x,
               kAuroraMinimumCameraCoordinate,
               kAuroraMaximumCameraCoordinate)) {
    return SkyFieldDiagnostic::camera_x_out_of_range;
  }
  if (!std::isfinite(input.camera_y)) {
    return SkyFieldDiagnostic::camera_y_non_finite;
  }
  if (!InRange(input.camera_y,
               kAuroraMinimumCameraCoordinate,
               kAuroraMaximumCameraCoordinate)) {
    return SkyFieldDiagnostic::camera_y_out_of_range;
  }
  if (!std::isfinite(input.camera_z)) {
    return SkyFieldDiagnostic::camera_z_non_finite;
  }
  if (!InRange(input.camera_z,
               kAuroraMinimumCameraCoordinate,
               kAuroraMaximumCameraCoordinate)) {
    return SkyFieldDiagnostic::camera_z_out_of_range;
  }
  return SkyFieldDiagnostic::none;
}

[[nodiscard]] std::uint32_t MixBits(std::uint32_t value) noexcept {
  value ^= value >> 16U;
  value *= 0x7FEB352DU;
  value ^= value >> 15U;
  value *= 0x846CA68BU;
  value ^= value >> 16U;
  return value;
}

[[nodiscard]] float Smooth(const float value) noexcept {
  return value * value * (3.0F - (2.0F * value));
}

[[nodiscard]] float LinearInterpolate(const float lhs, const float rhs, const float amount) noexcept {
  return lhs + ((rhs - lhs) * amount);
}

[[nodiscard]] float SmoothStep(const float lower, const float upper, const float value) noexcept {
  const float amount = std::clamp((value - lower) / (upper - lower), 0.0F, 1.0F);
  return Smooth(amount);
}

[[nodiscard]] bool IsFinite(const SkyFieldRadiance& value) noexcept {
  return std::isfinite(value.r) && std::isfinite(value.g) && std::isfinite(value.b);
}

[[nodiscard]] bool IsUnitInterval(const SkyFieldRadiance& value) noexcept {
  return InRange(value.r, 0.0F, 1.0F)
      && InRange(value.g, 0.0F, 1.0F)
      && InRange(value.b, 0.0F, 1.0F);
}

struct FieldPoint {
  float x;
  float y;
  float z;
};

[[nodiscard]] float Noise(const FieldPoint point) noexcept {
  return detail::SkyFieldValueNoise3D(point.x, point.y, point.z);
}

[[nodiscard]] FieldPoint ScaleAndOffset(
    const FieldPoint point,
    const float scale,
    const FieldPoint offset) noexcept {
  return {
      (point.x * scale) + offset.x,
      (point.y * scale) + offset.y,
      (point.z * scale) + offset.z,
  };
}

}  // namespace

namespace detail {

float SkyFieldLatticeHash3D(
    const std::int32_t x,
    const std::int32_t y,
    const std::int32_t z) noexcept {
  const std::uint32_t x_bits = std::bit_cast<std::uint32_t>(x);
  const std::uint32_t y_bits = std::bit_cast<std::uint32_t>(y);
  const std::uint32_t z_bits = std::bit_cast<std::uint32_t>(z);
  const std::uint32_t combined = (x_bits * 0x8DA6B343U)
      ^ (y_bits * 0xD8163841U)
      ^ (z_bits * 0xCB1AB31FU)
      ^ 0x9E3779B9U;
  const std::uint32_t mixed = MixBits(combined);
  return static_cast<float>(mixed & 0x00FFFFFFU) / 16777215.0F;
}

float SkyFieldValueNoise3D(
    const float x,
    const float y,
    const float z) noexcept {
  const float floor_x = std::floor(x);
  const float floor_y = std::floor(y);
  const float floor_z = std::floor(z);
  const auto lattice_x = static_cast<std::int32_t>(floor_x);
  const auto lattice_y = static_cast<std::int32_t>(floor_y);
  const auto lattice_z = static_cast<std::int32_t>(floor_z);
  const float blend_x = Smooth(x - floor_x);
  const float blend_y = Smooth(y - floor_y);
  const float blend_z = Smooth(z - floor_z);

  const auto plane = [&](const std::int32_t plane_z) noexcept {
    const float lower = LinearInterpolate(
        SkyFieldLatticeHash3D(lattice_x, lattice_y, plane_z),
        SkyFieldLatticeHash3D(lattice_x + 1, lattice_y, plane_z),
        blend_x);
    const float upper = LinearInterpolate(
        SkyFieldLatticeHash3D(lattice_x, lattice_y + 1, plane_z),
        SkyFieldLatticeHash3D(lattice_x + 1, lattice_y + 1, plane_z),
        blend_x);
    return LinearInterpolate(lower, upper, blend_y);
  };

  return LinearInterpolate(plane(lattice_z), plane(lattice_z + 1), blend_z);
}

}  // namespace detail

SkyFieldEvaluation EvaluateSkyFields(
    const SkyFieldInput& input,
    SkyFieldOutput& output) noexcept {
  const SkyFieldDiagnostic diagnostic = ValidateInput(input);
  if (diagnostic != SkyFieldDiagnostic::none) {
    return Reject(diagnostic);
  }

  const float wrapped_phase = input.phase >= 1.0F ? 0.0F : input.phase;
  const float phase_angle = wrapped_phase * kTwoPi;
  const float phase_sine = std::sin(phase_angle);
  const float phase_cosine = std::cos(phase_angle);
  const float phase_arc = 1.0F - phase_cosine;
  const FieldPoint loop_offset{
      (0.48F * input.wind_x * phase_sine)
          + (0.19F * input.wind_y * phase_arc),
      (0.48F * input.wind_y * phase_sine)
          - (0.19F * input.wind_x * phase_arc),
      (0.17F * (input.wind_x + input.wind_y) * phase_sine)
          + (0.09F * (input.wind_x - input.wind_y) * phase_arc),
  };
  const FieldPoint direction_space{
      (1.62F * input.view_x) + loop_offset.x,
      (1.62F * input.view_y) + loop_offset.y,
      (2.35F * input.view_z) + loop_offset.z,
  };
  const FieldPoint domain_warp{
      Noise(ScaleAndOffset(direction_space, 0.72F, {17.1F, -4.7F, 8.3F})) - 0.5F,
      Noise(ScaleAndOffset(direction_space, 0.72F, {-9.2F, 13.6F, 2.8F})) - 0.5F,
      Noise(ScaleAndOffset(direction_space, 0.72F, {5.4F, 7.9F, -11.5F})) - 0.5F,
  };
  const FieldPoint warped{
      direction_space.x + (0.58F * domain_warp.x),
      direction_space.y + (0.58F * domain_warp.y),
      direction_space.z + (0.38F * domain_warp.z),
  };
  const float body_broad = Noise(ScaleAndOffset(warped, 0.62F, {1.7F, -3.2F, 5.1F}));
  const float body_strata = Noise(ScaleAndOffset(warped, 1.24F, {-6.4F, 8.8F, 2.3F}));
  const float body_breakup = Noise(ScaleAndOffset(warped, 2.48F, {12.9F, 4.6F, -7.7F}));
  const float cloud_body = std::clamp(
      0.075F
          + (0.52F * body_broad)
          + (0.31F * body_strata)
          + (0.17F * body_breakup),
      0.0F,
      1.0F);
  const float detail_primary = Noise(
      ScaleAndOffset(warped, 5.1F, {-5.4F, 9.2F, 3.1F}));
  const float detail_fine = Noise(
      ScaleAndOffset(warped, 10.3F, {3.8F, -12.6F, 7.4F}));
  const float detail_noise = std::clamp(
      (0.64F * detail_primary) + (0.36F * detail_fine),
      0.0F,
      1.0F);
  const float detail_erosion = detail_noise * kCloudErosionScale;
  const float coverage_threshold = LinearInterpolate(0.72F,
                                                      0.30F,
                                                      input.cloud_coverage);
  const float coverage_gate = SmoothStep(0.0F, 0.20F, input.cloud_coverage);
  const float occupied_body = input.cloud_coverage == 0.0F
      ? 0.0F
      : SmoothStep(coverage_threshold - 0.12F,
                   coverage_threshold + 0.42F,
                   cloud_body) * coverage_gate;
  const float weather_scale = 0.35F + (0.65F * input.weather_density);
  const float composed_cloud = std::clamp(
      std::max(occupied_body - detail_erosion, 0.0F) * input.cloud_density * weather_scale,
      0.0F,
      1.0F);

  SkyFieldOutput candidate{};
  candidate.cloud_body = cloud_body;
  candidate.cloud_detail_erosion = detail_erosion;
  candidate.cloud_density = composed_cloud;

  AuroraCurtainOutput aurora{};
  const AuroraCurtainInput aurora_input{
      input.camera_x,
      input.camera_y,
      input.camera_z,
      input.view_x,
      input.view_y,
      input.view_z,
      input.phase,
      input.wind_x,
      input.wind_y,
      input.aurora_activity,
      input.night_factor,
      AuroraQuality::balanced,
  };
  const AuroraCurtainEvaluation aurora_evaluation =
      EvaluateAuroraCurtain(aurora_input, aurora);
  if (aurora_evaluation.status != AuroraCurtainStatus::evaluated) {
    return Reject(AuroraCurtainDiagnostic::calculation_non_finite
                      == aurora_evaluation.diagnostic
                  ? SkyFieldDiagnostic::calculation_non_finite
                  : SkyFieldDiagnostic::calculation_out_of_range);
  }
  candidate.aurora_mask = aurora.mask;
  candidate.aurora_intrinsic_radiance = {
      aurora.intrinsic_radiance.r,
      aurora.intrinsic_radiance.g,
      aurora.intrinsic_radiance.b,
  };

  if (!std::isfinite(candidate.cloud_body)
      || !std::isfinite(candidate.cloud_detail_erosion)
      || !std::isfinite(candidate.cloud_density)
      || !std::isfinite(candidate.aurora_mask)
      || !IsFinite(candidate.aurora_intrinsic_radiance)) {
    return Reject(SkyFieldDiagnostic::calculation_non_finite);
  }
  if (!InRange(candidate.cloud_body, 0.0F, 1.0F)
      || !InRange(candidate.cloud_detail_erosion, 0.0F, 1.0F)
      || !InRange(candidate.cloud_density, 0.0F, 1.0F)
      || !InRange(candidate.aurora_mask, 0.0F, 1.0F)
      || !IsUnitInterval(candidate.aurora_intrinsic_radiance)) {
    return Reject(SkyFieldDiagnostic::calculation_out_of_range);
  }

  output = candidate;
  return {SkyFieldStatus::evaluated, SkyFieldDiagnostic::none};
}

}  // namespace truth::render
