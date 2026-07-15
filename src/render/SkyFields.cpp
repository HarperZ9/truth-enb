#include "truth/render/SkyFields.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>

namespace truth::render {
namespace {

inline constexpr float kPi = 3.14159265358979323846F;
inline constexpr float kTwoPi = 2.0F * kPi;
inline constexpr float kCloudCoverageSlope = 3.75F;
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

[[nodiscard]] float LatticeHash(const std::int32_t x, const std::int32_t y) noexcept {
  const std::uint32_t x_bits = std::bit_cast<std::uint32_t>(x);
  const std::uint32_t y_bits = std::bit_cast<std::uint32_t>(y);
  const std::uint32_t mixed = MixBits(x_bits ^ std::rotl(y_bits, 16) ^ 0x9E3779B9U);
  return static_cast<float>(mixed & 0x00FFFFFFU) / 16777215.0F;
}

[[nodiscard]] float Smooth(const float value) noexcept {
  return value * value * (3.0F - (2.0F * value));
}

[[nodiscard]] float LinearInterpolate(const float lhs, const float rhs, const float amount) noexcept {
  return lhs + ((rhs - lhs) * amount);
}

[[nodiscard]] float ValueNoise(const float x, const float y) noexcept {
  const float floor_x = std::floor(x);
  const float floor_y = std::floor(y);
  const auto lattice_x = static_cast<std::int32_t>(floor_x);
  const auto lattice_y = static_cast<std::int32_t>(floor_y);
  const float blend_x = Smooth(x - floor_x);
  const float blend_y = Smooth(y - floor_y);
  const float lower = LinearInterpolate(LatticeHash(lattice_x, lattice_y),
                                        LatticeHash(lattice_x + 1, lattice_y),
                                        blend_x);
  const float upper = LinearInterpolate(LatticeHash(lattice_x, lattice_y + 1),
                                        LatticeHash(lattice_x + 1, lattice_y + 1),
                                        blend_x);
  return LinearInterpolate(lower, upper, blend_y);
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

}  // namespace

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
  const float loop_x = (0.82F * input.wind_x * phase_sine)
      + (0.31F * input.wind_y * phase_arc);
  const float loop_y = (0.82F * input.wind_y * phase_sine)
      - (0.31F * input.wind_x * phase_arc);

  const float spatial_x = (2.4F * input.view_x) + (0.73F * input.view_z) + loop_x;
  const float spatial_y = (2.4F * input.view_y) - (0.41F * input.view_z) + loop_y;
  const float body_low = ValueNoise(0.85F * spatial_x, 0.85F * spatial_y);
  const float body_fold = ValueNoise((1.72F * spatial_x) + 11.3F,
                                     (1.72F * spatial_y) - 7.1F);
  const float cloud_body = std::clamp((0.67F * body_low) + (0.33F * body_fold), 0.0F, 1.0F);
  const float detail_primary = ValueNoise((6.7F * spatial_x) - 5.4F,
                                          (6.7F * spatial_y) + 9.2F);
  const float detail_fine = ValueNoise((13.9F * spatial_x) + 3.8F,
                                       (13.9F * spatial_y) - 12.6F);
  const float detail_noise = std::clamp((0.55F * detail_primary) + (0.45F * detail_fine),
                                        0.0F,
                                        1.0F);
  const float detail_erosion = detail_noise * kCloudErosionScale;
  const float occupied_body = std::clamp(
      (cloud_body + input.cloud_coverage - 1.0F) * kCloudCoverageSlope,
      0.0F,
      1.0F);
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
    const float curtain_center = (0.22F * std::sin((4.0F * input.view_x)
                                                   + (0.65F * phase_sine)))
        + (0.10F * std::sin((9.0F * input.view_x) - (0.45F * phase_cosine)));
    const float curtain_distance = std::fabs(input.view_y - curtain_center);
    float curtain = std::clamp(1.0F - (curtain_distance / 0.55F), 0.0F, 1.0F);
    curtain *= curtain;
    const float horizon_gate = SmoothStep(-0.05F, 0.28F, input.view_z);
    const float zenith_gate = 1.0F - SmoothStep(0.82F, 1.0F, input.view_z);
    const float fold = 0.38F + (0.62F * std::fabs(std::sin(
        (17.0F * input.view_x) + (3.0F * input.view_y) + phase_angle + (2.0F * cloud_body))));
    candidate.aurora_mask = std::clamp(
        curtain * horizon_gate * zenith_gate * fold * input.night_factor,
        0.0F,
        1.0F);

    if (input.aurora_activity == 0.0F || candidate.aurora_mask == 0.0F) {
      candidate.aurora_intrinsic_radiance = {0.0F, 0.0F, 0.0F};
    } else {
      const float hue = 0.5F + (0.5F * std::sin((6.5F * input.view_x)
                                                - (2.0F * input.view_y)
                                                + phase_angle));
      const float strength = candidate.aurora_mask * input.aurora_activity;
      candidate.aurora_intrinsic_radiance = {
          strength * LinearInterpolate(0.08F, 0.38F, hue),
          strength * LinearInterpolate(0.78F, 0.42F, hue),
          strength * LinearInterpolate(0.42F, 0.88F, hue),
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
