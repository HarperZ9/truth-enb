#include "truth/render/SkyViewAdapter.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <string_view>

namespace {

using truth::render::EvaluateSkyViewAdapter;
using truth::render::SkyViewAdapterDiagnostic;
using truth::render::SkyViewAdapterInput;
using truth::render::SkyViewAdapterOutput;
using truth::render::SkyViewAdapterStatus;
using truth::render::SkyViewMatrix;

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
    const SkyViewAdapterOutput& lhs,
    const SkyViewAdapterOutput& rhs) noexcept {
  return SameFloatBits(lhs.view_world_x, rhs.view_world_x)
      && SameFloatBits(lhs.view_world_y, rhs.view_world_y)
      && SameFloatBits(lhs.view_world_z, rhs.view_world_z)
      && SameFloatBits(lhs.camera_aurora_x, rhs.camera_aurora_x)
      && SameFloatBits(lhs.camera_aurora_y, rhs.camera_aurora_y)
      && SameFloatBits(lhs.camera_aurora_z, rhs.camera_aurora_z);
}

[[nodiscard]] bool Close(
    const float lhs,
    const float rhs,
    const float tolerance = 1.0e-5F) noexcept {
  return std::fabs(lhs - rhs) <= tolerance;
}

[[nodiscard]] SkyViewMatrix InverseViewProjectionFromBasis(
    const std::array<float, 3>& right,
    const std::array<float, 3>& up,
    const std::array<float, 3>& forward,
    const std::array<float, 3>& camera) noexcept {
  return {
      right[0], up[0], forward[0], camera[0],
      right[1], up[1], forward[1], camera[1],
      right[2], up[2], forward[2], camera[2],
      0.0F, 0.0F, 0.0F, 1.0F,
  };
}

[[nodiscard]] SkyViewAdapterInput ReferenceInput() noexcept {
  constexpr std::array right{1.0F, 0.0F, 0.0F};
  constexpr std::array up{0.0F, 0.0F, 1.0F};
  constexpr std::array forward{0.0F, 1.0F, 0.0F};
  constexpr std::array camera{8192.0F, -4096.0F, 40960.0F};
  return {
      0.5F,
      0.5F,
      InverseViewProjectionFromBasis(right, up, forward, camera),
      camera[0], camera[1], camera[2],
      4096.0F, -8192.0F, 32768.0F,
      4096.0F,
  };
}

void ExpectSucceeded(
    TestContext& context,
    const truth::render::SkyViewAdapterEvaluation evaluation) {
  context.expect(evaluation.status == SkyViewAdapterStatus::evaluated,
                 "sky-view adapter rejected valid input");
  context.expect(evaluation.diagnostic == SkyViewAdapterDiagnostic::none,
                 "sky-view adapter returned a diagnostic on success");
}

void RawEngineCoordinatesAreExplicitlyRebasedAndScaled(TestContext& context) {
  SkyViewAdapterOutput output{};
  ExpectSucceeded(context, EvaluateSkyViewAdapter(ReferenceInput(), output));
  context.expect(Close(output.view_world_x, 0.0F)
                     && Close(output.view_world_y, 1.0F)
                     && Close(output.view_world_z, 0.0F),
                 "center ray was not reconstructed in world space");
  context.expect(Close(output.camera_aurora_x, 1.0F)
                     && Close(output.camera_aurora_y, 1.0F)
                     && Close(output.camera_aurora_z, 2.0F),
                 "raw engine coordinates bypassed the declared aurora transform");
  context.expect(output.camera_aurora_z < truth::render::kAuroraBaseHeight,
                 "raw Skyrim altitude incorrectly placed the camera above the aurora slab");
}

void CameraYawAndPitchRotateTheWorldRay(TestContext& context) {
  SkyViewAdapterInput yaw = ReferenceInput();
  yaw.inverse_view_projection = InverseViewProjectionFromBasis(
      {0.0F, -1.0F, 0.0F},
      {0.0F, 0.0F, 1.0F},
      {1.0F, 0.0F, 0.0F},
      {yaw.camera_world_x, yaw.camera_world_y, yaw.camera_world_z});
  SkyViewAdapterOutput yaw_output{};
  ExpectSucceeded(context, EvaluateSkyViewAdapter(yaw, yaw_output));
  context.expect(Close(yaw_output.view_world_x, 1.0F)
                     && Close(yaw_output.view_world_y, 0.0F)
                     && Close(yaw_output.view_world_z, 0.0F),
                 "camera yaw did not rotate the reconstructed world ray");

  constexpr float inverse_sqrt_two = 0.7071067811865475F;
  SkyViewAdapterInput pitch = ReferenceInput();
  pitch.inverse_view_projection = InverseViewProjectionFromBasis(
      {1.0F, 0.0F, 0.0F},
      {0.0F, -inverse_sqrt_two, inverse_sqrt_two},
      {0.0F, inverse_sqrt_two, inverse_sqrt_two},
      {pitch.camera_world_x, pitch.camera_world_y, pitch.camera_world_z});
  SkyViewAdapterOutput pitch_output{};
  ExpectSucceeded(context, EvaluateSkyViewAdapter(pitch, pitch_output));
  context.expect(Close(pitch_output.view_world_x, 0.0F)
                     && Close(pitch_output.view_world_y, inverse_sqrt_two)
                     && Close(pitch_output.view_world_z, inverse_sqrt_two),
                 "camera pitch did not rotate the reconstructed world ray");
}

