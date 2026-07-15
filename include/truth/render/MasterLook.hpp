#pragma once

#include <cstdint>

namespace truth::render {

inline constexpr float kMinimumLuminance = 0.0F;
inline constexpr float kMaximumLuminance = 1'000'000.0F;
inline constexpr float kMinimumInteriorFactor = 0.0F;
inline constexpr float kMaximumInteriorFactor = 1.0F;
inline constexpr float kMinimumDeltaSecondsExclusive = 0.0F;
inline constexpr float kMaximumDeltaSeconds = 1.0F;
inline constexpr float kMinimumExposureEv = -16.0F;
inline constexpr float kMaximumExposureEv = 16.0F;
inline constexpr float kBrightenRateEvPerSecond = 3.0F;
inline constexpr float kDarkenRateEvPerSecond = 1.5F;
inline constexpr float kMiddleGray = 0.18F;
inline constexpr float kLuminanceFloor = 0.0001F;
inline constexpr float kFilmicLinearWhite = 4.0F;

struct AtmosphereSample {
  float scene_luminance;
  float sky_luminance;
  float interior_factor;
  float delta_seconds;
  bool discontinuity;
};

enum class StateValidity : std::uint32_t {
  invalid = 0U,
  valid = 1U,
};

struct MasterLookState {
  float exposure_ev;
  float target_exposure_ev;
  std::uint64_t history_epoch;
  StateValidity validity;
};

enum class UpdateStatus : std::uint32_t {
  updated = 0U,
  initialized = 1U,
  snapped = 2U,
  rejected = 3U,
};

enum class DiagnosticCode : std::uint32_t {
  none = 0U,
  scene_luminance_non_finite = 100U,
  scene_luminance_out_of_range = 101U,
  sky_luminance_non_finite = 110U,
  sky_luminance_out_of_range = 111U,
  interior_factor_non_finite = 120U,
  interior_factor_out_of_range = 121U,
  delta_seconds_non_finite = 130U,
  delta_seconds_out_of_range = 131U,
  exposure_ev_non_finite = 140U,
  exposure_ev_out_of_range = 141U,
  target_exposure_ev_non_finite = 150U,
  target_exposure_ev_out_of_range = 151U,
  state_validity_invalid = 160U,
  history_epoch_overflow = 170U,
  calculation_non_finite = 180U,
};

struct UpdateResult {
  UpdateStatus status;
  DiagnosticCode diagnostic;
};

[[nodiscard]] float UnifiedLuminance(const AtmosphereSample& sample) noexcept;
[[nodiscard]] float TargetExposureEv(const AtmosphereSample& sample) noexcept;
[[nodiscard]] float FilmicToneCurve(float linear_value) noexcept;
[[nodiscard]] UpdateResult Update(MasterLookState& state, const AtmosphereSample& sample) noexcept;

}  // namespace truth::render
