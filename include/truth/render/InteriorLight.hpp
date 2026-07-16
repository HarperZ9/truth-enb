#pragma once

#include <cstdint>

namespace truth::render {

// Occlusion-aware interior daylight reference. Unlike a whole-cell ambient
// approximation, an interior space is lit by the exterior sky only through its
// window/portal apertures and only when it is not occluded. A basement (no
// aperture, or full occlusion) therefore receives no exterior daylight at all;
// it keeps only its own ambient floor. This is the CPU seed of the Helios-class
// exterior-to-interior continuity replacement.

inline constexpr std::uint32_t kMaxInteriorApertures = 8U;
inline constexpr float kMinimumInteriorSkyLuminance = 0.0F;
inline constexpr float kMaximumInteriorSkyLuminance = 1'000'000.0F;
inline constexpr float kMinimumUnitFraction = 0.0F;
inline constexpr float kMaximumUnitFraction = 1.0F;
inline constexpr float kMaximumInteriorLight = 1'000'000.0F;

// One window or portal opening that can admit exterior daylight.
struct InteriorAperture {
  float sky_visibility;  // [0,1] fraction of the opening's cone that sees sky.
  float transmittance;   // [0,1] glass/opening transmittance.
};

struct InteriorLightInput {
  float exterior_sky_luminance;  // [0, 1e6]
  float ambient_floor;           // [0, 1e6] the cell's own baseline interior light.
  float occlusion;               // [0,1]; 1 = fully occluded (basement / sealed).
  std::uint32_t aperture_count;  // <= kMaxInteriorApertures.
  InteriorAperture apertures[kMaxInteriorApertures];
};

struct InteriorLightOutput {
  float interior_light;      // ambient_floor + clamped exterior daylight.
  float exterior_daylight;   // exterior contribution alone (exactly 0 for basements).
  float effective_aperture;  // clamped [0,1] total sky-open fraction.
  bool exterior_excluded;    // true when no exterior light reaches this space.
};

enum class InteriorLightStatus : std::uint32_t {
  evaluated = 0U,
  rejected = 1U,
};

enum class InteriorLightDiagnostic : std::uint32_t {
  none = 0U,
  exterior_sky_luminance_non_finite = 100U,
  exterior_sky_luminance_out_of_range = 101U,
  ambient_floor_non_finite = 110U,
  ambient_floor_out_of_range = 111U,
  occlusion_non_finite = 120U,
  occlusion_out_of_range = 121U,
  aperture_count_out_of_range = 130U,
  aperture_sky_visibility_non_finite = 140U,
  aperture_sky_visibility_out_of_range = 141U,
  aperture_transmittance_non_finite = 150U,
  aperture_transmittance_out_of_range = 151U,
  calculation_non_finite = 180U,
};

struct InteriorLightResult {
  InteriorLightStatus status;
  InteriorLightDiagnostic diagnostic;
};

// Validate every field, compute a bounded candidate, and assign it to output
// exactly once. On any rejection the caller's output is preserved bit-for-bit.
[[nodiscard]] InteriorLightResult EvaluateInteriorLight(
    InteriorLightOutput& output, const InteriorLightInput& input) noexcept;

}  // namespace truth::render
