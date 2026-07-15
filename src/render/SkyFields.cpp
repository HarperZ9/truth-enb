#include "truth/render/SkyFields.hpp"
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

  if (input.night_factor == 0.0F) {
    candidate.aurora_mask = 0.0F;
    candidate.aurora_intrinsic_radiance = {0.0F, 0.0F, 0.0F};
  } else {
    const float horizontal_length = std::max(
        std::sqrt((input.view_x * input.view_x) + (input.view_y * input.view_y)),
        0.001F);
    const float side = input.view_x / horizontal_length;
    const float forward = input.view_y / horizontal_length;
    const float arc_gate = SmoothStep(-0.42F, 0.02F, forward);
    const float fold_noise = Noise({
        (2.25F * side) + (0.42F * loop_offset.x) + 4.8F,
        (1.65F * forward) + (0.42F * loop_offset.y) - 7.2F,
        1.4F + (0.55F * loop_offset.z),
    });
    const float curtain_center = 0.70F
        + (0.085F * std::sin((2.8F * side) + (0.7F * forward)
                              + (0.55F * phase_sine)))
        + (0.052F * std::sin((6.3F * side) - (1.1F * forward)
                              - (0.45F * phase_cosine)))
        + (0.055F * (fold_noise - 0.5F));
    const float lower_edge = curtain_center - 0.37F - (0.035F * fold_noise);
    const float upper_edge = curtain_center + 0.22F + (0.020F * fold_noise);
    const float lower_falloff = SmoothStep(lower_edge,
                                           curtain_center - 0.045F,
                                           input.view_z);
    const float upper_falloff = 1.0F - SmoothStep(curtain_center + 0.045F,
                                                  upper_edge,
                                                  input.view_z);
    const float ray_primary = Noise({
        (8.4F * side) + loop_offset.x + 10.7F,
        (8.4F * forward) + loop_offset.y - 3.9F,
        2.6F + loop_offset.z,
    });
    const float ray_fine = Noise({
        (17.2F * side) + (1.7F * loop_offset.x) - 6.1F,
        (17.2F * forward) + (1.7F * loop_offset.y) + 12.4F,
        -4.3F + (1.3F * loop_offset.z),
    });
    const float vertical_rays = 0.48F
        + (0.52F * ((0.68F * ray_primary) + (0.32F * ray_fine)));
    const float folded_sheet = 0.72F
        + (0.28F * (1.0F - std::fabs((2.0F * fold_noise) - 1.0F)));
    candidate.aurora_mask = std::clamp(
        arc_gate
            * lower_falloff
            * upper_falloff
            * vertical_rays
            * folded_sheet
            * input.night_factor,
        0.0F,
        1.0F);

    if (input.aurora_activity == 0.0F || candidate.aurora_mask == 0.0F) {
      candidate.aurora_intrinsic_radiance = {0.0F, 0.0F, 0.0F};
    } else {
      const float upper_fringe = SmoothStep(curtain_center - 0.10F,
                                             curtain_center + 0.22F,
                                             input.view_z);
      const float fringe_blend = 0.68F * upper_fringe;
      const float altitude_gain = 0.39F
          + (0.81F * SmoothStep(curtain_center - 0.16F,
                                curtain_center + 0.12F,
                                input.view_z));
      const float strength = candidate.aurora_mask
          * input.aurora_activity
          * altitude_gain;
      candidate.aurora_intrinsic_radiance = {
          strength * LinearInterpolate(0.07F, 0.28F, fringe_blend),
          strength * LinearInterpolate(0.82F, 0.42F, fringe_blend),
          strength * LinearInterpolate(0.32F, 0.78F, fringe_blend),
      };
    }
  }

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