void TranslationPreservesOrientationAndMovesAuroraCamera(TestContext& context) {
  SkyViewAdapterInput baseline = ReferenceInput();
  SkyViewAdapterOutput baseline_output{};
  ExpectSucceeded(context, EvaluateSkyViewAdapter(baseline, baseline_output));

  SkyViewAdapterInput translated = baseline;
  translated.camera_world_x += 4096.0F;
  translated.camera_world_y -= 2048.0F;
  translated.camera_world_z += 1024.0F;
  translated.inverse_view_projection = InverseViewProjectionFromBasis(
      {1.0F, 0.0F, 0.0F},
      {0.0F, 0.0F, 1.0F},
      {0.0F, 1.0F, 0.0F},
      {translated.camera_world_x,
       translated.camera_world_y,
       translated.camera_world_z});
  SkyViewAdapterOutput translated_output{};
  ExpectSucceeded(context, EvaluateSkyViewAdapter(translated, translated_output));
  context.expect(Close(translated_output.view_world_x, baseline_output.view_world_x)
                     && Close(translated_output.view_world_y, baseline_output.view_world_y)
                     && Close(translated_output.view_world_z, baseline_output.view_world_z),
                 "camera translation changed world-ray orientation");
  context.expect(Close(translated_output.camera_aurora_x,
                       baseline_output.camera_aurora_x + 1.0F)
                     && Close(translated_output.camera_aurora_y,
                              baseline_output.camera_aurora_y - 0.5F)
                     && Close(translated_output.camera_aurora_z,
                              baseline_output.camera_aurora_z + 0.25F),
                 "camera translation did not use the declared engine-to-aurora scale");
}

void InvalidContractsPreserveOutput(TestContext& context) {
  const SkyViewAdapterOutput sentinel{-2.0F, 3.0F, -4.0F, 5.0F, -6.0F, 7.0F};
  const auto reject = [&](const SkyViewAdapterInput input,
                          const SkyViewAdapterDiagnostic diagnostic) {
    SkyViewAdapterOutput output = sentinel;
    const auto evaluation = EvaluateSkyViewAdapter(input, output);
    context.expect(evaluation.status == SkyViewAdapterStatus::rejected,
                   "invalid sky-view adapter contract was accepted");
    context.expect(evaluation.diagnostic == diagnostic,
                   "sky-view adapter rejection diagnostic was wrong");
    context.expect(SameOutput(output, sentinel),
                   "rejected sky-view adapter input mutated output");
  };

  SkyViewAdapterInput input = ReferenceInput();
  input.texcoord_x = -0.01F;
  reject(input, SkyViewAdapterDiagnostic::texcoord_x_out_of_range);
  input = ReferenceInput();
  input.engine_world_units_per_aurora_unit = 0.0F;
  reject(input, SkyViewAdapterDiagnostic::world_scale_out_of_range);
  input = ReferenceInput();
  input.inverse_view_projection.m00 = std::numeric_limits<float>::quiet_NaN();
  reject(input, SkyViewAdapterDiagnostic::inverse_view_projection_non_finite);
  input = ReferenceInput();
  input.inverse_view_projection.m30 = 0.0F;
  input.inverse_view_projection.m31 = 0.0F;
  input.inverse_view_projection.m32 = 0.0F;
  input.inverse_view_projection.m33 = 0.0F;
  reject(input, SkyViewAdapterDiagnostic::unproject_w_invalid);
}

using TestFunction = void (*)(TestContext&);
struct TestCase { std::string_view name; TestFunction function; };

constexpr TestCase kTests[] = {
    {"raw engine coordinates are explicitly rebased and scaled",
     &RawEngineCoordinatesAreExplicitlyRebasedAndScaled},
    {"camera yaw and pitch rotate the world ray", &CameraYawAndPitchRotateTheWorldRay},
    {"translation preserves orientation and moves aurora camera",
     &TranslationPreservesOrientationAndMovesAuroraCamera},
    {"invalid contracts preserve output", &InvalidContractsPreserveOutput},
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
  std::cout << "Truth sky-view adapter C++ cases: " << passed << '/'
            << std::size(kTests) << "; assertions: " << context.assertions << '\n';
  return 0;
}
