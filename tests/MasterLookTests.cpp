#include "truth/render/MasterLook.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <string_view>

namespace {

using truth::render::AtmosphereSample;
using truth::render::DiagnosticCode;
using truth::render::FilmicToneCurve;
using truth::render::MasterLookState;
using truth::render::StateValidity;
using truth::render::TargetExposureEv;
using truth::render::UnifiedLuminance;
using truth::render::Update;
using truth::render::UpdateStatus;

class TestFailure final : public std::exception {
public:
  explicit TestFailure(const std::string_view message) noexcept : message_(message) {}

  [[nodiscard]] const char* what() const noexcept override {
    return message_.data();
  }

private:
  std::string_view message_;
};

struct TestContext {
  std::uint32_t assertions{};

  void expect(const bool condition, const std::string_view message) {
    ++assertions;
    if (!condition) {
      throw TestFailure{message};
    }
  }
};

[[nodiscard]] bool Near(const float lhs, const float rhs, const float tolerance = 1.0e-5F) noexcept {
  return std::fabs(lhs - rhs) <= tolerance;
}

[[nodiscard]] bool SameFloatBits(const float lhs, const float rhs) noexcept {
  return std::bit_cast<std::uint32_t>(lhs) == std::bit_cast<std::uint32_t>(rhs);
}

[[nodiscard]] bool SameState(const MasterLookState& lhs, const MasterLookState& rhs) noexcept {
  return SameFloatBits(lhs.exposure_ev, rhs.exposure_ev)
      && SameFloatBits(lhs.target_exposure_ev, rhs.target_exposure_ev)
      && lhs.history_epoch == rhs.history_epoch
      && lhs.validity == rhs.validity;
}

void StableCodesAreExplicit(TestContext& context) {
  context.expect(static_cast<std::uint32_t>(UpdateStatus::updated) == 0U, "updated code changed");
  context.expect(static_cast<std::uint32_t>(UpdateStatus::initialized) == 1U, "initialized code changed");
  context.expect(static_cast<std::uint32_t>(UpdateStatus::snapped) == 2U, "snapped code changed");
  context.expect(static_cast<std::uint32_t>(UpdateStatus::rejected) == 3U, "rejected code changed");
  context.expect(static_cast<std::uint32_t>(DiagnosticCode::none) == 0U, "none diagnostic changed");
  context.expect(static_cast<std::uint32_t>(DiagnosticCode::scene_luminance_non_finite) == 100U,
                 "scene diagnostic changed");
  context.expect(static_cast<std::uint32_t>(DiagnosticCode::history_epoch_overflow) == 170U,
                 "epoch diagnostic changed");
}

void FirstSampleInitializesDeterministically(TestContext& context) {
  MasterLookState state{0.0F, 0.0F, 11U, StateValidity::invalid};
  const AtmosphereSample sample{1.0F, 1.0F, 0.35F, 1.0F / 60.0F, false};

  const auto result = Update(state, sample);
  const float expected_target = std::log2(0.18F);

  context.expect(result.status == UpdateStatus::initialized, "first sample was not initialization");
  context.expect(result.diagnostic == DiagnosticCode::none, "initialization returned a diagnostic");
  context.expect(state.validity == StateValidity::valid, "state did not become valid");
  context.expect(state.history_epoch == 11U, "initialization changed history epoch");
  context.expect(Near(UnifiedLuminance(sample), 1.0F), "unified luminance was not deterministic");
  context.expect(Near(TargetExposureEv(sample), expected_target), "target helper returned the wrong EV");
  context.expect(Near(state.exposure_ev, expected_target), "initial exposure did not snap to target");
  context.expect(Near(state.target_exposure_ev, expected_target), "target exposure was not stored");
}

void BrighteningUsesItsOwnBound(TestContext& context) {
  MasterLookState state{0.0F, 0.0F, 3U, StateValidity::valid};
  const AtmosphereSample sample{0.01125F, 0.01125F, 0.0F, 0.5F, false};

  const auto result = Update(state, sample);

  context.expect(result.status == UpdateStatus::updated, "brightening was not a continuous update");
  context.expect(Near(state.target_exposure_ev, 4.0F), "brightening target was unexpected");
  context.expect(Near(state.exposure_ev, 1.5F), "brightening exceeded or missed the 3 EV/s bound");
  context.expect(state.history_epoch == 3U, "continuous brightening changed epoch");
}

void DarkeningUsesItsOwnBound(TestContext& context) {
  MasterLookState state{0.0F, 0.0F, 4U, StateValidity::valid};
  const AtmosphereSample sample{2.88F, 2.88F, 0.0F, 0.5F, false};

  const auto result = Update(state, sample);

  context.expect(result.status == UpdateStatus::updated, "darkening was not a continuous update");
  context.expect(Near(state.target_exposure_ev, -4.0F), "darkening target was unexpected");
  context.expect(Near(state.exposure_ev, -0.75F), "darkening exceeded or missed the 1.5 EV/s bound");
  context.expect(state.history_epoch == 4U, "continuous darkening changed epoch");
}

void DiscontinuitySnapsAndAdvancesEpoch(TestContext& context) {
  MasterLookState state{0.0F, 0.0F, 7U, StateValidity::valid};
  const AtmosphereSample sample{0.045F, 0.045F, 1.0F, 0.25F, true};

  const auto result = Update(state, sample);

  context.expect(result.status == UpdateStatus::snapped, "discontinuity did not report a snap");
  context.expect(result.diagnostic == DiagnosticCode::none, "snap returned a diagnostic");
  context.expect(Near(state.target_exposure_ev, 2.0F), "snap target was unexpected");
  context.expect(Near(state.exposure_ev, 2.0F), "discontinuity did not snap current exposure");
  context.expect(state.history_epoch == 8U, "discontinuity did not advance epoch exactly once");
}

void DiscontinuousFirstSampleAlsoAdvancesEpoch(TestContext& context) {
  MasterLookState state{0.0F, 0.0F, 21U, StateValidity::invalid};
  const AtmosphereSample sample{0.045F, 0.045F, 1.0F, 0.25F, true};

  const auto result = Update(state, sample);

  context.expect(result.status == UpdateStatus::snapped, "first-sample discontinuity did not report a snap");
  context.expect(result.diagnostic == DiagnosticCode::none, "first-sample snap returned a diagnostic");
  context.expect(state.validity == StateValidity::valid, "first-sample snap did not establish valid history");
  context.expect(Near(state.exposure_ev, 2.0F), "first-sample discontinuity did not snap exposure");
  context.expect(state.history_epoch == 22U, "first-sample discontinuity did not advance epoch");
}

void InvalidSamplesNeverMutateState(TestContext& context) {
  const MasterLookState original{1.0F, 2.0F, 9U, StateValidity::valid};

  const auto reject = [&](const AtmosphereSample sample, const DiagnosticCode expected) {
    MasterLookState state = original;
    const auto result = Update(state, sample);
    context.expect(result.status == UpdateStatus::rejected, "invalid sample was not rejected");
    context.expect(result.diagnostic == expected, "invalid sample returned the wrong diagnostic");
    context.expect(SameState(state, original), "invalid sample mutated state");
  };

  const float nan = std::numeric_limits<float>::quiet_NaN();
  const float infinity = std::numeric_limits<float>::infinity();
  reject({nan, 1.0F, 0.0F, 0.1F, false}, DiagnosticCode::scene_luminance_non_finite);
  reject({-1.0F, 1.0F, 0.0F, 0.1F, false}, DiagnosticCode::scene_luminance_out_of_range);
  reject({1.0F, infinity, 0.0F, 0.1F, false}, DiagnosticCode::sky_luminance_non_finite);
  reject({1.0F, 1'000'001.0F, 0.0F, 0.1F, false}, DiagnosticCode::sky_luminance_out_of_range);
  reject({1.0F, 1.0F, nan, 0.1F, false}, DiagnosticCode::interior_factor_non_finite);
  reject({1.0F, 1.0F, 1.01F, 0.1F, false}, DiagnosticCode::interior_factor_out_of_range);
  reject({1.0F, 1.0F, 0.0F, infinity, false}, DiagnosticCode::delta_seconds_non_finite);
  reject({1.0F, 1.0F, 0.0F, 0.0F, false}, DiagnosticCode::delta_seconds_out_of_range);
  reject({1.0F, 1.0F, 0.0F, 1.01F, false}, DiagnosticCode::delta_seconds_out_of_range);
}

void InvalidStateNeverMutates(TestContext& context) {
  const AtmosphereSample sample{1.0F, 1.0F, 0.0F, 0.1F, false};

  const auto reject = [&](const MasterLookState original, const DiagnosticCode expected) {
    MasterLookState state = original;
    const auto result = Update(state, sample);
    context.expect(result.status == UpdateStatus::rejected, "invalid state was not rejected");
    context.expect(result.diagnostic == expected, "invalid state returned the wrong diagnostic");
    context.expect(SameState(state, original), "invalid state was mutated");
  };

  const float nan = std::numeric_limits<float>::quiet_NaN();
  reject({nan, 0.0F, 0U, StateValidity::valid}, DiagnosticCode::exposure_ev_non_finite);
  reject({17.0F, 0.0F, 0U, StateValidity::valid}, DiagnosticCode::exposure_ev_out_of_range);
  reject({0.0F, nan, 0U, StateValidity::valid}, DiagnosticCode::target_exposure_ev_non_finite);
  reject({0.0F, -17.0F, 0U, StateValidity::valid}, DiagnosticCode::target_exposure_ev_out_of_range);
  reject({0.0F, 0.0F, 0U, static_cast<StateValidity>(99U)}, DiagnosticCode::state_validity_invalid);
}

void EpochOverflowRejectsWithoutMutation(TestContext& context) {
  const MasterLookState original{0.0F, 0.0F, std::numeric_limits<std::uint64_t>::max(), StateValidity::valid};
  MasterLookState state = original;
  const AtmosphereSample sample{1.0F, 1.0F, 0.0F, 0.1F, true};

  const auto result = Update(state, sample);

  context.expect(result.status == UpdateStatus::rejected, "epoch overflow was not rejected");
  context.expect(result.diagnostic == DiagnosticCode::history_epoch_overflow,
                 "epoch overflow diagnostic changed");
  context.expect(SameState(state, original), "epoch overflow mutated state");
}

void FilmicCurveIsFiniteMonotonicAndCompressesHighlights(TestContext& context) {
  context.expect(SameFloatBits(FilmicToneCurve(0.0F), 0.0F), "tone curve did not map black to black");

  float previous = 0.0F;
  for (std::uint32_t index = 0; index <= 4'000U; ++index) {
    const float input = static_cast<float>(index) / 1'000.0F;
    const float output = FilmicToneCurve(input);
    context.expect(std::isfinite(output), "tone curve emitted a non-finite value");
    context.expect(output >= previous, "tone curve was not monotonic");
    if (input < 4.0F) {
      context.expect(output < 1.0F, "tone curve clipped below its white point");
    }
    previous = output;
  }

  context.expect(Near(FilmicToneCurve(4.0F), 1.0F), "tone curve missed its declared white point");
  context.expect(Near(FilmicToneCurve(4000.0F), 1.0F), "tone curve did not remain bounded");
  context.expect(std::isfinite(FilmicToneCurve(std::numeric_limits<float>::infinity())),
                 "tone curve returned non-finite output for infinity");
  context.expect(std::isfinite(FilmicToneCurve(std::numeric_limits<float>::quiet_NaN())),
                 "tone curve returned non-finite output for NaN");
  context.expect(FilmicToneCurve(2.0F) < 1.0F, "highlight compression clipped prematurely");
  context.expect((FilmicToneCurve(3.0F) - FilmicToneCurve(2.0F)) < 1.0F,
                 "highlight interval was not compressed");
}

using TestFunction = void (*)(TestContext&);

struct TestCase {
  std::string_view name;
  TestFunction function;
};

constexpr TestCase kTests[] = {
    {"stable codes are explicit", &StableCodesAreExplicit},
    {"first sample initializes deterministically", &FirstSampleInitializesDeterministically},
    {"brightening uses its own bound", &BrighteningUsesItsOwnBound},
    {"darkening uses its own bound", &DarkeningUsesItsOwnBound},
    {"discontinuity snaps and advances epoch", &DiscontinuitySnapsAndAdvancesEpoch},
    {"discontinuous first sample also advances epoch", &DiscontinuousFirstSampleAlsoAdvancesEpoch},
    {"invalid samples never mutate state", &InvalidSamplesNeverMutateState},
    {"invalid state never mutates", &InvalidStateNeverMutates},
    {"epoch overflow rejects without mutation", &EpochOverflowRejectsWithoutMutation},
    {"filmic curve is finite monotonic and compresses highlights", &FilmicCurveIsFiniteMonotonicAndCompressesHighlights},
};

}  // namespace

int main() {
  TestContext context;
  std::uint32_t passed{};

  for (const auto& test : kTests) {
    try {
      test.function(context);
      ++passed;
      std::cout << "[PASS] " << test.name << '\n';
    } catch (const std::exception& exception) {
      std::cerr << "[FAIL] " << test.name << ": " << exception.what() << '\n';
      return 1;
    }
  }

  std::cout << "Truth master-look C++ cases: " << passed << '/' << std::size(kTests)
            << "; assertions: " << context.assertions << '\n';
  return 0;
}
