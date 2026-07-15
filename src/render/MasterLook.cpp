#include "truth/render/MasterLook.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace truth::render {
namespace {

[[nodiscard]] bool InRange(const float value, const float minimum, const float maximum) noexcept {
  return value >= minimum && value <= maximum;
}

[[nodiscard]] UpdateResult Reject(const DiagnosticCode diagnostic) noexcept {
  return {UpdateStatus::rejected, diagnostic};
}

[[nodiscard]] DiagnosticCode ValidateSample(const AtmosphereSample& sample) noexcept {
  if (!std::isfinite(sample.scene_luminance)) {
    return DiagnosticCode::scene_luminance_non_finite;
  }
  if (!InRange(sample.scene_luminance, kMinimumLuminance, kMaximumLuminance)) {
    return DiagnosticCode::scene_luminance_out_of_range;
  }
  if (!std::isfinite(sample.sky_luminance)) {
    return DiagnosticCode::sky_luminance_non_finite;
  }
  if (!InRange(sample.sky_luminance, kMinimumLuminance, kMaximumLuminance)) {
    return DiagnosticCode::sky_luminance_out_of_range;
  }
  if (!std::isfinite(sample.interior_factor)) {
    return DiagnosticCode::interior_factor_non_finite;
  }
  if (!InRange(sample.interior_factor, kMinimumInteriorFactor, kMaximumInteriorFactor)) {
    return DiagnosticCode::interior_factor_out_of_range;
  }
  if (!std::isfinite(sample.delta_seconds)) {
    return DiagnosticCode::delta_seconds_non_finite;
  }
  if (sample.delta_seconds <= kMinimumDeltaSecondsExclusive
      || sample.delta_seconds > kMaximumDeltaSeconds) {
    return DiagnosticCode::delta_seconds_out_of_range;
  }
  return DiagnosticCode::none;
}

[[nodiscard]] DiagnosticCode ValidateState(const MasterLookState& state) noexcept {
  if (!std::isfinite(state.exposure_ev)) {
    return DiagnosticCode::exposure_ev_non_finite;
  }
  if (!InRange(state.exposure_ev, kMinimumExposureEv, kMaximumExposureEv)) {
    return DiagnosticCode::exposure_ev_out_of_range;
  }
  if (!std::isfinite(state.target_exposure_ev)) {
    return DiagnosticCode::target_exposure_ev_non_finite;
  }
  if (!InRange(state.target_exposure_ev, kMinimumExposureEv, kMaximumExposureEv)) {
    return DiagnosticCode::target_exposure_ev_out_of_range;
  }
  if (state.validity != StateValidity::invalid && state.validity != StateValidity::valid) {
    return DiagnosticCode::state_validity_invalid;
  }
  return DiagnosticCode::none;
}

}  // namespace

float UnifiedLuminance(const AtmosphereSample& sample) noexcept {
  const float exterior_luminance = (0.75F * sample.scene_luminance) + (0.25F * sample.sky_luminance);
  return exterior_luminance
      + ((sample.scene_luminance - exterior_luminance) * sample.interior_factor);
}

float TargetExposureEv(const AtmosphereSample& sample) noexcept {
  const float metered_luminance = std::max(UnifiedLuminance(sample), kLuminanceFloor);
  return std::clamp(std::log2(kMiddleGray / metered_luminance),
                    kMinimumExposureEv,
                    kMaximumExposureEv);
}

float FilmicToneCurve(const float linear_value) noexcept {
  if (std::isnan(linear_value) || linear_value <= 0.0F) {
    return 0.0F;
  }
  if (!std::isfinite(linear_value) || linear_value >= kFilmicLinearWhite) {
    return 1.0F;
  }

  constexpr float white_squared = kFilmicLinearWhite * kFilmicLinearWhite;
  return (linear_value * (1.0F + (linear_value / white_squared))) / (1.0F + linear_value);
}

UpdateResult Update(MasterLookState& state, const AtmosphereSample& sample) noexcept {
  const DiagnosticCode sample_diagnostic = ValidateSample(sample);
  if (sample_diagnostic != DiagnosticCode::none) {
    return Reject(sample_diagnostic);
  }

  const DiagnosticCode state_diagnostic = ValidateState(state);
  if (state_diagnostic != DiagnosticCode::none) {
    return Reject(state_diagnostic);
  }

  if (sample.discontinuity
      && state.history_epoch == std::numeric_limits<std::uint64_t>::max()) {
    return Reject(DiagnosticCode::history_epoch_overflow);
  }

  const float target_exposure_ev = TargetExposureEv(sample);
  if (!std::isfinite(target_exposure_ev)) {
    return Reject(DiagnosticCode::calculation_non_finite);
  }

  MasterLookState candidate = state;
  candidate.target_exposure_ev = target_exposure_ev;

  if (state.validity == StateValidity::invalid) {
    candidate.exposure_ev = target_exposure_ev;
    candidate.validity = StateValidity::valid;
    if (sample.discontinuity) {
      ++candidate.history_epoch;
      state = candidate;
      return {UpdateStatus::snapped, DiagnosticCode::none};
    }
    state = candidate;
    return {UpdateStatus::initialized, DiagnosticCode::none};
  }

  if (sample.discontinuity) {
    candidate.exposure_ev = target_exposure_ev;
    ++candidate.history_epoch;
    state = candidate;
    return {UpdateStatus::snapped, DiagnosticCode::none};
  }

  const float difference_ev = target_exposure_ev - state.exposure_ev;
  const float minimum_step_ev = -kDarkenRateEvPerSecond * sample.delta_seconds;
  const float maximum_step_ev = kBrightenRateEvPerSecond * sample.delta_seconds;
  const float step_ev = std::clamp(difference_ev, minimum_step_ev, maximum_step_ev);
  candidate.exposure_ev = std::clamp(state.exposure_ev + step_ev,
                                     kMinimumExposureEv,
                                     kMaximumExposureEv);

  if (!std::isfinite(candidate.exposure_ev)) {
    return Reject(DiagnosticCode::calculation_non_finite);
  }

  state = candidate;
  return {UpdateStatus::updated, DiagnosticCode::none};
}

}  // namespace truth::render
