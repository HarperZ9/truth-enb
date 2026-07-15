#include "truth/render/CloudVolume.hpp"

#include "truth/render/detail/SkyFieldNoise.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>

namespace truth::render {
namespace {

inline constexpr float kPi = 3.14159265358979323846F;
inline constexpr float kTwoPi = 2.0F * kPi;
inline constexpr float kDirectionTolerance = 0.0035F;
inline constexpr float kExtinctionScale = 1.35F;
inline constexpr float kShadowExtinctionScale = 1.10F;
inline constexpr float kMinimumTransmittance = 0.015F;

[[nodiscard]] bool InRange(
    const float value,
    const float minimum,
    const float maximum) noexcept {
  return value >= minimum && value <= maximum;
}

[[nodiscard]] bool IsFinite(const CloudVolumeVector value) noexcept {
  return std::isfinite(value.x)
      && std::isfinite(value.y)
      && std::isfinite(value.z);
}

[[nodiscard]] bool IsNormalized(const CloudVolumeVector value) noexcept {
  const float length_squared = (value.x * value.x)
      + (value.y * value.y)
      + (value.z * value.z);
  return std::isfinite(length_squared)
      && std::fabs(length_squared - 1.0F) <= kDirectionTolerance;
}

[[nodiscard]] bool IsDirectionInRange(const CloudVolumeVector value) noexcept {
  return InRange(value.x, -1.0F, 1.0F)
      && InRange(value.y, -1.0F, 1.0F)
      && InRange(value.z, -1.0F, 1.0F);
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

[[nodiscard]] CloudVolumeVector LinearInterpolate(
    const CloudVolumeVector lhs,
    const CloudVolumeVector rhs,
    const float amount) noexcept {
  return {
      LinearInterpolate(lhs.x, rhs.x, amount),
      LinearInterpolate(lhs.y, rhs.y, amount),
      LinearInterpolate(lhs.z, rhs.z, amount),
  };
}

[[nodiscard]] CloudVolumeVector Add(
    const CloudVolumeVector lhs,
    const CloudVolumeVector rhs) noexcept {
  return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

[[nodiscard]] CloudVolumeVector Scale(
    const CloudVolumeVector value,
    const float scale) noexcept {
  return {value.x * scale, value.y * scale, value.z * scale};
}

[[nodiscard]] float Dot(
    const CloudVolumeVector lhs,
    const CloudVolumeVector rhs) noexcept {
  return (lhs.x * rhs.x) + (lhs.y * rhs.y) + (lhs.z * rhs.z);
}

[[nodiscard]] float Noise(const CloudVolumeVector point) noexcept {
  return detail::SkyFieldValueNoise3D(point.x, point.y, point.z);
}

[[nodiscard]] std::uint32_t MixBits(std::uint32_t value) noexcept {
  value ^= value >> 16U;
  value *= 0x7FEB352DU;
  value ^= value >> 15U;
  value *= 0x846CA68BU;
  value ^= value >> 16U;
  return value;
}

[[nodiscard]] CloudVolumeEvaluation Reject(
    const CloudVolumeDiagnostic diagnostic) noexcept {
  return {CloudVolumeStatus::rejected, diagnostic};
}

[[nodiscard]] CloudVolumeDiagnostic ValidateInput(
    const CloudVolumeInput& input) noexcept {
  if (!IsFinite(input.camera_position)) {
    return CloudVolumeDiagnostic::camera_non_finite;
  }
  if (!InRange(input.camera_position.x, -10000.0F, 10000.0F)
      || !InRange(input.camera_position.y, -10000.0F, 10000.0F)
      || !InRange(input.camera_position.z, -10.0F, 20.0F)) {
    return CloudVolumeDiagnostic::camera_out_of_range;
  }
  if (!IsFinite(input.view_direction)) {
    return CloudVolumeDiagnostic::view_direction_non_finite;
  }
  if (!IsDirectionInRange(input.view_direction)) {
    return CloudVolumeDiagnostic::view_direction_out_of_range;
  }
  if (!IsNormalized(input.view_direction)) {
    return CloudVolumeDiagnostic::view_direction_not_normalized;
  }
  if (!IsFinite(input.sun_direction)) {
    return CloudVolumeDiagnostic::sun_direction_non_finite;
  }
  if (!IsDirectionInRange(input.sun_direction)) {
    return CloudVolumeDiagnostic::sun_direction_out_of_range;
  }
  if (!IsNormalized(input.sun_direction)) {
    return CloudVolumeDiagnostic::sun_direction_not_normalized;
  }
  if (!std::isfinite(input.phase)) {
    return CloudVolumeDiagnostic::phase_non_finite;
  }
  if (!InRange(input.phase, 0.0F, 1.0F)) {
    return CloudVolumeDiagnostic::phase_out_of_range;
  }
  if (!std::isfinite(input.wind_x) || !std::isfinite(input.wind_y)) {
    return CloudVolumeDiagnostic::wind_non_finite;
  }
  if (!InRange(input.wind_x, -1.0F, 1.0F)
      || !InRange(input.wind_y, -1.0F, 1.0F)) {
    return CloudVolumeDiagnostic::wind_out_of_range;
  }
  if (!std::isfinite(input.cloud_coverage)
      || !std::isfinite(input.cloud_density)
      || !std::isfinite(input.weather_density)
      || !std::isfinite(input.cloud_type)
      || !std::isfinite(input.night_factor)) {
    return CloudVolumeDiagnostic::control_non_finite;
  }
  if (!InRange(input.cloud_coverage, 0.0F, 1.0F)
      || !InRange(input.cloud_density, 0.0F, 1.0F)
      || !InRange(input.weather_density, 0.0F, 1.0F)
      || !InRange(input.cloud_type, 0.0F, 1.0F)
      || !InRange(input.night_factor, 0.0F, 1.0F)) {
    return CloudVolumeDiagnostic::control_out_of_range;
  }
  if (!std::isfinite(input.cloud_base_height)
      || !std::isfinite(input.cloud_top_height)) {
    return CloudVolumeDiagnostic::layer_non_finite;
  }
  if (input.cloud_base_height < 0.10F
      || input.cloud_top_height > 20.0F
      || input.cloud_top_height - input.cloud_base_height < 0.10F) {
    return CloudVolumeDiagnostic::layer_out_of_range;
  }
  if (!std::isfinite(input.max_distance)) {
    return CloudVolumeDiagnostic::max_distance_non_finite;
  }
  if (!InRange(input.max_distance, 1.0F, 200.0F)) {
    return CloudVolumeDiagnostic::max_distance_out_of_range;
  }
  if (CloudVolumePrimaryStepBudget(input.quality) == 0U
      || CloudVolumeLightStepBudget(input.quality) == 0U) {
    return CloudVolumeDiagnostic::quality_out_of_range;
  }
  return CloudVolumeDiagnostic::none;
}

[[nodiscard]] CloudVolumeVector PhaseMotion(
    const CloudVolumeInput& input) noexcept {
  const float wrapped_phase = input.phase >= 1.0F ? 0.0F : input.phase;
  const float angle = wrapped_phase * kTwoPi;
  const float phase_sine = std::sin(angle);
  const float phase_arc = 1.0F - std::cos(angle);
  return {
      (2.4F * input.wind_x * phase_sine)
          + (0.75F * input.wind_y * phase_arc),
      (2.4F * input.wind_y * phase_sine)
          - (0.75F * input.wind_x * phase_arc),
      0.22F * (input.wind_x - input.wind_y) * phase_sine,
  };
}

[[nodiscard]] float CellularDistance(const CloudVolumeVector point) noexcept {
  const auto base_x = static_cast<std::int32_t>(std::floor(point.x));
  const auto base_y = static_cast<std::int32_t>(std::floor(point.y));
  const auto base_z = static_cast<std::int32_t>(std::floor(point.z));
  float minimum_squared = std::numeric_limits<float>::max();
  for (std::int32_t offset_z = -1; offset_z <= 1; ++offset_z) {
    for (std::int32_t offset_y = -1; offset_y <= 1; ++offset_y) {
      for (std::int32_t offset_x = -1; offset_x <= 1; ++offset_x) {
        const std::int32_t lattice_x = base_x + offset_x;
        const std::int32_t lattice_y = base_y + offset_y;
        const std::int32_t lattice_z = base_z + offset_z;
        const CloudVolumeVector feature{
            static_cast<float>(lattice_x)
                + detail::SkyFieldLatticeHash3D(lattice_x, lattice_y, lattice_z),
            static_cast<float>(lattice_y)
                + detail::SkyFieldLatticeHash3D(lattice_x + 37,
                                                lattice_y - 17,
                                                lattice_z + 53),
            static_cast<float>(lattice_z)
                + detail::SkyFieldLatticeHash3D(lattice_x - 29,
                                                lattice_y + 71,
                                                lattice_z + 11),
        };
        const float delta_x = feature.x - point.x;
        const float delta_y = feature.y - point.y;
        const float delta_z = feature.z - point.z;
        minimum_squared = std::min(
            minimum_squared,
            (delta_x * delta_x) + (delta_y * delta_y) + (delta_z * delta_z));
      }
    }
  }
  return std::sqrt(minimum_squared);
}

[[nodiscard]] float SampleDensityInternal(
    const CloudVolumeInput& input,
    const CloudVolumeVector position,
    const bool detailed) noexcept {
  if (input.cloud_coverage == 0.0F || input.cloud_density == 0.0F
      || position.z <= input.cloud_base_height
      || position.z >= input.cloud_top_height) {
    return 0.0F;
  }

  const CloudVolumeVector motion = PhaseMotion(input);
  const CloudVolumeVector advected = Add(position, motion);
  const float weather_low = Noise({
      (0.045F * advected.x) + 12.7F,
      (0.045F * advected.y) - 8.2F,
      4.3F,
  });
  const float weather_fold = Noise({
      (0.095F * advected.x) - 5.1F,
      (0.095F * advected.y) + 17.6F,
      -9.4F,
  });
  const float weather_field = (0.68F * weather_low) + (0.32F * weather_fold);
  const float local_coverage = std::clamp(
      input.cloud_coverage
          + (0.50F * (weather_field - 0.5F))
          + (0.12F * input.weather_density),
      0.0F,
      1.0F);
  const float local_type = std::clamp(
      input.cloud_type + (0.24F * (weather_fold - 0.5F)),
      0.0F,
      1.0F);

  const CloudVolumeVector body_point{
      (0.22F * advected.x) + 2.7F,
      (0.22F * advected.y) - 6.1F,
      (0.46F * advected.z) + 9.3F,
  };
  const float body_broad = Noise(body_point);
  const float body_middle = Noise({
      (1.93F * body_point.x) - 11.2F,
      (1.93F * body_point.y) + 7.8F,
      (1.93F * body_point.z) - 3.5F,
  });
  const float body_breakup = Noise({
      (3.87F * body_point.x) + 5.6F,
      (3.87F * body_point.y) - 13.1F,
      (3.87F * body_point.z) + 8.4F,
  });
  const float body = std::clamp(
      (0.52F * body_broad) + (0.31F * body_middle) + (0.17F * body_breakup),
      0.0F,
      1.0F);
  const float threshold = LinearInterpolate(0.76F, 0.30F, local_coverage);
  const float occupied = SmoothStep(threshold - 0.14F,
                                    threshold + 0.18F,
                                    body);
  const float normalized_height = (position.z - input.cloud_base_height)
      / (input.cloud_top_height - input.cloud_base_height);
  const float vertical_profile = CloudVolumeVerticalProfile(normalized_height,
                                                             local_type);
  const float base_support = occupied * vertical_profile;
  if (base_support <= 0.002F) {
    return 0.0F;
  }
  const float weather_scale = 0.62F + (0.58F * input.weather_density);
  const float coarse_density = std::clamp(
      base_support * input.cloud_density * weather_scale * 0.82F,
      0.0F,
      1.0F);
  if (!detailed) {
    return coarse_density;
  }
  const CloudVolumeVector camera_delta{
      position.x - input.camera_position.x,
      position.y - input.camera_position.y,
      position.z - input.camera_position.z,
  };
  if (input.night_factor >= 0.75F && Dot(camera_delta, camera_delta) > 400.0F) {
    return coarse_density;
  }

  const float detail_noise = Noise({
      (1.35F * advected.x) - 4.8F,
      (1.35F * advected.y) + 3.2F,
      (1.75F * advected.z) + 7.7F,
  });
  const float cellular_scale = LinearInterpolate(1.15F, 1.75F, local_type);
  const float cellular_distance = CellularDistance({
      (cellular_scale * advected.x) + 6.3F,
      (cellular_scale * advected.y) - 9.7F,
      (1.25F * cellular_scale * advected.z) + 2.1F,
  });
  const float cellular_support = 1.0F - SmoothStep(0.24F,
                                                   0.96F,
                                                   cellular_distance);
  const float cellular_weight = 0.42F
      * LinearInterpolate(1.0F, 0.45F, input.night_factor);
  float detail_support = ((1.0F - cellular_weight) * detail_noise)
      + (cellular_weight * cellular_support);
  detail_support = LinearInterpolate(detail_support,
                                    0.70F,
                                    0.42F * input.night_factor);
  const float erosion = 0.28F * (1.0F - cellular_support)
      * LinearInterpolate(1.0F, 0.68F, input.weather_density)
      * LinearInterpolate(1.0F, 0.62F, input.night_factor);

  const float sculpted = std::max(occupied - erosion, 0.0F)
      * LinearInterpolate(0.62F, 1.0F, detail_support);
  return std::clamp(
      sculpted
          * vertical_profile
          * input.cloud_density
          * weather_scale,
      0.0F,
      1.0F);
}

[[nodiscard]] bool OutputIsFinite(const CloudVolumeOutput& output) noexcept {
  return IsFinite(output.radiance)
      && std::isfinite(output.transmittance)
      && std::isfinite(output.optical_depth);
}

[[nodiscard]] bool OutputIsInRange(const CloudVolumeOutput& output) noexcept {
  return InRange(output.radiance.x, 0.0F, kCloudVolumeMaximumRadiance)
      && InRange(output.radiance.y, 0.0F, kCloudVolumeMaximumRadiance)
      && InRange(output.radiance.z, 0.0F, kCloudVolumeMaximumRadiance)
      && InRange(output.transmittance, 0.0F, 1.0F)
      && InRange(output.optical_depth, 0.0F, kCloudVolumeMaximumOpticalDepth);
}

}  // namespace

bool IntersectCloudVolumeLayer(
    const float camera_height,
    const float view_vertical,
    const float cloud_base_height,
    const float cloud_top_height,
    const float max_distance,
    CloudVolumeLayerIntersection& intersection) noexcept {
  if (!std::isfinite(camera_height)
      || !std::isfinite(view_vertical)
      || !std::isfinite(cloud_base_height)
      || !std::isfinite(cloud_top_height)
      || !std::isfinite(max_distance)
      || cloud_top_height <= cloud_base_height
      || max_distance <= 0.0F
      || std::fabs(view_vertical) < 1.0e-6F) {
    return false;
  }
  const float first = (cloud_base_height - camera_height) / view_vertical;
  const float second = (cloud_top_height - camera_height) / view_vertical;
  const float near_distance = std::max(std::min(first, second), 0.0F);
  const float far_distance = std::min(std::max(first, second), max_distance);
  if (!std::isfinite(near_distance)
      || !std::isfinite(far_distance)
      || far_distance <= near_distance) {
    return false;
  }
  intersection = {near_distance, far_distance};
  return true;
}

float CloudVolumeInterleavedJitter(
    const std::uint32_t pixel_x,
    const std::uint32_t pixel_y,
    const std::uint32_t frame) noexcept {
  const std::uint32_t seed = (pixel_x * 0x8DA6B343U)
      ^ (pixel_y * 0xD8163841U)
      ^ (frame * 0xCB1AB31FU)
      ^ 0xA511E9B3U;
  return static_cast<float>(MixBits(seed) & 0x00FFFFFFU) / 16777216.0F;
}

float CloudVolumeVerticalProfile(
    const float normalized_height,
    const float cloud_type) noexcept {
  if (!std::isfinite(normalized_height)
      || !std::isfinite(cloud_type)
      || normalized_height <= 0.0F
      || normalized_height >= 1.0F) {
    return 0.0F;
  }
  const float height = std::clamp(normalized_height, 0.0F, 1.0F);
  const float type = std::clamp(cloud_type, 0.0F, 1.0F);
  const float stratus = SmoothStep(0.0F, 0.08F, height)
      * (1.0F - SmoothStep(0.58F, 0.82F, height));
  const float cumulus = SmoothStep(0.0F, 0.18F, height)
      * (1.0F - SmoothStep(0.76F, 1.0F, height));
  const float anvil_body = SmoothStep(0.0F, 0.12F, height)
      * (1.0F - SmoothStep(0.90F, 1.0F, height));
  const float anvil_shelf = 0.58F + (0.42F * SmoothStep(0.48F, 0.68F, height));
  const float anvil = anvil_body * anvil_shelf;
  if (type <= 0.5F) {
    return LinearInterpolate(stratus, cumulus, type * 2.0F);
  }
  return LinearInterpolate(cumulus, anvil, (type - 0.5F) * 2.0F);
}

CloudVolumeEvaluation SampleCloudVolumeDensity(
    const CloudVolumeInput& input,
    const CloudVolumeVector& position,
    float& density) noexcept {
  const CloudVolumeDiagnostic diagnostic = ValidateInput(input);
  if (diagnostic != CloudVolumeDiagnostic::none) {
    return Reject(diagnostic);
  }
  if (!IsFinite(position)) {
    return Reject(CloudVolumeDiagnostic::calculation_non_finite);
  }
  if (!InRange(position.x, -20000.0F, 20000.0F)
      || !InRange(position.y, -20000.0F, 20000.0F)
      || !InRange(position.z, -20.0F, 40.0F)) {
    return Reject(CloudVolumeDiagnostic::calculation_out_of_range);
  }
  const float candidate = SampleDensityInternal(input, position, true);
  if (!std::isfinite(candidate)) {
    return Reject(CloudVolumeDiagnostic::calculation_non_finite);
  }
  if (!InRange(candidate, 0.0F, 1.0F)) {
    return Reject(CloudVolumeDiagnostic::calculation_out_of_range);
  }
  density = candidate;
  return {CloudVolumeStatus::evaluated, CloudVolumeDiagnostic::none};
}

CloudVolumeEvaluation EvaluateCloudVolume(
    const CloudVolumeInput& input,
    CloudVolumeOutput& output) noexcept {
  const CloudVolumeDiagnostic diagnostic = ValidateInput(input);
  if (diagnostic != CloudVolumeDiagnostic::none) {
    return Reject(diagnostic);
  }

  CloudVolumeOutput candidate{{0.0F, 0.0F, 0.0F}, 1.0F, 0.0F, 0U, 0U};
  if (input.cloud_coverage == 0.0F || input.cloud_density == 0.0F) {
    output = candidate;
    return {CloudVolumeStatus::evaluated, CloudVolumeDiagnostic::none};
  }

  CloudVolumeLayerIntersection intersection{};
  if (!IntersectCloudVolumeLayer(input.camera_position.z,
                                 input.view_direction.z,
                                 input.cloud_base_height,
                                 input.cloud_top_height,
                                 input.max_distance,
                                 intersection)) {
    output = candidate;
    return {CloudVolumeStatus::evaluated, CloudVolumeDiagnostic::none};
  }

  const std::uint32_t primary_budget = CloudVolumePrimaryStepBudget(input.quality);
  const std::uint32_t light_budget = CloudVolumeLightStepBudget(input.quality);
  const float path_length = intersection.far_distance - intersection.near_distance;
  const float step_length = path_length / static_cast<float>(primary_budget);
  const float jitter = CloudVolumeInterleavedJitter(input.pixel_x,
                                                    input.pixel_y,
                                                    input.jitter_frame);
  float sample_distance = intersection.near_distance + (jitter * step_length);
  const float view_sun_cosine = std::clamp(Dot(input.view_direction,
                                               input.sun_direction),
                                            -1.0F,
                                            1.0F);
  const float daylight = (1.0F - input.night_factor)
      * SmoothStep(-0.08F, 0.18F, input.sun_direction.z);
  constexpr float anisotropy = 0.65F;
  constexpr float anisotropy_squared = anisotropy * anisotropy;
  const float phase_denominator = std::max(
      1.0F + anisotropy_squared
          - (2.0F * anisotropy * view_sun_cosine),
      0.035F);
  const float forward_phase = std::clamp(
      (1.0F - anisotropy_squared)
          / (phase_denominator * std::sqrt(phase_denominator)),
      0.0F,
      4.0F);

  for (std::uint32_t step_index = 0; step_index < primary_budget; ++step_index) {
    if (sample_distance >= intersection.far_distance) {
      break;
    }
    ++candidate.primary_steps;
    const CloudVolumeVector position = Add(
        input.camera_position,
        Scale(input.view_direction, sample_distance));
    const float density = SampleDensityInternal(input, position, true);
    if (density > 0.002F) {
      CloudVolumeLayerIntersection light_intersection{};
      float shadow_optical_depth{};
      if (IntersectCloudVolumeLayer(position.z,
                                    input.sun_direction.z,
                                    input.cloud_base_height,
                                    input.cloud_top_height,
                                    12.0F,
                                    light_intersection)) {
        const float light_path = light_intersection.far_distance
            - light_intersection.near_distance;
        const float light_step_length = light_path / static_cast<float>(light_budget);
        for (std::uint32_t light_index = 0; light_index < light_budget; ++light_index) {
          const float light_distance = light_intersection.near_distance
              + ((static_cast<float>(light_index) + 0.5F) * light_step_length);
          const CloudVolumeVector light_position = Add(
              position,
              Scale(input.sun_direction, light_distance));
          shadow_optical_depth += SampleDensityInternal(input, light_position, false)
              * kShadowExtinctionScale
              * light_step_length;
          ++candidate.light_samples;
        }
      }
      const float sun_transmittance = std::exp(-std::min(shadow_optical_depth, 16.0F));
      const float powder = 1.0F - std::exp(-2.4F * density);
      const float silver_lining = SmoothStep(0.72F, 0.995F, view_sun_cosine)
          * (1.0F - sun_transmittance)
          * powder
          * LinearInterpolate(1.0F, 0.12F, input.weather_density);
      const float multiple_scattering = 0.055F
          + (0.18F
             * (1.0F - sun_transmittance)
             * LinearInterpolate(1.0F, 0.50F, input.weather_density))
          + (0.10F * powder);
      const float ambient = 0.08F
          + (0.12F * (1.0F - input.weather_density))
          + (0.025F * input.night_factor)
          + multiple_scattering;
      const float core_darkening = 1.0F
          - (0.52F
             * input.weather_density
             * powder
             * (0.55F + (0.45F * (1.0F - sun_transmittance))));
      const float normalized_height = (position.z - input.cloud_base_height)
          / (input.cloud_top_height - input.cloud_base_height);
      const float top_lighting = LinearInterpolate(
          1.0F,
          0.52F + (0.48F * SmoothStep(0.08F, 0.92F, normalized_height)),
          input.weather_density);
      const float direct = daylight
          * ((sun_transmittance
              * (0.16F + (0.24F * forward_phase))
              * LinearInterpolate(1.0F, 0.45F, input.weather_density))
             + (0.55F * silver_lining));

      const CloudVolumeVector day_tint{1.00F, 0.98F, 0.94F};
      const CloudVolumeVector storm_tint{0.50F, 0.57F, 0.68F};
      const CloudVolumeVector night_tint{0.12F, 0.15F, 0.20F};
      const CloudVolumeVector weather_tint = LinearInterpolate(day_tint,
                                                                storm_tint,
                                                                input.weather_density);
      const CloudVolumeVector tint = LinearInterpolate(weather_tint,
                                                       night_tint,
                                                       input.night_factor);
      const float step_optical_depth = density * kExtinctionScale * step_length;
      const float segment_transmittance = std::exp(-step_optical_depth);
      const float segment_opacity = 1.0F - segment_transmittance;
      const CloudVolumeVector source = Scale(
          tint,
          (ambient * core_darkening * top_lighting) + direct);
      candidate.radiance = Add(
          candidate.radiance,
          Scale(source, candidate.transmittance * segment_opacity));
      candidate.transmittance *= segment_transmittance;
      candidate.optical_depth = std::min(
          candidate.optical_depth + step_optical_depth,
          kCloudVolumeMaximumOpticalDepth);
      if (candidate.transmittance <= kMinimumTransmittance) {
        break;
      }
    }
    sample_distance += step_length;
  }

  if (!OutputIsFinite(candidate)) {
    return Reject(CloudVolumeDiagnostic::calculation_non_finite);
  }
  if (!OutputIsInRange(candidate)
      || candidate.primary_steps > primary_budget
      || candidate.light_samples > primary_budget * light_budget) {
    return Reject(CloudVolumeDiagnostic::calculation_out_of_range);
  }
  output = candidate;
  return {CloudVolumeStatus::evaluated, CloudVolumeDiagnostic::none};
}

}  // namespace truth::render
