#include "truth/render/AuroraCurtain.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>

namespace truth::render {
namespace {

inline constexpr float kPi = 3.14159265358979323846F;
inline constexpr float kTwoPi = 2.0F * kPi;
inline constexpr float kMinimumRayElevation = 0.0001F;
inline constexpr float kPathLengthFloor = 0.08F;
inline constexpr float kMaximumStepPathWeight = 0.55F;

struct Point3 {
  float x;
  float y;
  float z;
};

struct ElectronFlux {
  float dynamic;
  float persistent;
};

[[nodiscard]] bool InRange(
    const float value,
    const float minimum,
    const float maximum) noexcept {
  return value >= minimum && value <= maximum;
}

[[nodiscard]] AuroraCurtainEvaluation Reject(
    const AuroraCurtainDiagnostic diagnostic) noexcept {
  return {AuroraCurtainStatus::rejected, diagnostic};
}

[[nodiscard]] float Smooth(const float value) noexcept {
  return value * value * (3.0F - (2.0F * value));
}

[[nodiscard]] float SmoothStep(
    const float lower,
    const float upper,
    const float value) noexcept {
  const float amount = std::clamp((value - lower) / (upper - lower), 0.0F, 1.0F);
  return Smooth(amount);
}

[[nodiscard]] float LinearInterpolate(
    const float lhs,
    const float rhs,
    const float amount) noexcept {
  return lhs + ((rhs - lhs) * amount);
}

[[nodiscard]] std::uint32_t MixBits(std::uint32_t value) noexcept {
  value ^= value >> 16U;
  value *= 0x7FEB352DU;
  value ^= value >> 15U;
  value *= 0x846CA68BU;
  value ^= value >> 16U;
  return value;
}

[[nodiscard]] float LatticeHash3D(
    const std::int32_t x,
    const std::int32_t y,
    const std::int32_t z) noexcept {
  const std::uint32_t x_bits = std::bit_cast<std::uint32_t>(x);
  const std::uint32_t y_bits = std::bit_cast<std::uint32_t>(y);
  const std::uint32_t z_bits = std::bit_cast<std::uint32_t>(z);
  const std::uint32_t combined = (x_bits * 0xA24BAED5U)
      ^ (y_bits * 0x9FB21C65U)
      ^ (z_bits * 0xC13FA9A9U)
      ^ 0x91E10DA5U;
  return static_cast<float>(MixBits(combined) & 0x00FFFFFFU) / 16777215.0F;
}

[[nodiscard]] float ValueNoise3D(const Point3 point) noexcept {
  const float floor_x = std::floor(point.x);
  const float floor_y = std::floor(point.y);
  const float floor_z = std::floor(point.z);
  const auto lattice_x = static_cast<std::int32_t>(floor_x);
  const auto lattice_y = static_cast<std::int32_t>(floor_y);
  const auto lattice_z = static_cast<std::int32_t>(floor_z);
  const float blend_x = Smooth(point.x - floor_x);
  const float blend_y = Smooth(point.y - floor_y);
  const float blend_z = Smooth(point.z - floor_z);

  const auto plane = [&](const std::int32_t plane_z) noexcept {
    const float lower = LinearInterpolate(
        LatticeHash3D(lattice_x, lattice_y, plane_z),
        LatticeHash3D(lattice_x + 1, lattice_y, plane_z),
        blend_x);
    const float upper = LinearInterpolate(
        LatticeHash3D(lattice_x, lattice_y + 1, plane_z),
        LatticeHash3D(lattice_x + 1, lattice_y + 1, plane_z),
        blend_x);
    return LinearInterpolate(lower, upper, blend_y);
  };
  return LinearInterpolate(plane(lattice_z), plane(lattice_z + 1), blend_z);
}

[[nodiscard]] float Gaussian(
    const float value,
    const float center,
    const float standard_deviation) noexcept {
  const float normalized = (value - center) / standard_deviation;
  return std::exp(-0.5F * normalized * normalized);
}

[[nodiscard]] AuroraCurtainDiagnostic ValidateInput(
    const AuroraCurtainInput& input) noexcept {
  const auto coordinate = [](const float value) noexcept {
    return InRange(value, kAuroraMinimumCameraCoordinate, kAuroraMaximumCameraCoordinate);
  };
  if (!std::isfinite(input.camera_x)) {
    return AuroraCurtainDiagnostic::camera_x_non_finite;
  }
  if (!coordinate(input.camera_x)) {
    return AuroraCurtainDiagnostic::camera_x_out_of_range;
  }
  if (!std::isfinite(input.camera_y)) {
    return AuroraCurtainDiagnostic::camera_y_non_finite;
  }
  if (!coordinate(input.camera_y)) {
    return AuroraCurtainDiagnostic::camera_y_out_of_range;
  }
  if (!std::isfinite(input.camera_z)) {
    return AuroraCurtainDiagnostic::camera_z_non_finite;
  }
  if (!coordinate(input.camera_z)) {
    return AuroraCurtainDiagnostic::camera_z_out_of_range;
  }
  if (!std::isfinite(input.view_x)) {
    return AuroraCurtainDiagnostic::view_x_non_finite;
  }
  if (!InRange(input.view_x, kAuroraMinimumDirection, kAuroraMaximumDirection)) {
    return AuroraCurtainDiagnostic::view_x_out_of_range;
  }
  if (!std::isfinite(input.view_y)) {
    return AuroraCurtainDiagnostic::view_y_non_finite;
  }
  if (!InRange(input.view_y, kAuroraMinimumDirection, kAuroraMaximumDirection)) {
    return AuroraCurtainDiagnostic::view_y_out_of_range;
  }
  if (!std::isfinite(input.view_z)) {
    return AuroraCurtainDiagnostic::view_z_non_finite;
  }
  if (!InRange(input.view_z, kAuroraMinimumDirection, kAuroraMaximumDirection)) {
    return AuroraCurtainDiagnostic::view_z_out_of_range;
  }
  const float direction_length_squared = (input.view_x * input.view_x)
      + (input.view_y * input.view_y)
      + (input.view_z * input.view_z);
  if (!std::isfinite(direction_length_squared)
      || std::fabs(direction_length_squared - 1.0F)
          > kAuroraDirectionLengthTolerance) {
    return AuroraCurtainDiagnostic::view_direction_not_normalized;
  }
  if (!std::isfinite(input.phase)) {
    return AuroraCurtainDiagnostic::phase_non_finite;
  }
  if (!InRange(input.phase, kAuroraMinimumPhase, kAuroraMaximumPhase)) {
    return AuroraCurtainDiagnostic::phase_out_of_range;
  }
  if (!std::isfinite(input.wind_x)) {
    return AuroraCurtainDiagnostic::wind_x_non_finite;
  }
  if (!InRange(input.wind_x, kAuroraMinimumWind, kAuroraMaximumWind)) {
    return AuroraCurtainDiagnostic::wind_x_out_of_range;
  }
  if (!std::isfinite(input.wind_y)) {
    return AuroraCurtainDiagnostic::wind_y_non_finite;
  }
  if (!InRange(input.wind_y, kAuroraMinimumWind, kAuroraMaximumWind)) {
    return AuroraCurtainDiagnostic::wind_y_out_of_range;
  }
  if (!std::isfinite(input.activity)) {
    return AuroraCurtainDiagnostic::activity_non_finite;
  }
  if (!InRange(input.activity, kAuroraMinimumControl, kAuroraMaximumControl)) {
    return AuroraCurtainDiagnostic::activity_out_of_range;
  }
  if (!std::isfinite(input.night_factor)) {
    return AuroraCurtainDiagnostic::night_factor_non_finite;
  }
  if (!InRange(input.night_factor, kAuroraMinimumControl, kAuroraMaximumControl)) {
    return AuroraCurtainDiagnostic::night_factor_out_of_range;
  }
  if (AuroraSampleCount(input.quality) == 0U) {
    return AuroraCurtainDiagnostic::quality_invalid;
  }
  return AuroraCurtainDiagnostic::none;
}

[[nodiscard]] ElectronFlux EvaluateElectronFlux(
    const float world_x,
    const float world_y,
    const float phase_sine,
    const float phase_cosine,
    const float wind_x,
    const float wind_y) noexcept {
  constexpr float basis_cosine = 0.96105546F;
  constexpr float basis_sine = 0.27635565F;
  const float along = (basis_cosine * world_x) + (basis_sine * world_y);
  const float across = (-basis_sine * world_x) + (basis_cosine * world_y);
  const float phase_arc = 1.0F - phase_cosine;
  const float advected_along = along
      + (0.78F * wind_x * phase_sine)
      + (0.31F * wind_y * phase_arc);
  const float advected_across = across
      + (0.64F * wind_y * phase_sine)
      - (0.24F * wind_x * phase_arc);

  const float broad_warp = ValueNoise3D({
      (0.055F * advected_along) + (0.42F * phase_sine) + 11.3F,
      (0.045F * advected_across) + (0.42F * phase_cosine) - 7.1F,
      2.9F + (0.35F * phase_sine),
  }) - 0.5F;
  const float curl_warp = ValueNoise3D({
      (0.13F * advected_along) - (0.37F * phase_cosine) - 5.8F,
      (0.10F * advected_across) + (0.37F * phase_sine) + 9.6F,
      -4.2F + (0.29F * phase_cosine),
  }) - 0.5F;
  const float center = 10.45F
      + (0.0060F * advected_along * advected_along)
      + (1.15F * std::sin((0.115F * advected_along) + (0.55F * phase_sine)))
      + (0.52F * std::sin((0.34F * advected_along) - (0.42F * phase_cosine)))
      + (1.75F * broad_warp)
      + (0.58F * curl_warp);
  const float primary_width = 1.05F
      + (0.40F * ValueNoise3D({
          (0.085F * advected_along) + 3.2F,
          (0.065F * advected_across) - 8.4F,
          7.7F + (0.25F * phase_sine),
      }));
  const float primary_distance = (advected_across - center) / primary_width;
  const float primary_sheet = std::exp(-0.5F * primary_distance * primary_distance);

  const float secondary_center = center + 4.35F
      + (0.62F * std::sin((0.19F * advected_along) + 1.4F - phase_sine));
  const float secondary_distance = (advected_across - secondary_center) / 1.55F;
  const float secondary_sheet = std::exp(
      -0.5F * secondary_distance * secondary_distance);
  const float along_envelope = std::exp(
      -0.5F * (advected_along / 29.0F) * (advected_along / 29.0F));

  const float coarse_filament = ValueNoise3D({
      (0.24F * advected_along) + (0.75F * phase_sine) + 4.8F,
      (0.035F * advected_across) + (0.75F * phase_cosine) - 3.1F,
      12.7F + (0.45F * phase_sine),
  });
  const float fine_filament = ValueNoise3D({
      (0.92F * advected_along) - (1.15F * phase_cosine) - 8.2F,
      (0.055F * advected_across) + (1.15F * phase_sine) + 5.7F,
      -6.4F + (0.63F * phase_cosine),
  });
  const float filament_wave = 0.5F + (0.5F * std::sin(
      (2.75F * advected_along) + (1.9F * curl_warp) + (0.8F * phase_sine)));
  const float filament_signal = (0.53F * coarse_filament)
      + (0.31F * fine_filament)
      + (0.16F * filament_wave);
  const float filaments = 0.52F + (0.48F * SmoothStep(0.30F, 0.78F, filament_signal));
  const float sheet = std::clamp(
      primary_sheet + (0.38F * secondary_sheet), 0.0F, 1.0F);
  const float persistent_sheet = std::clamp(
      primary_sheet + (0.52F * secondary_sheet), 0.0F, 1.0F);
  return {
      std::clamp(along_envelope * sheet * filaments, 0.0F, 1.0F),
      std::clamp(along_envelope
                     * persistent_sheet
                     * (0.70F + (0.30F * coarse_filament)),
                 0.0F,
                 1.0F),
  };
}

[[nodiscard]] bool IsFinite(const AuroraRadiance& radiance) noexcept {
  return std::isfinite(radiance.r)
      && std::isfinite(radiance.g)
      && std::isfinite(radiance.b);
}

[[nodiscard]] bool IsUnitInterval(const AuroraRadiance& radiance) noexcept {
  return InRange(radiance.r, 0.0F, 1.0F)
      && InRange(radiance.g, 0.0F, 1.0F)
      && InRange(radiance.b, 0.0F, 1.0F);
}

}  // namespace

AuroraDepositionProfile EvaluateAuroraDepositionProfile(
    const float normalized_height) noexcept {
  if (!std::isfinite(normalized_height)) {
    return {};
  }
  const float height = std::clamp(normalized_height, 0.0F, 1.0F);
  const float green = Gaussian(height, 0.32F, 0.13F);
  const float blue = std::clamp(
      (0.84F * Gaussian(height, 0.29F, 0.115F))
          + (0.16F * Gaussian(height, 0.46F, 0.17F)),
      0.0F,
      1.0F);
  const float red = Gaussian(height, 0.72F, 0.23F);
  return {red, green, blue};
}

AuroraCurtainEvaluation EvaluateAuroraCurtain(
    const AuroraCurtainInput& input,
    AuroraCurtainOutput& output) noexcept {
  const AuroraCurtainDiagnostic diagnostic = ValidateInput(input);
  if (diagnostic != AuroraCurtainDiagnostic::none) {
    return Reject(diagnostic);
  }

  AuroraCurtainOutput candidate{};
  if (input.night_factor == 0.0F
      || input.view_z <= kMinimumRayElevation
      || input.camera_z >= kAuroraTopHeight) {
    output = candidate;
    return {AuroraCurtainStatus::evaluated, AuroraCurtainDiagnostic::none};
  }

  const float wrapped_phase = input.phase >= 1.0F ? 0.0F : input.phase;
  const float phase_angle = wrapped_phase * kTwoPi;
  const float phase_sine = std::sin(phase_angle);
  const float phase_cosine = std::cos(phase_angle);
  const float lower_height = std::max(kAuroraBaseHeight, input.camera_z);
  const float height_span = kAuroraTopHeight - lower_height;
  const std::uint32_t sample_count = AuroraSampleCount(input.quality);
  const float height_step = height_span / static_cast<float>(sample_count);
  const float path_denominator = std::max(input.view_z, kPathLengthFloor);
  const float normalized_step_path = std::min(
      height_step / ((kAuroraTopHeight - kAuroraBaseHeight) * path_denominator),
      kMaximumStepPathWeight);
  const float horizon_fade = SmoothStep(0.035F, 0.14F, input.view_z);

  float mask{};
  AuroraRadiance radiance{};
  for (std::uint32_t index = 0; index < sample_count; ++index) {
    const float unit_height = (static_cast<float>(index) + 0.5F)
        / static_cast<float>(sample_count);
    const float height = lower_height + (height_span * unit_height);
    const float distance = (height - input.camera_z) / input.view_z;
    if (distance > kAuroraMaximumRayDistance) {
      continue;
    }
    const float world_x = input.camera_x + (input.view_x * distance);
    const float world_y = input.camera_y + (input.view_y * distance);
    const ElectronFlux flux = EvaluateElectronFlux(
        world_x,
        world_y,
        phase_sine,
        phase_cosine,
        input.wind_x,
        input.wind_y);
    const float normalized_altitude = (height - kAuroraBaseHeight)
        / (kAuroraTopHeight - kAuroraBaseHeight);
    const AuroraDepositionProfile deposition =
        EvaluateAuroraDepositionProfile(normalized_altitude);
    const float blue_flux = (0.82F * flux.dynamic) + (0.18F * flux.persistent);
    mask += std::max(flux.dynamic, 0.55F * flux.persistent)
        * std::max({deposition.r, deposition.g, deposition.b})
        * normalized_step_path;
    radiance.r += 0.075F
        * deposition.r
        * flux.persistent
        * normalized_step_path;
    radiance.g += 0.310F
        * deposition.g
        * flux.dynamic
        * normalized_step_path;
    radiance.b += 0.100F
        * deposition.b
        * blue_flux
        * normalized_step_path;
  }

  const float visibility = horizon_fade * input.night_factor;
  candidate.mask = std::clamp(0.65F * mask * visibility, 0.0F, 1.0F);
  const float emission_scale = visibility * input.activity;
  candidate.intrinsic_radiance = {
      std::clamp(radiance.r * emission_scale, 0.0F, 1.0F),
      std::clamp(radiance.g * emission_scale, 0.0F, 1.0F),
      std::clamp(radiance.b * emission_scale, 0.0F, 1.0F),
  };
  candidate.samples = sample_count;

  if (!std::isfinite(candidate.mask) || !IsFinite(candidate.intrinsic_radiance)) {
    return Reject(AuroraCurtainDiagnostic::calculation_non_finite);
  }
  if (!InRange(candidate.mask, 0.0F, 1.0F)
      || !IsUnitInterval(candidate.intrinsic_radiance)) {
    return Reject(AuroraCurtainDiagnostic::calculation_out_of_range);
  }

  output = candidate;
  return {AuroraCurtainStatus::evaluated, AuroraCurtainDiagnostic::none};
}

}  // namespace truth::render
