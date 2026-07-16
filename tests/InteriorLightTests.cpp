#include "truth/render/InteriorLight.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string_view>

namespace {

using truth::render::EvaluateInteriorLight;
using truth::render::InteriorAperture;
using truth::render::InteriorLightDiagnostic;
using truth::render::InteriorLightInput;
using truth::render::InteriorLightOutput;
using truth::render::InteriorLightStatus;
using truth::render::kMaxInteriorApertures;
using truth::render::kMaximumInteriorLight;

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

[[nodiscard]] bool Near(const float lhs, const float rhs, const float tolerance = 1.0e-4F) noexcept {
  return std::fabs(lhs - rhs) <= tolerance;
}

[[nodiscard]] bool SameFloatBits(const float lhs, const float rhs) noexcept {
  return std::bit_cast<std::uint32_t>(lhs) == std::bit_cast<std::uint32_t>(rhs);
}

[[nodiscard]] InteriorLightInput OpenRoom() noexcept {
  InteriorLightInput input{};
  input.exterior_sky_luminance = 100.0F;
  input.ambient_floor = 2.0F;
  input.occlusion = 0.0F;
  input.aperture_count = 1U;
  input.apertures[0] = InteriorAperture{1.0F, 1.0F};
  return input;
}

void StableCodesAreExplicit(TestContext& context) {
  context.expect(static_cast<std::uint32_t>(InteriorLightStatus::evaluated) == 0U, "evaluated code changed");
  context.expect(static_cast<std::uint32_t>(InteriorLightStatus::rejected) == 1U, "rejected code changed");
  context.expect(static_cast<std::uint32_t>(InteriorLightDiagnostic::none) == 0U, "none code changed");
  context.expect(static_cast<std::uint32_t>(InteriorLightDiagnostic::occlusion_out_of_range) == 121U,
                 "occlusion diagnostic changed");
  context.expect(static_cast<std::uint32_t>(InteriorLightDiagnostic::aperture_count_out_of_range) == 130U,
                 "aperture-count diagnostic changed");
}

void OpenRoomReceivesClampedDaylight(TestContext& context) {
  InteriorLightOutput output{};
  const auto result = EvaluateInteriorLight(output, OpenRoom());

  context.expect(result.status == InteriorLightStatus::evaluated, "open room was not evaluated");
  context.expect(Near(output.effective_aperture, 1.0F), "open aperture was not full");
  context.expect(Near(output.exterior_daylight, 100.0F), "open daylight was wrong");
  context.expect(Near(output.interior_light, 102.0F), "open interior light was wrong");
  context.expect(!output.exterior_excluded, "open room was wrongly excluded");
}

void BasementReceivesNoExteriorDaylight(TestContext& context) {
  // The exact flaw a whole-cell ambient model gets wrong: a windowless cell.
  InteriorLightInput input{};
  input.exterior_sky_luminance = 100.0F;
  input.ambient_floor = 2.0F;
  input.occlusion = 0.0F;
  input.aperture_count = 0U;

  InteriorLightOutput output{};
  const auto result = EvaluateInteriorLight(output, input);

  context.expect(result.status == InteriorLightStatus::evaluated, "basement was not evaluated");
  context.expect(SameFloatBits(output.exterior_daylight, 0.0F), "basement leaked exterior daylight");
  context.expect(Near(output.interior_light, 2.0F), "basement lost its ambient floor");
  context.expect(output.exterior_excluded, "basement was not marked excluded");
}

void FullOcclusionSealsAWindowedCell(TestContext& context) {
  InteriorLightInput input = OpenRoom();
  input.occlusion = 1.0F;  // sealed / blocked despite a nominal aperture.

  InteriorLightOutput output{};
  const auto result = EvaluateInteriorLight(output, input);

  context.expect(result.status == InteriorLightStatus::evaluated, "sealed cell was not evaluated");
  context.expect(SameFloatBits(output.exterior_daylight, 0.0F), "sealed cell leaked daylight");
  context.expect(output.exterior_excluded, "sealed cell was not excluded");
}

void ApertureSumClampsToUnity(TestContext& context) {
  InteriorLightInput input{};
  input.exterior_sky_luminance = 50.0F;
  input.ambient_floor = 0.0F;
  input.occlusion = 0.0F;
  input.aperture_count = 2U;
  input.apertures[0] = InteriorAperture{1.0F, 1.0F};
  input.apertures[1] = InteriorAperture{1.0F, 1.0F};

  InteriorLightOutput output{};
  const auto result = EvaluateInteriorLight(output, input);

  context.expect(result.status == InteriorLightStatus::evaluated, "double aperture was not evaluated");
  context.expect(Near(output.effective_aperture, 1.0F), "aperture sum did not clamp to one");
  context.expect(Near(output.exterior_daylight, 50.0F), "over-unity aperture over-lit the cell");
}

void PartialApertureAndOcclusionCompose(TestContext& context) {
  InteriorLightInput input{};
  input.exterior_sky_luminance = 100.0F;
  input.ambient_floor = 1.0F;
  input.occlusion = 0.25F;
  input.aperture_count = 1U;
  input.apertures[0] = InteriorAperture{0.5F, 0.8F};  // 0.40 open

  InteriorLightOutput output{};
  const auto result = EvaluateInteriorLight(output, input);

  context.expect(result.status == InteriorLightStatus::evaluated, "partial cell was not evaluated");
  context.expect(Near(output.effective_aperture, 0.40F), "effective aperture was wrong");
  context.expect(Near(output.exterior_daylight, 30.0F), "partial daylight was wrong");  // 100*0.4*0.75
  context.expect(Near(output.interior_light, 31.0F), "partial interior light was wrong");
  context.expect(!output.exterior_excluded, "partial cell was wrongly excluded");
}

void InteriorLightClampsAtCeiling(TestContext& context) {
  InteriorLightInput input = OpenRoom();
  input.exterior_sky_luminance = kMaximumInteriorLight;
  input.ambient_floor = kMaximumInteriorLight;

  InteriorLightOutput output{};
  const auto result = EvaluateInteriorLight(output, input);

  context.expect(result.status == InteriorLightStatus::evaluated, "ceiling case was not evaluated");
  context.expect(Near(output.interior_light, kMaximumInteriorLight, 1.0F), "interior light did not clamp");
}

void InvalidInputPreservesOutputBitForBit(TestContext& context) {
  InteriorLightOutput output{};
  output.interior_light = 7.0F;
  output.exterior_daylight = 8.0F;
  output.effective_aperture = 0.5F;
  output.exterior_excluded = true;

  InteriorLightInput input = OpenRoom();
  input.exterior_sky_luminance = std::nanf("");

  const auto result = EvaluateInteriorLight(output, input);

  context.expect(result.status == InteriorLightStatus::rejected, "NaN sky was not rejected");
  context.expect(result.diagnostic == InteriorLightDiagnostic::exterior_sky_luminance_non_finite,
                 "wrong diagnostic for NaN sky");
  context.expect(SameFloatBits(output.interior_light, 7.0F), "rejected path mutated interior light");
  context.expect(SameFloatBits(output.exterior_daylight, 8.0F), "rejected path mutated daylight");
  context.expect(SameFloatBits(output.effective_aperture, 0.5F), "rejected path mutated aperture");
  context.expect(output.exterior_excluded, "rejected path mutated exclusion flag");
}

void OutOfRangeOcclusionRejects(TestContext& context) {
  InteriorLightInput input = OpenRoom();
  input.occlusion = 1.5F;

  InteriorLightOutput output{};
  const auto result = EvaluateInteriorLight(output, input);

  context.expect(result.status == InteriorLightStatus::rejected, "out-of-range occlusion was accepted");
  context.expect(result.diagnostic == InteriorLightDiagnostic::occlusion_out_of_range,
                 "wrong diagnostic for occlusion");
}

void TooManyAperturesRejects(TestContext& context) {
  InteriorLightInput input = OpenRoom();
  input.aperture_count = kMaxInteriorApertures + 1U;

  InteriorLightOutput output{};
  const auto result = EvaluateInteriorLight(output, input);

  context.expect(result.status == InteriorLightStatus::rejected, "excess aperture count was accepted");
  context.expect(result.diagnostic == InteriorLightDiagnostic::aperture_count_out_of_range,
                 "wrong diagnostic for aperture count");
}

struct TestCase {
  const char* name;
  void (*function)(TestContext&);
};

constexpr TestCase kTests[] = {
    {"stable codes are explicit", &StableCodesAreExplicit},
    {"open room receives clamped daylight", &OpenRoomReceivesClampedDaylight},
    {"basement receives no exterior daylight", &BasementReceivesNoExteriorDaylight},
    {"full occlusion seals a windowed cell", &FullOcclusionSealsAWindowedCell},
    {"aperture sum clamps to unity", &ApertureSumClampsToUnity},
    {"partial aperture and occlusion compose", &PartialApertureAndOcclusionCompose},
    {"interior light clamps at ceiling", &InteriorLightClampsAtCeiling},
    {"invalid input preserves output bit-for-bit", &InvalidInputPreservesOutputBitForBit},
    {"out-of-range occlusion rejects", &OutOfRangeOcclusionRejects},
    {"too many apertures rejects", &TooManyAperturesRejects},
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

  std::cout << "Truth interior-light C++ cases: " << passed << '/' << std::size(kTests)
            << "; assertions: " << context.assertions << '\n';
  return 0;
}
