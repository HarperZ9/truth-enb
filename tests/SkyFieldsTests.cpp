#include "truth/render/SkyFields.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <iterator>
#include <limits>
#include <string_view>

namespace {

using truth::render::EvaluateSkyFields;
using truth::render::SkyFieldDiagnostic;
using truth::render::SkyFieldEvaluation;
using truth::render::SkyFieldInput;
using truth::render::SkyFieldOutput;
using truth::render::SkyFieldRadiance;
using truth::render::SkyFieldStatus;

class TestFailure final : public std::exception {
public:
  explicit TestFailure(const std::string_view message) noexcept : message_(message) {}
  [[nodiscard]] const char* what() const noexcept override { return message_.data(); }

private:
  std::string_view message_;
};

struct TestContext {
  std::uint64_t assertions{};

  void expect(const bool condition, const std::string_view message) {
    ++assertions;
    if (!condition) {
      throw TestFailure{message};
    }
  }
};

[[nodiscard]] bool SameFloatBits(const float lhs, const float rhs) noexcept {
  return std::bit_cast<std::uint32_t>(lhs) == std::bit_cast<std::uint32_t>(rhs);
}

[[nodiscard]] bool SameRadiance(const SkyFieldRadiance& lhs, const SkyFieldRadiance& rhs) noexcept {
  return SameFloatBits(lhs.r, rhs.r)
      && SameFloatBits(lhs.g, rhs.g)
      && SameFloatBits(lhs.b, rhs.b);
}

[[nodiscard]] bool SameOutput(const SkyFieldOutput& lhs, const SkyFieldOutput& rhs) noexcept {
  return SameFloatBits(lhs.cloud_body, rhs.cloud_body)
      && SameFloatBits(lhs.cloud_detail_erosion, rhs.cloud_detail_erosion)
      && SameFloatBits(lhs.cloud_density, rhs.cloud_density)
      && SameFloatBits(lhs.aurora_mask, rhs.aurora_mask)
      && SameRadiance(lhs.aurora_intrinsic_radiance, rhs.aurora_intrinsic_radiance);
}

[[nodiscard]] SkyFieldInput ReferenceInput() noexcept {
  return {
      0.6F, 0.0F, 0.8F,
      0.25F,
      0.4F, -0.25F,
      0.65F, 0.8F, 0.5F,
      0.9F, 1.0F,
  };
}

void ExpectSucceeded(TestContext& context, const SkyFieldEvaluation evaluation) {
  context.expect(evaluation.status == SkyFieldStatus::evaluated, "sky-field evaluation was rejected");
  context.expect(evaluation.diagnostic == SkyFieldDiagnostic::none,
                 "successful sky-field evaluation returned a diagnostic");
}

void ExpectUnitInterval(TestContext& context, const float value, const std::string_view message) {
  context.expect(std::isfinite(value), "sky-field value was non-finite");
  context.expect(value >= 0.0F && value <= 1.0F, message);
}

void StableCodesAreExplicit(TestContext& context) {
  context.expect(static_cast<std::uint32_t>(SkyFieldStatus::evaluated) == 0U,
                 "evaluated status code changed");
  context.expect(static_cast<std::uint32_t>(SkyFieldStatus::rejected) == 1U,
                 "rejected status code changed");
  context.expect(static_cast<std::uint32_t>(SkyFieldDiagnostic::none) == 0U,
                 "none diagnostic code changed");
  context.expect(static_cast<std::uint32_t>(SkyFieldDiagnostic::view_x_non_finite) == 100U,
                 "view-x diagnostic code changed");
  context.expect(static_cast<std::uint32_t>(SkyFieldDiagnostic::view_direction_not_normalized) == 130U,
                 "view normalization diagnostic code changed");
  context.expect(static_cast<std::uint32_t>(SkyFieldDiagnostic::night_factor_out_of_range) == 211U,
                 "night-factor diagnostic code changed");
  context.expect(static_cast<std::uint32_t>(SkyFieldDiagnostic::calculation_out_of_range) == 301U,
                 "calculation diagnostic code changed");
}

void EvaluationIsBitDeterministic(TestContext& context) {
  const SkyFieldInput input = ReferenceInput();
  SkyFieldOutput first{};
  SkyFieldOutput second{};
  ExpectSucceeded(context, EvaluateSkyFields(input, first));
  ExpectSucceeded(context, EvaluateSkyFields(input, second));
  context.expect(SameOutput(first, second), "identical sky-field inputs changed output bits");
}

void InvalidInputsNeverMutateOutput(TestContext& context) {
  const SkyFieldOutput sentinel{
      -1.0F, 2.0F, -3.0F, 4.0F,
      {5.0F, -6.0F, 7.0F},
  };
  const SkyFieldInput valid = ReferenceInput();
  const auto reject = [&](const SkyFieldInput input, const SkyFieldDiagnostic expected) {
    SkyFieldOutput output = sentinel;
    const SkyFieldEvaluation evaluation = EvaluateSkyFields(input, output);
    context.expect(evaluation.status == SkyFieldStatus::rejected,
                   "invalid sky-field input was accepted");
    context.expect(evaluation.diagnostic == expected,
                   "invalid sky-field diagnostic was wrong");
    context.expect(SameOutput(output, sentinel), "invalid sky-field input mutated output");
  };

  const float nan = std::numeric_limits<float>::quiet_NaN();
  const float infinity = std::numeric_limits<float>::infinity();
  SkyFieldInput input = valid;
  input.view_x = nan;
  reject(input, SkyFieldDiagnostic::view_x_non_finite);
  input = valid; input.view_x = 1.01F;
  reject(input, SkyFieldDiagnostic::view_x_out_of_range);
  input = valid; input.view_y = infinity;
  reject(input, SkyFieldDiagnostic::view_y_non_finite);
  input = valid; input.view_y = -1.01F;
  reject(input, SkyFieldDiagnostic::view_y_out_of_range);
  input = valid; input.view_z = nan;
  reject(input, SkyFieldDiagnostic::view_z_non_finite);
  input = valid; input.view_z = 1.01F;
  reject(input, SkyFieldDiagnostic::view_z_out_of_range);
  input = valid; input.view_x = 0.5F; input.view_y = 0.5F; input.view_z = 0.5F;
  reject(input, SkyFieldDiagnostic::view_direction_not_normalized);
  input = valid; input.phase = infinity;
  reject(input, SkyFieldDiagnostic::phase_non_finite);
  input = valid; input.phase = -0.01F;
  reject(input, SkyFieldDiagnostic::phase_out_of_range);
  input = valid; input.wind_x = nan;
  reject(input, SkyFieldDiagnostic::wind_x_non_finite);
  input = valid; input.wind_x = 1.01F;
  reject(input, SkyFieldDiagnostic::wind_x_out_of_range);
  input = valid; input.wind_y = infinity;
  reject(input, SkyFieldDiagnostic::wind_y_non_finite);
  input = valid; input.wind_y = -1.01F;
  reject(input, SkyFieldDiagnostic::wind_y_out_of_range);
  input = valid; input.cloud_coverage = nan;
  reject(input, SkyFieldDiagnostic::cloud_coverage_non_finite);
  input = valid; input.cloud_coverage = 1.01F;
  reject(input, SkyFieldDiagnostic::cloud_coverage_out_of_range);
  input = valid; input.cloud_density = infinity;
  reject(input, SkyFieldDiagnostic::cloud_density_non_finite);
  input = valid; input.cloud_density = -0.01F;
  reject(input, SkyFieldDiagnostic::cloud_density_out_of_range);
  input = valid; input.weather_density = nan;
  reject(input, SkyFieldDiagnostic::weather_density_non_finite);
  input = valid; input.weather_density = 1.01F;
  reject(input, SkyFieldDiagnostic::weather_density_out_of_range);
  input = valid; input.aurora_activity = infinity;
  reject(input, SkyFieldDiagnostic::aurora_activity_non_finite);
  input = valid; input.aurora_activity = -0.01F;
  reject(input, SkyFieldDiagnostic::aurora_activity_out_of_range);
  input = valid; input.night_factor = nan;
  reject(input, SkyFieldDiagnostic::night_factor_non_finite);
  input = valid; input.night_factor = 1.01F;
  reject(input, SkyFieldDiagnostic::night_factor_out_of_range);
}

void DenseGridStaysFiniteAndBounded(TestContext& context) {
  constexpr float pi = 3.14159265358979323846F;
  for (std::uint32_t azimuth_index = 0; azimuth_index < 17U; ++azimuth_index) {
    const float azimuth = 2.0F * pi * static_cast<float>(azimuth_index) / 17.0F;
    for (std::uint32_t zenith_index = 0; zenith_index <= 8U; ++zenith_index) {
      const float view_z = 0.05F + (0.95F * static_cast<float>(zenith_index) / 8.0F);
      const float radial = std::sqrt(std::max(0.0F, 1.0F - (view_z * view_z)));
      for (std::uint32_t phase_index = 0; phase_index <= 8U; ++phase_index) {
        for (std::uint32_t control_index = 0; control_index <= 5U; ++control_index) {
          SkyFieldInput input{};
          input.view_x = radial * std::cos(azimuth);
          input.view_y = radial * std::sin(azimuth);
          input.view_z = view_z;
          input.phase = static_cast<float>(phase_index) / 8.0F;
          input.wind_x = -1.0F + (2.0F * static_cast<float>(azimuth_index) / 16.0F);
          input.wind_y = -1.0F + (2.0F * static_cast<float>(zenith_index) / 8.0F);
          input.cloud_coverage = static_cast<float>(control_index) / 5.0F;
          input.cloud_density = static_cast<float>((control_index + phase_index) % 6U) / 5.0F;
          input.weather_density = static_cast<float>((control_index + zenith_index) % 6U) / 5.0F;
          input.aurora_activity = static_cast<float>((control_index + azimuth_index) % 6U) / 5.0F;
          input.night_factor = static_cast<float>((control_index + phase_index) % 6U) / 5.0F;

          SkyFieldOutput output{};
          ExpectSucceeded(context, EvaluateSkyFields(input, output));
          ExpectUnitInterval(context, output.cloud_body, "cloud body left [0,1]");
          ExpectUnitInterval(context, output.cloud_detail_erosion,
                             "cloud detail erosion left [0,1]");
          ExpectUnitInterval(context, output.cloud_density, "cloud density left [0,1]");
          ExpectUnitInterval(context, output.aurora_mask, "aurora mask left [0,1]");
          ExpectUnitInterval(context, output.aurora_intrinsic_radiance.r,
                             "aurora intrinsic r left [0,1]");
          ExpectUnitInterval(context, output.aurora_intrinsic_radiance.g,
                             "aurora intrinsic g left [0,1]");
          ExpectUnitInterval(context, output.aurora_intrinsic_radiance.b,
                             "aurora intrinsic b left [0,1]");
        }
      }
    }
  }
}

void PhaseWrapHasAnExactSeam(TestContext& context) {
  SkyFieldInput start = ReferenceInput();
  start.phase = 0.0F;
  SkyFieldInput end = start;
  end.phase = 1.0F;
  SkyFieldOutput start_output{};
  SkyFieldOutput end_output{};
  ExpectSucceeded(context, EvaluateSkyFields(start, start_output));
  ExpectSucceeded(context, EvaluateSkyFields(end, end_output));
  context.expect(SameOutput(start_output, end_output), "phase 0 and 1 did not share an exact seam");

  SkyFieldInput after = start;
  after.phase = 0.0001F;
  SkyFieldInput before = start;
  before.phase = 0.9999F;
  SkyFieldOutput after_output{};
  SkyFieldOutput before_output{};
  ExpectSucceeded(context, EvaluateSkyFields(after, after_output));
  ExpectSucceeded(context, EvaluateSkyFields(before, before_output));
  context.expect(std::fabs(after_output.cloud_density - before_output.cloud_density) < 0.01F,
                 "cloud field was discontinuous around the phase seam");
  context.expect(std::fabs(after_output.aurora_mask - before_output.aurora_mask) < 0.01F,
                 "aurora field was discontinuous around the phase seam");
}

void CloudFieldHasBodyAndDetailVariation(TestContext& context) {
  constexpr float pi = 3.14159265358979323846F;
  SkyFieldInput input = ReferenceInput();
  input.cloud_coverage = 1.0F;
  input.cloud_density = 1.0F;
  input.weather_density = 1.0F;
  float minimum_body = 1.0F;
  float maximum_body = 0.0F;
  float minimum_erosion = 1.0F;
  float maximum_erosion = 0.0F;
  float minimum_density = 1.0F;
  float maximum_density = 0.0F;
  for (std::uint32_t index = 0; index < 32U; ++index) {
    const float azimuth = 2.0F * pi * static_cast<float>(index) / 32.0F;
    input.view_x = 0.8F * std::cos(azimuth);
    input.view_y = 0.8F * std::sin(azimuth);
    input.view_z = 0.6F;
    SkyFieldOutput output{};
    ExpectSucceeded(context, EvaluateSkyFields(input, output));
    minimum_body = std::min(minimum_body, output.cloud_body);
    maximum_body = std::max(maximum_body, output.cloud_body);
    minimum_erosion = std::min(minimum_erosion, output.cloud_detail_erosion);
    maximum_erosion = std::max(maximum_erosion, output.cloud_detail_erosion);
    minimum_density = std::min(minimum_density, output.cloud_density);
    maximum_density = std::max(maximum_density, output.cloud_density);
  }
  context.expect(maximum_body - minimum_body > 0.05F, "low-frequency cloud body was spatially flat");
  context.expect(maximum_erosion - minimum_erosion > 0.02F,
                 "cloud detail erosion was spatially flat");
  context.expect(maximum_density - minimum_density > 0.02F,
                 "composed cloud density was spatially flat");
  context.expect(maximum_erosion > 0.0F, "detail erosion never contributed");
}

void CloudControlsAreMonotonic(TestContext& context) {
  SkyFieldInput input = ReferenceInput();
  input.cloud_density = 1.0F;
  input.weather_density = 0.6F;
  float previous = 0.0F;
  for (std::uint32_t index = 0; index <= 40U; ++index) {
    input.cloud_coverage = static_cast<float>(index) / 40.0F;
    SkyFieldOutput output{};
    ExpectSucceeded(context, EvaluateSkyFields(input, output));
    context.expect(output.cloud_density + 1.0e-6F >= previous,
                   "cloud density decreased with coverage");
    previous = output.cloud_density;
  }

  input.cloud_coverage = 0.8F;
  input.cloud_density = 0.8F;
  previous = 0.0F;
  for (std::uint32_t index = 0; index <= 40U; ++index) {
    input.weather_density = static_cast<float>(index) / 40.0F;
    SkyFieldOutput output{};
    ExpectSucceeded(context, EvaluateSkyFields(input, output));
    context.expect(output.cloud_density + 1.0e-6F >= previous,
                   "cloud density decreased with weather density");
    previous = output.cloud_density;
  }

  input.weather_density = 0.75F;
  previous = 0.0F;
  for (std::uint32_t index = 0; index <= 40U; ++index) {
    input.cloud_density = static_cast<float>(index) / 40.0F;
    SkyFieldOutput output{};
    ExpectSucceeded(context, EvaluateSkyFields(input, output));
    context.expect(output.cloud_density + 1.0e-6F >= previous,
                   "cloud density decreased with density control");
    previous = output.cloud_density;
  }
}

void DayAuroraIsBitExactZero(TestContext& context) {
  SkyFieldInput input = ReferenceInput();
  input.aurora_activity = 1.0F;
  input.night_factor = 0.0F;
  SkyFieldOutput output{};
  ExpectSucceeded(context, EvaluateSkyFields(input, output));
  context.expect(SameFloatBits(output.aurora_mask, 0.0F), "day aurora mask was not exact +0");
  context.expect(SameFloatBits(output.aurora_intrinsic_radiance.r, 0.0F),
                 "day aurora intrinsic r was not exact +0");
  context.expect(SameFloatBits(output.aurora_intrinsic_radiance.g, 0.0F),
                 "day aurora intrinsic g was not exact +0");
  context.expect(SameFloatBits(output.aurora_intrinsic_radiance.b, 0.0F),
                 "day aurora intrinsic b was not exact +0");
}

void NightAuroraFormsCurtains(TestContext& context) {
  constexpr float pi = 3.14159265358979323846F;
  SkyFieldInput input = ReferenceInput();
  input.aurora_activity = 1.0F;
  input.night_factor = 1.0F;
  float minimum_mask = 1.0F;
  float maximum_mask = 0.0F;
  float maximum_green = 0.0F;
  for (std::uint32_t index = 0; index < 64U; ++index) {
    const float azimuth = 2.0F * pi * static_cast<float>(index) / 64.0F;
    input.view_x = 0.8F * std::cos(azimuth);
    input.view_y = 0.8F * std::sin(azimuth);
    input.view_z = 0.6F;
    SkyFieldOutput output{};
    ExpectSucceeded(context, EvaluateSkyFields(input, output));
    minimum_mask = std::min(minimum_mask, output.aurora_mask);
    maximum_mask = std::max(maximum_mask, output.aurora_mask);
    maximum_green = std::max(maximum_green, output.aurora_intrinsic_radiance.g);
  }
  context.expect(maximum_mask > 0.15F, "night aurora never formed a visible curtain");
  context.expect(maximum_mask - minimum_mask > 0.12F, "night aurora curtain was spatially flat");
  context.expect(maximum_green > 0.05F, "active night aurora emitted no intrinsic radiance");
}

void AuroraActivityScalesOnlyRadiance(TestContext& context) {
  SkyFieldInput inactive = ReferenceInput();
  inactive.night_factor = 1.0F;
  inactive.aurora_activity = 0.0F;
  SkyFieldInput active = inactive;
  active.aurora_activity = 1.0F;
  SkyFieldOutput inactive_output{};
  SkyFieldOutput active_output{};
  ExpectSucceeded(context, EvaluateSkyFields(inactive, inactive_output));
  ExpectSucceeded(context, EvaluateSkyFields(active, active_output));
  context.expect(SameFloatBits(inactive_output.aurora_mask, active_output.aurora_mask),
                 "aurora activity changed the spatial curtain mask");
  context.expect(SameFloatBits(inactive_output.aurora_intrinsic_radiance.r, 0.0F),
                 "inactive aurora intrinsic r was not exact +0");
  context.expect(SameFloatBits(inactive_output.aurora_intrinsic_radiance.g, 0.0F),
                 "inactive aurora intrinsic g was not exact +0");
  context.expect(SameFloatBits(inactive_output.aurora_intrinsic_radiance.b, 0.0F),
                 "inactive aurora intrinsic b was not exact +0");
  context.expect(active_output.aurora_intrinsic_radiance.g > 0.0F,
                 "active aurora intrinsic radiance stayed zero");
}

using TestFunction = void (*)(TestContext&);
struct TestCase { std::string_view name; TestFunction function; };

constexpr TestCase kTests[] = {
    {"stable codes are explicit", &StableCodesAreExplicit},
    {"evaluation is bit deterministic", &EvaluationIsBitDeterministic},
    {"invalid inputs never mutate output", &InvalidInputsNeverMutateOutput},
    {"dense grid stays finite and bounded", &DenseGridStaysFiniteAndBounded},
    {"phase wrap has an exact seam", &PhaseWrapHasAnExactSeam},
    {"cloud field has body and detail variation", &CloudFieldHasBodyAndDetailVariation},
    {"cloud controls are monotonic", &CloudControlsAreMonotonic},
    {"day aurora is bit-exact zero", &DayAuroraIsBitExactZero},
    {"night aurora forms curtains", &NightAuroraFormsCurtains},
    {"aurora activity scales only radiance", &AuroraActivityScalesOnlyRadiance},
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
  std::cout << "Truth sky-field C++ cases: " << passed << '/' << std::size(kTests)
            << "; assertions: " << context.assertions << '\n';
  return 0;
}
