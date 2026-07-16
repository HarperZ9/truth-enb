#include "truth/render/InteriorLight.hpp"

#include <algorithm>
#include <cmath>

namespace truth::render {
namespace {

[[nodiscard]] bool InRange(const float value, const float minimum, const float maximum) noexcept {
  return value >= minimum && value <= maximum;
}

[[nodiscard]] InteriorLightResult Reject(const InteriorLightDiagnostic diagnostic) noexcept {
  return {InteriorLightStatus::rejected, diagnostic};
}

[[nodiscard]] InteriorLightDiagnostic ValidateInput(const InteriorLightInput& input) noexcept {
  if (!std::isfinite(input.exterior_sky_luminance)) {
    return InteriorLightDiagnostic::exterior_sky_luminance_non_finite;
  }
  if (!InRange(input.exterior_sky_luminance, kMinimumInteriorSkyLuminance,
               kMaximumInteriorSkyLuminance)) {
    return InteriorLightDiagnostic::exterior_sky_luminance_out_of_range;
  }
  if (!std::isfinite(input.ambient_floor)) {
    return InteriorLightDiagnostic::ambient_floor_non_finite;
  }
  if (!InRange(input.ambient_floor, kMinimumInteriorSkyLuminance, kMaximumInteriorSkyLuminance)) {
    return InteriorLightDiagnostic::ambient_floor_out_of_range;
  }
  if (!std::isfinite(input.occlusion)) {
    return InteriorLightDiagnostic::occlusion_non_finite;
  }
  if (!InRange(input.occlusion, kMinimumUnitFraction, kMaximumUnitFraction)) {
    return InteriorLightDiagnostic::occlusion_out_of_range;
  }
  if (input.aperture_count > kMaxInteriorApertures) {
    return InteriorLightDiagnostic::aperture_count_out_of_range;
  }

  for (std::uint32_t index = 0U; index < input.aperture_count; ++index) {
    const InteriorAperture& aperture = input.apertures[index];
    if (!std::isfinite(aperture.sky_visibility)) {
      return InteriorLightDiagnostic::aperture_sky_visibility_non_finite;
    }
    if (!InRange(aperture.sky_visibility, kMinimumUnitFraction, kMaximumUnitFraction)) {
      return InteriorLightDiagnostic::aperture_sky_visibility_out_of_range;
    }
    if (!std::isfinite(aperture.transmittance)) {
      return InteriorLightDiagnostic::aperture_transmittance_non_finite;
    }
    if (!InRange(aperture.transmittance, kMinimumUnitFraction, kMaximumUnitFraction)) {
      return InteriorLightDiagnostic::aperture_transmittance_out_of_range;
    }
  }

  return InteriorLightDiagnostic::none;
}

}  // namespace

InteriorLightResult EvaluateInteriorLight(InteriorLightOutput& output,
                                          const InteriorLightInput& input) noexcept {
  const InteriorLightDiagnostic diagnostic = ValidateInput(input);
  if (diagnostic != InteriorLightDiagnostic::none) {
    return Reject(diagnostic);
  }

  float aperture_sum = 0.0F;
  for (std::uint32_t index = 0U; index < input.aperture_count; ++index) {
    const InteriorAperture& aperture = input.apertures[index];
    aperture_sum += aperture.sky_visibility * aperture.transmittance;
  }

  const float effective_aperture =
      std::clamp(aperture_sum, kMinimumUnitFraction, kMaximumUnitFraction);
  const float open_factor = effective_aperture * (kMaximumUnitFraction - input.occlusion);
  const float exterior_daylight = input.exterior_sky_luminance * open_factor;
  const float interior_light = std::clamp(input.ambient_floor + exterior_daylight,
                                          kMinimumInteriorSkyLuminance,
                                          kMaximumInteriorLight);

  if (!std::isfinite(effective_aperture) || !std::isfinite(exterior_daylight)
      || !std::isfinite(interior_light)) {
    return Reject(InteriorLightDiagnostic::calculation_non_finite);
  }

  InteriorLightOutput candidate;
  candidate.interior_light = interior_light;
  candidate.exterior_daylight = exterior_daylight;
  candidate.effective_aperture = effective_aperture;
  candidate.exterior_excluded = (open_factor == 0.0F);

  output = candidate;
  return {InteriorLightStatus::evaluated, InteriorLightDiagnostic::none};
}

}  // namespace truth::render
