#include "truth/render/Atmosphere.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <iterator>
#include <limits>
#include <string_view>

namespace {

using truth::render::AtmosphereDiagnostic;
using truth::render::AtmosphereEvaluation;
using truth::render::AtmosphereInput;
using truth::render::AtmosphereOutput;
using truth::render::AtmosphereStatus;
using truth::render::EvaluateAtmosphere;
using truth::render::MiePhase;
using truth::render::RayleighPhase;
using truth::render::RgbRadiance;

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

[[nodiscard]] bool Near(const float lhs, const float rhs, const float tolerance = 1.0e-5F) noexcept {
  return std::fabs(lhs - rhs) <= tolerance;
}

[[nodiscard]] bool SameFloatBits(const float lhs, const float rhs) noexcept {
  return std::bit_cast<std::uint32_t>(lhs) == std::bit_cast<std::uint32_t>(rhs);
}

[[nodiscard]] bool SameRadiance(const RgbRadiance& lhs, const RgbRadiance& rhs) noexcept {
  return SameFloatBits(lhs.r, rhs.r) && SameFloatBits(lhs.g, rhs.g) && SameFloatBits(lhs.b, rhs.b);
}

[[nodiscard]] bool SameOutput(const AtmosphereOutput& lhs, const AtmosphereOutput& rhs) noexcept {
  return SameRadiance(lhs.sky_radiance, rhs.sky_radiance)
      && SameFloatBits(lhs.cloud_transmittance, rhs.cloud_transmittance)
      && SameFloatBits(lhs.fog_transmittance, rhs.fog_transmittance)
      && SameRadiance(lhs.aurora_radiance, rhs.aurora_radiance)
      && SameRadiance(lhs.composite_radiance, rhs.composite_radiance);
}

[[nodiscard]] AtmosphereInput ClearNoon() noexcept {
  return {1.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 1.0F, 0.0F};
}

void ExpectFiniteNonnegative(TestContext& context, const RgbRadiance& value) {
  context.expect(std::isfinite(value.r), "radiance r was non-finite");
  context.expect(std::isfinite(value.g), "radiance g was non-finite");
  context.expect(std::isfinite(value.b), "radiance b was non-finite");
  context.expect(value.r >= 0.0F, "radiance r was negative");
  context.expect(value.g >= 0.0F, "radiance g was negative");
  context.expect(value.b >= 0.0F, "radiance b was negative");
}

void ExpectEvaluationSucceeded(TestContext& context, const AtmosphereEvaluation evaluation) {
  context.expect(evaluation.status == AtmosphereStatus::evaluated, "atmosphere evaluation was rejected");
  context.expect(evaluation.diagnostic == AtmosphereDiagnostic::none, "successful evaluation returned diagnostic");
}

void StableCodesAreExplicit(TestContext& context) {
  context.expect(static_cast<std::uint32_t>(AtmosphereStatus::evaluated) == 0U, "evaluated code changed");
  context.expect(static_cast<std::uint32_t>(AtmosphereStatus::rejected) == 1U, "rejected code changed");
  context.expect(static_cast<std::uint32_t>(AtmosphereDiagnostic::none) == 0U, "none diagnostic changed");
  context.expect(static_cast<std::uint32_t>(AtmosphereDiagnostic::view_zenith_cosine_non_finite) == 100U,
                 "view zenith diagnostic changed");
  context.expect(static_cast<std::uint32_t>(AtmosphereDiagnostic::night_factor_out_of_range) == 191U,
                 "night factor diagnostic changed");
  context.expect(static_cast<std::uint32_t>(AtmosphereDiagnostic::calculation_out_of_range) == 201U,
                 "calculation diagnostic changed");
}

void DenseGridStaysFiniteAndBounded(TestContext& context) {
  for (std::uint32_t zenith_index = 0; zenith_index <= 40U; ++zenith_index) {
    for (std::uint32_t sun_index = 0; sun_index <= 40U; ++sun_index) {
      for (std::uint32_t elevation_index = 0; elevation_index <= 8U; ++elevation_index) {
        AtmosphereInput input{};
        input.view_zenith_cosine = -1.0F + (0.05F * static_cast<float>(zenith_index));
        input.view_sun_cosine = -1.0F + (0.05F * static_cast<float>(sun_index));
        input.sun_elevation = -1.0F + (0.25F * static_cast<float>(elevation_index));
        input.weather_density = static_cast<float>(zenith_index) / 40.0F;
        input.cloud_coverage = static_cast<float>(sun_index) / 40.0F;
        input.cloud_density = static_cast<float>(elevation_index) / 8.0F;
        input.fog_density = static_cast<float>((zenith_index + sun_index + elevation_index) % 9U) / 8.0F;
        input.aurora_activity = static_cast<float>(sun_index % 9U) / 8.0F;
        input.aurora_mask = static_cast<float>(zenith_index % 9U) / 8.0F;
        input.night_factor = static_cast<float>(8U - elevation_index) / 8.0F;

        AtmosphereOutput output{};
        ExpectEvaluationSucceeded(context, EvaluateAtmosphere(input, output));
        ExpectFiniteNonnegative(context, output.sky_radiance);
        ExpectFiniteNonnegative(context, output.aurora_radiance);
        ExpectFiniteNonnegative(context, output.composite_radiance);
        context.expect(std::isfinite(output.cloud_transmittance), "cloud transmittance was non-finite");
        context.expect(output.cloud_transmittance >= 0.0F && output.cloud_transmittance <= 1.0F,
                       "cloud transmittance left [0,1]");
        context.expect(std::isfinite(output.fog_transmittance), "fog transmittance was non-finite");
        context.expect(output.fog_transmittance >= 0.0F && output.fog_transmittance <= 1.0F,
                       "fog transmittance left [0,1]");
        const float attenuation = output.cloud_transmittance * output.fog_transmittance;
        context.expect(Near(output.composite_radiance.r,
                            output.sky_radiance.r * attenuation + output.aurora_radiance.r, 2.0e-5F),
                       "dense-grid composite r broke coupling equality");
        context.expect(Near(output.composite_radiance.g,
                            output.sky_radiance.g * attenuation + output.aurora_radiance.g, 2.0e-5F),
                       "dense-grid composite g broke coupling equality");
        context.expect(Near(output.composite_radiance.b,
                            output.sky_radiance.b * attenuation + output.aurora_radiance.b, 2.0e-5F),
                       "dense-grid composite b broke coupling equality");
      }
    }
  }
}

void HorizonFallbackIsConservativeAndContinuous(TestContext& context) {
  AtmosphereInput below = ClearNoon();
  below.view_zenith_cosine = -1.0F;
  AtmosphereInput horizon = below;
  horizon.view_zenith_cosine = 0.0F;
  AtmosphereInput floor = below;
  floor.view_zenith_cosine = 0.05F;
  AtmosphereInput above = below;
  above.view_zenith_cosine = 0.0501F;

  AtmosphereOutput below_output{};
  AtmosphereOutput horizon_output{};
  AtmosphereOutput floor_output{};
  AtmosphereOutput above_output{};
  ExpectEvaluationSucceeded(context, EvaluateAtmosphere(below, below_output));
  ExpectEvaluationSucceeded(context, EvaluateAtmosphere(horizon, horizon_output));
  ExpectEvaluationSucceeded(context, EvaluateAtmosphere(floor, floor_output));
  ExpectEvaluationSucceeded(context, EvaluateAtmosphere(above, above_output));

  context.expect(SameRadiance(below_output.sky_radiance, horizon_output.sky_radiance),
                 "below-horizon sky did not use horizon fallback");
  context.expect(SameRadiance(horizon_output.sky_radiance, floor_output.sky_radiance),
                 "horizon sky did not use cosine floor");
  context.expect(SameFloatBits(below_output.fog_transmittance, floor_output.fog_transmittance),
                 "below-horizon fog path differed from horizon fallback");
  context.expect(std::fabs(above_output.sky_radiance.b - floor_output.sky_radiance.b) < 0.001F,
                 "sky was discontinuous above the horizon clamp");
  context.expect(std::fabs(above_output.fog_transmittance - floor_output.fog_transmittance) < 0.001F,
                 "fog was discontinuous above the horizon clamp");
}

void PhaseFunctionsHaveExpectedSymmetryAndAsymmetry(TestContext& context) {
  context.expect(Near(RayleighPhase(0.8F), RayleighPhase(-0.8F)), "Rayleigh phase lost symmetry");
  context.expect(MiePhase(0.8F) > MiePhase(-0.8F), "Mie phase lost forward asymmetry");
  context.expect(std::isfinite(RayleighPhase(1.0F)), "sun-aligned Rayleigh was non-finite");
  context.expect(std::isfinite(MiePhase(1.0F)), "sun-aligned Mie was non-finite");
  context.expect(std::isfinite(RayleighPhase(-1.0F)), "anti-solar Rayleigh was non-finite");
  context.expect(std::isfinite(MiePhase(-1.0F)), "anti-solar Mie was non-finite");
  context.expect(RayleighPhase(1.0F) <= 1.5F, "Rayleigh exceeded its bound");
  context.expect(MiePhase(1.0F) <= 12.0F, "Mie exceeded its bound");
}

void CloudOpacityIsMonotonic(TestContext& context) {
  AtmosphereInput input = ClearNoon();
  input.weather_density = 0.6F;
  input.cloud_density = 0.8F;
  float previous = 1.0F;
  for (std::uint32_t index = 0; index <= 20U; ++index) {
    input.cloud_coverage = static_cast<float>(index) / 20.0F;
    AtmosphereOutput output{};
    ExpectEvaluationSucceeded(context, EvaluateAtmosphere(input, output));
    context.expect(output.cloud_transmittance <= previous + 1.0e-6F,
                   "cloud transmittance increased with coverage");
    previous = output.cloud_transmittance;
  }

  input.cloud_coverage = 0.8F;
  previous = 1.0F;
  for (std::uint32_t index = 0; index <= 20U; ++index) {
    input.cloud_density = static_cast<float>(index) / 20.0F;
    AtmosphereOutput output{};
    ExpectEvaluationSucceeded(context, EvaluateAtmosphere(input, output));
    context.expect(output.cloud_transmittance <= previous + 1.0e-6F,
                   "cloud transmittance increased with density");
    previous = output.cloud_transmittance;
  }
}

void FogTransmittanceIsMonotonic(TestContext& context) {
  AtmosphereInput input = ClearNoon();
  input.view_zenith_cosine = 0.25F;
  float previous = 1.0F;
  for (std::uint32_t index = 0; index <= 20U; ++index) {
    input.fog_density = static_cast<float>(index) / 20.0F;
    AtmosphereOutput output{};
    ExpectEvaluationSucceeded(context, EvaluateAtmosphere(input, output));
    context.expect(output.fog_transmittance <= previous + 1.0e-6F,
                   "fog transmittance increased with density");
    previous = output.fog_transmittance;
  }
}

void DayAuroraIsExactlyZero(TestContext& context) {
  AtmosphereInput input = ClearNoon();
  input.aurora_activity = 1.0F;
  input.aurora_mask = 1.0F;
  input.night_factor = 0.0F;
  AtmosphereOutput output{};
  ExpectEvaluationSucceeded(context, EvaluateAtmosphere(input, output));
  context.expect(SameFloatBits(output.aurora_radiance.r, 0.0F), "day aurora r was not exact zero");
  context.expect(SameFloatBits(output.aurora_radiance.g, 0.0F), "day aurora g was not exact zero");
  context.expect(SameFloatBits(output.aurora_radiance.b, 0.0F), "day aurora b was not exact zero");
}

void NightAuroraIncreasesWithActivity(TestContext& context) {
  AtmosphereInput input = ClearNoon();
  input.sun_elevation = -1.0F;
  input.night_factor = 1.0F;
  input.aurora_mask = 1.0F;
  float previous = 0.0F;
  for (std::uint32_t index = 0; index <= 20U; ++index) {
    input.aurora_activity = static_cast<float>(index) / 20.0F;
    AtmosphereOutput output{};
    ExpectEvaluationSucceeded(context, EvaluateAtmosphere(input, output));
    context.expect(output.aurora_radiance.g + 1.0e-7F >= previous,
                   "night aurora decreased with activity");
    previous = output.aurora_radiance.g;
  }
  context.expect(previous > 0.0F, "active night aurora stayed zero");
}

void AuroraIsAttenuatedByCloudAndFog(TestContext& context) {
  AtmosphereInput clear = ClearNoon();
  clear.sun_elevation = -1.0F;
  clear.night_factor = 1.0F;
  AtmosphereInput cloudy = clear;
  cloudy.weather_density = 1.0F;
  cloudy.cloud_coverage = 1.0F;
  cloudy.cloud_density = 1.0F;
  AtmosphereInput foggy = clear;
  foggy.fog_density = 1.0F;
  AtmosphereInput both = cloudy;
  both.fog_density = 1.0F;

  AtmosphereOutput clear_output{};
  AtmosphereOutput cloudy_output{};
  AtmosphereOutput foggy_output{};
  AtmosphereOutput both_output{};
  ExpectEvaluationSucceeded(context, EvaluateAtmosphere(clear, clear_output));
  ExpectEvaluationSucceeded(context, EvaluateAtmosphere(cloudy, cloudy_output));
  ExpectEvaluationSucceeded(context, EvaluateAtmosphere(foggy, foggy_output));
  ExpectEvaluationSucceeded(context, EvaluateAtmosphere(both, both_output));
  context.expect(cloudy_output.aurora_radiance.g < clear_output.aurora_radiance.g,
                 "clouds did not attenuate aurora");
  context.expect(foggy_output.aurora_radiance.g < clear_output.aurora_radiance.g,
                 "fog did not attenuate aurora");
  context.expect(both_output.aurora_radiance.g < cloudy_output.aurora_radiance.g,
                 "fog was not coupled after cloud attenuation");
  context.expect(both_output.aurora_radiance.g < foggy_output.aurora_radiance.g,
                 "cloud was not coupled after fog attenuation");
}

void CompositeUsesSingleAttenuation(TestContext& context) {
  AtmosphereInput input{0.2F, 0.7F, -0.3F, 0.6F, 0.7F, 0.8F, 0.4F, 0.9F, 0.75F, 1.0F};
  AtmosphereOutput output{};
  ExpectEvaluationSucceeded(context, EvaluateAtmosphere(input, output));
  const float attenuation = output.cloud_transmittance * output.fog_transmittance;
  context.expect(Near(output.composite_radiance.r,
                      output.sky_radiance.r * attenuation + output.aurora_radiance.r),
                 "composite r double-attenuated or skipped aurora");
  context.expect(Near(output.composite_radiance.g,
                      output.sky_radiance.g * attenuation + output.aurora_radiance.g),
                 "composite g double-attenuated or skipped aurora");
  context.expect(Near(output.composite_radiance.b,
                      output.sky_radiance.b * attenuation + output.aurora_radiance.b),
                 "composite b double-attenuated or skipped aurora");
}

void InvalidInputsNeverMutateOutput(TestContext& context) {
  const AtmosphereOutput sentinel{{-1.0F, 2.0F, -3.0F}, -4.0F, 5.0F,
                                  {6.0F, -7.0F, 8.0F}, {-9.0F, 10.0F, -11.0F}};
  const AtmosphereInput valid = ClearNoon();
  const auto reject = [&](const AtmosphereInput input, const AtmosphereDiagnostic expected) {
    AtmosphereOutput output = sentinel;
    const auto evaluation = EvaluateAtmosphere(input, output);
    context.expect(evaluation.status == AtmosphereStatus::rejected, "invalid atmosphere input was accepted");
    context.expect(evaluation.diagnostic == expected, "invalid atmosphere diagnostic was wrong");
    context.expect(SameOutput(output, sentinel), "invalid atmosphere input mutated output");
  };

  const float nan = std::numeric_limits<float>::quiet_NaN();
  const float infinity = std::numeric_limits<float>::infinity();
  AtmosphereInput input = valid;
  input.view_zenith_cosine = nan;
  reject(input, AtmosphereDiagnostic::view_zenith_cosine_non_finite);
  input = valid; input.view_zenith_cosine = -1.01F;
  reject(input, AtmosphereDiagnostic::view_zenith_cosine_out_of_range);
  input = valid; input.view_sun_cosine = infinity;
  reject(input, AtmosphereDiagnostic::view_sun_cosine_non_finite);
  input = valid; input.view_sun_cosine = 1.01F;
  reject(input, AtmosphereDiagnostic::view_sun_cosine_out_of_range);
  input = valid; input.sun_elevation = nan;
  reject(input, AtmosphereDiagnostic::sun_elevation_non_finite);
  input = valid; input.sun_elevation = -1.01F;
  reject(input, AtmosphereDiagnostic::sun_elevation_out_of_range);
  input = valid; input.weather_density = infinity;
  reject(input, AtmosphereDiagnostic::weather_density_non_finite);
  input = valid; input.weather_density = 1.01F;
  reject(input, AtmosphereDiagnostic::weather_density_out_of_range);
  input = valid; input.cloud_coverage = nan;
  reject(input, AtmosphereDiagnostic::cloud_coverage_non_finite);
  input = valid; input.cloud_coverage = -0.01F;
  reject(input, AtmosphereDiagnostic::cloud_coverage_out_of_range);
  input = valid; input.cloud_density = infinity;
  reject(input, AtmosphereDiagnostic::cloud_density_non_finite);
  input = valid; input.cloud_density = 1.01F;
  reject(input, AtmosphereDiagnostic::cloud_density_out_of_range);
  input = valid; input.fog_density = nan;
  reject(input, AtmosphereDiagnostic::fog_density_non_finite);
  input = valid; input.fog_density = -0.01F;
  reject(input, AtmosphereDiagnostic::fog_density_out_of_range);
  input = valid; input.aurora_activity = infinity;
  reject(input, AtmosphereDiagnostic::aurora_activity_non_finite);
  input = valid; input.aurora_activity = 1.01F;
  reject(input, AtmosphereDiagnostic::aurora_activity_out_of_range);
  input = valid; input.aurora_mask = nan;
  reject(input, AtmosphereDiagnostic::aurora_mask_non_finite);
  input = valid; input.aurora_mask = -0.01F;
  reject(input, AtmosphereDiagnostic::aurora_mask_out_of_range);
  input = valid; input.night_factor = infinity;
  reject(input, AtmosphereDiagnostic::night_factor_non_finite);
  input = valid; input.night_factor = 1.01F;
  reject(input, AtmosphereDiagnostic::night_factor_out_of_range);
}

void CpuGoldenSamplesStayStable(TestContext& context) {
  AtmosphereOutput noon{};
  ExpectEvaluationSucceeded(context, EvaluateAtmosphere(ClearNoon(), noon));
  context.expect(Near(noon.sky_radiance.r, 0.14691356F), "clear-noon sky r golden changed");
  context.expect(Near(noon.sky_radiance.g, 0.21850969F), "clear-noon sky g golden changed");
  context.expect(Near(noon.sky_radiance.b, 0.39510581F), "clear-noon sky b golden changed");
  context.expect(Near(noon.cloud_transmittance, 1.0F), "clear-noon cloud golden changed");
  context.expect(Near(noon.fog_transmittance, 1.0F), "clear-noon fog golden changed");

  const AtmosphereInput night{0.0F, 1.0F, -1.0F, 1.0F, 0.5F, 0.5F, 0.2F, 0.5F, 0.8F, 1.0F};
  AtmosphereOutput night_output{};
  ExpectEvaluationSucceeded(context, EvaluateAtmosphere(night, night_output));
  context.expect(Near(night_output.cloud_transmittance, 0.36787944F), "night cloud golden changed");
  context.expect(Near(night_output.fog_transmittance, 0.09926125F), "night fog golden changed");
  context.expect(Near(night_output.sky_radiance.r, 0.00101250F), "night sky r golden changed");
  context.expect(Near(night_output.aurora_radiance.g, 0.00408981F), "night aurora g golden changed");
  context.expect(Near(night_output.composite_radiance.b, 0.00303358F), "night composite b golden changed");
}

using TestFunction = void (*)(TestContext&);
struct TestCase { std::string_view name; TestFunction function; };

constexpr TestCase kTests[] = {
    {"stable codes are explicit", &StableCodesAreExplicit},
    {"dense grid stays finite and bounded", &DenseGridStaysFiniteAndBounded},
    {"horizon fallback is conservative and continuous", &HorizonFallbackIsConservativeAndContinuous},
    {"phase functions have expected symmetry and asymmetry", &PhaseFunctionsHaveExpectedSymmetryAndAsymmetry},
    {"cloud opacity is monotonic", &CloudOpacityIsMonotonic},
    {"fog transmittance is monotonic", &FogTransmittanceIsMonotonic},
    {"day aurora is exactly zero", &DayAuroraIsExactlyZero},
    {"night aurora increases with activity", &NightAuroraIncreasesWithActivity},
    {"aurora is attenuated by cloud and fog", &AuroraIsAttenuatedByCloudAndFog},
    {"composite uses single attenuation", &CompositeUsesSingleAttenuation},
    {"invalid inputs never mutate output", &InvalidInputsNeverMutateOutput},
    {"CPU golden samples stay stable", &CpuGoldenSamplesStayStable},
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
  std::cout << "Truth atmosphere C++ cases: " << passed << '/' << std::size(kTests)
            << "; assertions: " << context.assertions << '\n';
  return 0;
}
