#include "truth/render/AuroraCurtain.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <string_view>

namespace {

using truth::render::AuroraCurtainDiagnostic;
using truth::render::AuroraCurtainEvaluation;
using truth::render::AuroraCurtainInput;
using truth::render::AuroraCurtainOutput;
using truth::render::AuroraCurtainStatus;
using truth::render::AuroraDepositionProfile;
using truth::render::AuroraQuality;
using truth::render::EvaluateAuroraCurtain;
using truth::render::EvaluateAuroraDepositionProfile;

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

[[nodiscard]] bool SameOutput(
    const AuroraCurtainOutput& lhs,
    const AuroraCurtainOutput& rhs) noexcept {
  return SameFloatBits(lhs.mask, rhs.mask)
      && SameFloatBits(lhs.intrinsic_radiance.r, rhs.intrinsic_radiance.r)
      && SameFloatBits(lhs.intrinsic_radiance.g, rhs.intrinsic_radiance.g)
      && SameFloatBits(lhs.intrinsic_radiance.b, rhs.intrinsic_radiance.b)
      && lhs.samples == rhs.samples;
}

[[nodiscard]] AuroraCurtainInput ReferenceInput() noexcept {
  return {
      0.0F, 0.0F, 0.20F,
      0.0F, 0.80F, 0.60F,
      0.63F,
      0.62F, -0.27F,
      0.82F,
      1.0F,
      AuroraQuality::balanced,
  };
}

void SetPanoramaDirection(
    AuroraCurtainInput& input,
    const float azimuth,
    const float view_z) noexcept {
  const float radial = std::sqrt(std::max(0.0F, 1.0F - (view_z * view_z)));
  input.view_x = std::sin(azimuth) * radial;
  input.view_y = std::cos(azimuth) * radial;
  input.view_z = view_z;
}

void ExpectSucceeded(
    TestContext& context,
    const AuroraCurtainEvaluation evaluation) {
  context.expect(evaluation.status == AuroraCurtainStatus::evaluated,
                 "aurora-curtain evaluation was rejected");
  context.expect(evaluation.diagnostic == AuroraCurtainDiagnostic::none,
                 "successful aurora-curtain evaluation returned a diagnostic");
}

void ExpectBounded(TestContext& context, const AuroraCurtainOutput& output) {
  const auto bounded = [&](const float value, const std::string_view message) {
    context.expect(std::isfinite(value), "aurora-curtain output was non-finite");
    context.expect(value >= 0.0F && value <= 1.0F, message);
  };
  bounded(output.mask, "aurora mask left [0,1]");
  bounded(output.intrinsic_radiance.r, "aurora red left [0,1]");
  bounded(output.intrinsic_radiance.g, "aurora green left [0,1]");
  bounded(output.intrinsic_radiance.b, "aurora blue left [0,1]");
}

void StableCodesAndBudgetsAreExplicit(TestContext& context) {
  context.expect(static_cast<std::uint32_t>(AuroraCurtainStatus::evaluated) == 0U,
                 "aurora evaluated status changed");
  context.expect(static_cast<std::uint32_t>(AuroraCurtainStatus::rejected) == 1U,
                 "aurora rejected status changed");
  context.expect(static_cast<std::uint32_t>(AuroraCurtainDiagnostic::none) == 0U,
                 "aurora none diagnostic changed");
  context.expect(static_cast<std::uint32_t>(AuroraCurtainDiagnostic::view_x_non_finite)
                     == 130U,
                 "aurora view-x diagnostic changed");
  context.expect(static_cast<std::uint32_t>(AuroraCurtainDiagnostic::quality_invalid)
                     == 250U,
                 "aurora quality diagnostic changed");
  context.expect(truth::render::AuroraSampleCount(AuroraQuality::fallback) == 1U,
                 "aurora fallback budget changed");
  context.expect(truth::render::AuroraSampleCount(AuroraQuality::low) == 4U,
                 "aurora low budget changed");
  context.expect(truth::render::AuroraSampleCount(AuroraQuality::balanced) == 7U,
                 "aurora balanced budget changed");
  context.expect(truth::render::AuroraSampleCount(AuroraQuality::high) == 10U,
                 "aurora high budget changed");
}

void InvalidInputsPreserveOutput(TestContext& context) {
  const AuroraCurtainOutput sentinel{-1.0F, {2.0F, -3.0F, 4.0F}, 99U};
  const AuroraCurtainInput valid = ReferenceInput();
  const auto reject = [&](const AuroraCurtainInput input,
                          const AuroraCurtainDiagnostic expected) {
    AuroraCurtainOutput output = sentinel;
    const auto evaluation = EvaluateAuroraCurtain(input, output);
    context.expect(evaluation.status == AuroraCurtainStatus::rejected,
                   "invalid aurora input was accepted");
    context.expect(evaluation.diagnostic == expected,
                   "invalid aurora diagnostic was wrong");
    context.expect(SameOutput(output, sentinel),
                   "invalid aurora input mutated output");
  };

  const float nan = std::numeric_limits<float>::quiet_NaN();
  const float infinity = std::numeric_limits<float>::infinity();
  AuroraCurtainInput input = valid;
  input.camera_x = nan;
  reject(input, AuroraCurtainDiagnostic::camera_x_non_finite);
  input = valid; input.camera_y = infinity;
  reject(input, AuroraCurtainDiagnostic::camera_y_non_finite);
  input = valid; input.camera_z = nan;
  reject(input, AuroraCurtainDiagnostic::camera_z_non_finite);
  input = valid; input.view_x = infinity;
  reject(input, AuroraCurtainDiagnostic::view_x_non_finite);
  input = valid; input.view_y = nan;
  reject(input, AuroraCurtainDiagnostic::view_y_non_finite);
  input = valid; input.view_z = infinity;
  reject(input, AuroraCurtainDiagnostic::view_z_non_finite);
  input = valid; input.view_x = 0.5F; input.view_y = 0.5F; input.view_z = 0.5F;
  reject(input, AuroraCurtainDiagnostic::view_direction_not_normalized);
  input = valid; input.phase = nan;
  reject(input, AuroraCurtainDiagnostic::phase_non_finite);
  input = valid; input.phase = 1.01F;
  reject(input, AuroraCurtainDiagnostic::phase_out_of_range);
  input = valid; input.wind_x = infinity;
  reject(input, AuroraCurtainDiagnostic::wind_x_non_finite);
  input = valid; input.wind_x = -1.01F;
  reject(input, AuroraCurtainDiagnostic::wind_x_out_of_range);
  input = valid; input.wind_y = nan;
  reject(input, AuroraCurtainDiagnostic::wind_y_non_finite);
  input = valid; input.wind_y = 1.01F;
  reject(input, AuroraCurtainDiagnostic::wind_y_out_of_range);
  input = valid; input.activity = infinity;
  reject(input, AuroraCurtainDiagnostic::activity_non_finite);
  input = valid; input.activity = -0.01F;
  reject(input, AuroraCurtainDiagnostic::activity_out_of_range);
  input = valid; input.night_factor = nan;
  reject(input, AuroraCurtainDiagnostic::night_factor_non_finite);
  input = valid; input.night_factor = 1.01F;
  reject(input, AuroraCurtainDiagnostic::night_factor_out_of_range);
  input = valid; input.quality = static_cast<AuroraQuality>(99U);
  reject(input, AuroraCurtainDiagnostic::quality_invalid);
}

void DepositionBandsHavePhysicalAltitudeOrdering(TestContext& context) {
  float green_peak_height{};
  float blue_peak_height{};
  float red_peak_height{};
  float maximum_green{};
  float maximum_blue{};
  float maximum_red{};
  std::uint32_t green_half_width{};
  std::uint32_t red_half_width{};
  std::array<AuroraDepositionProfile, 1001U> profiles{};

  for (std::uint32_t index = 0; index <= 1000U; ++index) {
    const float height = static_cast<float>(index) / 1000.0F;
    profiles[index] = EvaluateAuroraDepositionProfile(height);
    const auto& profile = profiles[index];
    context.expect(std::isfinite(profile.r)
                       && std::isfinite(profile.g)
                       && std::isfinite(profile.b),
                   "aurora deposition profile was non-finite");
    context.expect(profile.r >= 0.0F && profile.r <= 1.0F
                       && profile.g >= 0.0F && profile.g <= 1.0F
                       && profile.b >= 0.0F && profile.b <= 1.0F,
                   "aurora deposition profile left [0,1]");
    if (profile.g > maximum_green) {
      maximum_green = profile.g;
      green_peak_height = height;
    }
    if (profile.b > maximum_blue) {
      maximum_blue = profile.b;
      blue_peak_height = height;
    }
    if (profile.r > maximum_red) {
      maximum_red = profile.r;
      red_peak_height = height;
    }
  }
  for (const auto& profile : profiles) {
    green_half_width += profile.g >= maximum_green * 0.5F ? 1U : 0U;
    red_half_width += profile.r >= maximum_red * 0.5F ? 1U : 0U;
  }

  context.expect(green_peak_height >= 0.22F && green_peak_height <= 0.42F,
                 "green deposition peak left the lower aurora band");
  context.expect(std::fabs(blue_peak_height - green_peak_height) <= 0.08F,
                 "blue deposition peak separated from the green band");
  context.expect(red_peak_height >= green_peak_height + 0.25F,
                 "red deposition peak was not substantially higher");
  context.expect(red_half_width >= green_half_width + 70U,
                 "red deposition band was not longer-lived/broader in altitude");
}

void DayAndInactiveRadianceIsExactZero(TestContext& context) {
  AuroraCurtainInput input = ReferenceInput();
  input.night_factor = 0.0F;
  AuroraCurtainOutput output{};
  ExpectSucceeded(context, EvaluateAuroraCurtain(input, output));
  context.expect(SameFloatBits(output.mask, 0.0F),
                 "day aurora mask was not exact +0");
  context.expect(SameFloatBits(output.intrinsic_radiance.r, 0.0F)
                     && SameFloatBits(output.intrinsic_radiance.g, 0.0F)
                     && SameFloatBits(output.intrinsic_radiance.b, 0.0F),
                 "day aurora radiance was not exact +0");
  context.expect(output.samples == 0U,
                 "day aurora consumed integration samples");

  input = ReferenceInput();
  input.activity = 0.0F;
  AuroraCurtainOutput inactive{};
  AuroraCurtainOutput active{};
  ExpectSucceeded(context, EvaluateAuroraCurtain(ReferenceInput(), active));
  ExpectSucceeded(context, EvaluateAuroraCurtain(input, inactive));
  context.expect(SameFloatBits(inactive.mask, active.mask),
                 "aurora activity changed the spatial curtain mask");
  context.expect(SameFloatBits(inactive.intrinsic_radiance.r, 0.0F)
                     && SameFloatBits(inactive.intrinsic_radiance.g, 0.0F)
                     && SameFloatBits(inactive.intrinsic_radiance.b, 0.0F),
                 "inactive aurora radiance was not exact +0");
  context.expect(inactive.samples == truth::render::AuroraSampleCount(input.quality),
                 "inactive aurora changed the spatial integration budget");
}

void DenseGridIsDeterministicFiniteAndBounded(TestContext& context) {
  constexpr float pi = 3.14159265358979323846F;
  constexpr std::array qualities{
      AuroraQuality::fallback,
      AuroraQuality::low,
      AuroraQuality::balanced,
      AuroraQuality::high,
  };
  for (const auto quality : qualities) {
    for (std::uint32_t azimuth_index = 0; azimuth_index < 17U; ++azimuth_index) {
      const float azimuth = -pi
          + (2.0F * pi * static_cast<float>(azimuth_index) / 17.0F);
      for (std::uint32_t height_index = 0; height_index <= 8U; ++height_index) {
        AuroraCurtainInput input = ReferenceInput();
        SetPanoramaDirection(
            input, azimuth, 0.05F + (0.90F * static_cast<float>(height_index) / 8.0F));
        input.phase = static_cast<float>((azimuth_index + height_index) % 9U) / 8.0F;
        input.activity = static_cast<float>((2U * azimuth_index + height_index) % 9U)
            / 8.0F;
        input.night_factor = static_cast<float>((azimuth_index + 3U * height_index) % 9U)
            / 8.0F;
        input.camera_x = -1.0F + (2.0F * static_cast<float>(azimuth_index) / 16.0F);
        input.camera_y = -0.5F + (static_cast<float>(height_index) / 8.0F);
        input.quality = quality;
        AuroraCurtainOutput first{};
        AuroraCurtainOutput second{};
        ExpectSucceeded(context, EvaluateAuroraCurtain(input, first));
        ExpectSucceeded(context, EvaluateAuroraCurtain(input, second));
        context.expect(SameOutput(first, second),
                       "identical aurora input changed output bits");
        ExpectBounded(context, first);
        const std::uint32_t expected_samples =
            input.night_factor == 0.0F || input.view_z <= 0.0F
            ? 0U
            : truth::render::AuroraSampleCount(quality);
        context.expect(first.samples == expected_samples,
                       "aurora integration exceeded its quality budget");
      }
    }
  }
}

void PhaseLoopIsExactAndTemporallyContinuous(TestContext& context) {
  AuroraCurtainInput start = ReferenceInput();
  start.phase = 0.0F;
  AuroraCurtainInput end = start;
  end.phase = 1.0F;
  AuroraCurtainOutput start_output{};
  AuroraCurtainOutput end_output{};
  ExpectSucceeded(context, EvaluateAuroraCurtain(start, start_output));
  ExpectSucceeded(context, EvaluateAuroraCurtain(end, end_output));
  context.expect(SameOutput(start_output, end_output),
                 "aurora phase 0 and 1 did not share an exact seam");

  AuroraCurtainInput before = start;
  before.phase = 0.9999F;
  AuroraCurtainInput after = start;
  after.phase = 0.0001F;
  AuroraCurtainOutput before_output{};
  AuroraCurtainOutput after_output{};
  ExpectSucceeded(context, EvaluateAuroraCurtain(before, before_output));
  ExpectSucceeded(context, EvaluateAuroraCurtain(after, after_output));
  context.expect(std::fabs(before_output.mask - after_output.mask) < 0.015F,
                 "aurora mask jumped across the temporal seam");
  context.expect(std::fabs(before_output.intrinsic_radiance.g
                           - after_output.intrinsic_radiance.g) < 0.015F,
                 "aurora radiance jumped across the temporal seam");
}

void PanoramaSeamIsContinuous(TestContext& context) {
  constexpr float pi = 3.14159265358979323846F;
  AuroraCurtainInput input = ReferenceInput();
  for (const float view_z : {0.08F, 0.32F, 0.62F, 0.88F}) {
    SetPanoramaDirection(input, -pi, view_z);
    AuroraCurtainOutput first{};
    ExpectSucceeded(context, EvaluateAuroraCurtain(input, first));
    SetPanoramaDirection(input, pi, view_z);
    AuroraCurtainOutput last{};
    ExpectSucceeded(context, EvaluateAuroraCurtain(input, last));
    context.expect(std::fabs(first.mask - last.mask) < 1.0e-5F,
                   "aurora mask opened at panorama seam");
    context.expect(std::fabs(first.intrinsic_radiance.r
                             - last.intrinsic_radiance.r) < 1.0e-5F
                       && std::fabs(first.intrinsic_radiance.g
                                    - last.intrinsic_radiance.g) < 1.0e-5F
                       && std::fabs(first.intrinsic_radiance.b
                                    - last.intrinsic_radiance.b) < 1.0e-5F,
                   "aurora radiance opened at panorama seam");
  }
}

void CameraTranslationProducesParallax(TestContext& context) {
  AuroraCurtainInput baseline = ReferenceInput();
  AuroraCurtainInput translated = baseline;
  translated.camera_x = 0.85F;
  translated.camera_y = -0.40F;
  AuroraCurtainOutput baseline_output{};
  AuroraCurtainOutput translated_output{};
  ExpectSucceeded(context, EvaluateAuroraCurtain(baseline, baseline_output));
  ExpectSucceeded(context, EvaluateAuroraCurtain(translated, translated_output));
  context.expect(!SameOutput(baseline_output, translated_output),
                 "camera translation did not move the aurora curtain");
}

void QualityTiersAreBoundedAndDistinct(TestContext& context) {
  AuroraCurtainInput input = ReferenceInput();
  input.quality = AuroraQuality::fallback;
  AuroraCurtainOutput fallback{};
  ExpectSucceeded(context, EvaluateAuroraCurtain(input, fallback));
  input.quality = AuroraQuality::low;
  AuroraCurtainOutput low{};
  ExpectSucceeded(context, EvaluateAuroraCurtain(input, low));
  input.quality = AuroraQuality::balanced;
  AuroraCurtainOutput balanced{};
  ExpectSucceeded(context, EvaluateAuroraCurtain(input, balanced));
  input.quality = AuroraQuality::high;
  AuroraCurtainOutput high{};
  ExpectSucceeded(context, EvaluateAuroraCurtain(input, high));

  context.expect(fallback.samples == 1U && low.samples == 4U
                     && balanced.samples == 7U && high.samples == 10U,
                 "aurora quality tier sample counts were wrong");
  context.expect(!SameOutput(fallback, balanced),
                 "aurora fallback was indistinguishable from balanced");
  context.expect(!SameOutput(low, high),
                 "aurora low and high tiers were indistinguishable");
  ExpectBounded(context, fallback);
  ExpectBounded(context, low);
  ExpectBounded(context, balanced);
  ExpectBounded(context, high);
}

void CurtainFormsBroadArcsWithoutPillars(TestContext& context) {
  constexpr float pi = 3.14159265358979323846F;
  constexpr std::uint32_t horizontal_samples = 192U;
  constexpr std::uint32_t vertical_samples = 64U;
  AuroraCurtainInput input = ReferenceInput();
  std::array<bool, horizontal_samples> supported_columns{};
  float maximum_column_support{};
  std::uint32_t active_pixels{};

  for (std::uint32_t x = 0; x < horizontal_samples; ++x) {
    const float azimuth = -pi
        + (2.0F * pi * static_cast<float>(x)
           / static_cast<float>(horizontal_samples));
    std::uint32_t column_support{};
    for (std::uint32_t y = 0; y < vertical_samples; ++y) {
      const float elevation = 0.035F
          + (((0.5F * pi) - 0.07F) * static_cast<float>(y)
             / static_cast<float>(vertical_samples - 1U));
      SetPanoramaDirection(input, azimuth, std::sin(elevation));
      AuroraCurtainOutput output{};
      ExpectSucceeded(context, EvaluateAuroraCurtain(input, output));
      if (output.mask > 0.018F) {
        ++column_support;
        ++active_pixels;
        supported_columns[x] = true;
      }
    }
    maximum_column_support = std::max(
        maximum_column_support,
        static_cast<float>(column_support) / static_cast<float>(vertical_samples));
  }

  std::uint32_t supported_count{};
  std::uint32_t longest_run{};
  std::uint32_t current_run{};
  for (const bool supported : supported_columns) {
    if (supported) {
      ++supported_count;
      ++current_run;
      longest_run = std::max(longest_run, current_run);
    } else {
      current_run = 0U;
    }
  }
  const float active_fraction = static_cast<float>(active_pixels)
      / static_cast<float>(horizontal_samples * vertical_samples);
  std::cout << "aurora structure columns=" << supported_count
            << " longest_run=" << longest_run
            << " max_column_support=" << maximum_column_support
            << " active_fraction=" << active_fraction << '\n';
  context.expect(longest_run >= 58U,
                 "aurora broke into disconnected narrow columns");
  context.expect(supported_count < 166U,
                 "aurora filled almost the entire panorama");
  context.expect(maximum_column_support < 0.72F,
                 "aurora collapsed into a full-height pillar");
  context.expect(active_fraction >= 0.025F && active_fraction <= 0.32F,
                 "aurora support left the restrained sky-area budget");
}

void EmissionBudgetIsRestrainedAndGreenLed(TestContext& context) {
  constexpr float pi = 3.14159265358979323846F;
  AuroraCurtainInput input = ReferenceInput();
  float maximum_channel{};
  double red_sum{};
  double green_sum{};
  double blue_sum{};
  for (std::uint32_t x = 0; x < 128U; ++x) {
    const float azimuth = -pi + (2.0F * pi * static_cast<float>(x) / 128.0F);
    for (std::uint32_t y = 0; y < 48U; ++y) {
      SetPanoramaDirection(input, azimuth, 0.06F + (0.90F * static_cast<float>(y) / 47.0F));
      AuroraCurtainOutput output{};
      ExpectSucceeded(context, EvaluateAuroraCurtain(input, output));
      maximum_channel = std::max({
          maximum_channel,
          output.intrinsic_radiance.r,
          output.intrinsic_radiance.g,
          output.intrinsic_radiance.b,
      });
      red_sum += output.intrinsic_radiance.r;
      green_sum += output.intrinsic_radiance.g;
      blue_sum += output.intrinsic_radiance.b;
    }
  }
  std::cout << "aurora emission max=" << maximum_channel
            << " rgb_sum=" << red_sum << ',' << green_sum << ',' << blue_sum << '\n';
  context.expect(maximum_channel >= 0.055F,
                 "active aurora never reached a bloom-ready highlight");
  context.expect(maximum_channel <= 0.42F,
                 "active aurora exceeded the non-neon radiance ceiling");
  context.expect(green_sum > red_sum * 1.45 && green_sum > blue_sum * 1.25,
                 "aurora emission was not led by a restrained green band");
}

using TestFunction = void (*)(TestContext&);
struct TestCase { std::string_view name; TestFunction function; };

constexpr TestCase kTests[] = {
    {"stable codes and budgets are explicit", &StableCodesAndBudgetsAreExplicit},
    {"invalid inputs preserve output", &InvalidInputsPreserveOutput},
    {"deposition bands have physical altitude ordering",
     &DepositionBandsHavePhysicalAltitudeOrdering},
    {"day and inactive radiance is exact zero", &DayAndInactiveRadianceIsExactZero},
    {"dense grid is deterministic finite and bounded",
     &DenseGridIsDeterministicFiniteAndBounded},
    {"phase loop is exact and temporally continuous",
     &PhaseLoopIsExactAndTemporallyContinuous},
    {"panorama seam is continuous", &PanoramaSeamIsContinuous},
    {"camera translation produces parallax", &CameraTranslationProducesParallax},
    {"quality tiers are bounded and distinct", &QualityTiersAreBoundedAndDistinct},
    {"curtain forms broad arcs without pillars", &CurtainFormsBroadArcsWithoutPillars},
    {"emission budget is restrained and green led", &EmissionBudgetIsRestrainedAndGreenLed},
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
  std::cout << "Truth aurora-curtain C++ cases: " << passed << '/' << std::size(kTests)
            << "; assertions: " << context.assertions << '\n';
  return 0;
}
