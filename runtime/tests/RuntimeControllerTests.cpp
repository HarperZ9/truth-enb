#include "TestHarness.hpp"

#include <truth/runtime/RuntimeController.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using namespace truth::runtime;
using truth::runtime::tests::Context;

struct ParameterCall final {
    bool write;
    std::string category;
    std::string key;
    Float4 value;
};

class RecordingParameters final : public ShaderParameterApi {
public:
    std::array<Float4, 6> values{
        Float4{1.0F, 2.0F, 3.0F, 4.0F},
        Float4{5.0F, 6.0F, 7.0F, 8.0F},
        Float4{9.0F, 10.0F, 11.0F, 12.0F},
        Float4{13.0F, 14.0F, 15.0F, 16.0F},
        Float4{17.0F, 18.0F, 19.0F, 20.0F},
        Float4{21.0F, 22.0F, 23.0F, 24.0F},
    };
    std::vector<ParameterCall> calls;
    std::size_t fail_get_at{static_cast<std::size_t>(-1)};
    std::size_t fail_set_at{static_cast<std::size_t>(-1)};
    std::vector<std::size_t> fail_set_calls;
    std::size_t get_count{0};
    std::size_t set_count{0};

    [[nodiscard]] bool GetColor4(
        const std::string_view category,
        const std::string_view key,
        Float4& value) noexcept override
    {
        const std::size_t index = IndexOf(key);
        calls.push_back({false, std::string{category}, std::string{key}, {}});
        const bool accepted = get_count++ != fail_get_at
            && index < values.size();
        if (accepted) {
            value = values[index];
        }
        return accepted;
    }

    [[nodiscard]] bool SetColor4(
        const std::string_view category,
        const std::string_view key,
        const Float4& value) noexcept override
    {
        const std::size_t index = IndexOf(key);
        calls.push_back({true, std::string{category}, std::string{key}, value});
        const std::size_t call_index = set_count++;
        const bool accepted = call_index != fail_set_at
            && !std::ranges::contains(fail_set_calls, call_index)
            && index < values.size();
        if (accepted) {
            values[index] = value;
        }
        return accepted;
    }

    [[nodiscard]] static std::size_t IndexOf(const std::string_view key) noexcept
    {
        const auto found = std::ranges::find(kShaderParameterKeys, key);
        return found == kShaderParameterKeys.end()
            ? kShaderParameterKeys.size()
            : static_cast<std::size_t>(found - kShaderParameterKeys.begin());
    }
};

class FixedCamera final : public CameraFrameProvider {
public:
    CameraFrameResult next{};

    FixedCamera()
    {
        next.diagnostic = CameraFrameDiagnostic::None;
        next.frame.inverse_view_projection_rows = {
            Float4{1.0F, 0.0F, 0.0F, 10.0F},
            Float4{0.0F, 1.0F, 0.0F, 20.0F},
            Float4{0.0F, 0.0F, 1.0F, 30.0F},
            Float4{0.0F, 0.0F, 0.0F, 1.0F},
        };
        next.frame.camera_world = {10.0F, 20.0F, 30.0F, 0.0F};
    }

    [[nodiscard]] CameraFrameResult Sample() noexcept override
    {
        return next;
    }
};

[[nodiscard]] bool Same(const Float4 left, const Float4 right) noexcept
{
    return left == right;
}

void PostLoadUsesExactUiNamesAndNeverHlslIdentifiers(Context& context)
{
    RecordingParameters parameters;
    const auto original = parameters.values;
    FixedCamera camera;
    RuntimeController controller{parameters, camera};

    controller.HandleCallback(enbcore::enb::CallbackId::OnInit);
    context.expect(parameters.calls.empty(),
        "OnInit performed parameter I/O outside parameter-ready callback");
    controller.HandleCallback(enbcore::enb::CallbackId::PostLoad);

    context.expect(parameters.calls.size() == 13U,
        "PostLoad did not capture and transactionally replace the parameters");
    for (std::size_t index = 0; index < kShaderParameterKeys.size(); ++index) {
        const ParameterCall& get = parameters.calls[index];
        context.expect(!get.write,
            "PostLoad baseline capture attempted a parameter write");
        context.expect(get.category == kShaderCategory,
            "GetParameter used the wrong shader category");
        context.expect(get.key == kShaderParameterKeys[index],
            "GetParameter used an HLSL identifier instead of the exact UIName key");
        context.expect(get.key.find("TruthInverse") == std::string::npos,
            "an HLSL identifier leaked into the ENB SDK binding");
    }
    constexpr std::array<std::size_t, 7> expected_write_order{5, 0, 1, 2, 3, 4, 5};
    for (std::size_t index = 0; index < expected_write_order.size(); ++index) {
        const ParameterCall& set = parameters.calls[index + 6U];
        context.expect(set.write && set.category == kShaderCategory,
            "PostLoad parameter write used the wrong operation or category");
        context.expect(set.key == kShaderParameterKeys[expected_write_order[index]],
            "PostLoad did not bracket payload writes with Status");
    }
    context.expect(parameters.calls[6].value.y == 0.0F,
        "PostLoad did not invalidate Status before writing payload data");
    context.expect(parameters.values[5].x == kProtocolVersion
            && parameters.values[5].y == 0.0F
            && parameters.values[5].z == 0.0F
            && parameters.values[5].w == kDefaultEngineWorldUnitsPerAuroraUnit,
        "PostLoad did not publish a fail-closed status payload");
    context.expect(controller.diagnostics().baseline_captured,
        "PostLoad did not retain the original shader values");
    context.expect(!Same(parameters.values[0], original[0]),
        "PostLoad left stale matrix data active");
}

void BeginFramePublishesCameraAndExactFoldedGeneration(Context& context)
{
    context.expect(FoldGenerationForShader(0U) == 0.0F
            && FoldGenerationForShader(1U) == 1.0F
            && FoldGenerationForShader(16'777'215U) == 16'777'215.0F
            && FoldGenerationForShader(16'777'216U) == 0.0F
            && FoldGenerationForShader(16'777'217U) == 1.0F,
        "generation folding left the exact IEEE-754 integer range");

    RecordingParameters parameters;
    FixedCamera camera;
    RuntimeController controller{parameters, camera};
    controller.HandleCallback(enbcore::enb::CallbackId::OnInit);
    controller.HandleCallback(enbcore::enb::CallbackId::PostLoad);
    parameters.calls.clear();

    controller.HandleCallback(enbcore::enb::CallbackId::BeginFrame);

    context.expect(parameters.calls.size() == 7U,
        "BeginFrame did not perform one complete transaction");
    for (const ParameterCall& call : parameters.calls) {
        context.expect(call.write, "BeginFrame attempted a parameter read");
    }
    context.expect(parameters.calls.front().key == kShaderParameterKeys[5]
            && parameters.calls.front().value.y == 0.0F,
        "BeginFrame did not invalidate Status before publishing frame data");
    context.expect(parameters.calls.back().key == kShaderParameterKeys[5]
            && parameters.calls.back().value.y == 1.0F,
        "BeginFrame did not commit valid Status last");
    for (std::size_t row = 0; row < 4U; ++row) {
        context.expect(Same(parameters.values[row],
            camera.next.frame.inverse_view_projection_rows[row]),
            "inverse VP row was published incorrectly");
    }
    context.expect(Same(parameters.values[4], camera.next.frame.camera_world),
        "camera world position was published incorrectly");
    context.expect(parameters.values[5].x == 1.0F
            && parameters.values[5].y == 1.0F
            && parameters.values[5].z == 1.0F
            && parameters.values[5].w == 4096.0F,
        "valid status payload was wrong");
    const RuntimeDiagnostics diagnostics = controller.diagnostics();
    context.expect(diagnostics.generation == 1U
            && diagnostics.frame_data_valid,
        "runtime diagnostics did not record the valid frame");
}

void PreSaveRestoresBaselineAndNextFrameReapplies(Context& context)
{
    RecordingParameters parameters;
    const auto baseline = parameters.values;
    FixedCamera camera;
    RuntimeController controller{parameters, camera};
    controller.HandleCallback(enbcore::enb::CallbackId::OnInit);
    controller.HandleCallback(enbcore::enb::CallbackId::PostLoad);
    controller.HandleCallback(enbcore::enb::CallbackId::BeginFrame);
    parameters.calls.clear();

    controller.HandleCallback(enbcore::enb::CallbackId::PreSave);
    context.expect(parameters.values == baseline,
        "PreSave did not restore all original shader values");
    context.expect(controller.diagnostics().state == RuntimeSessionState::SaveQuiesced,
        "PreSave did not quiesce runtime writes");

    parameters.calls.clear();
    controller.HandleCallback(enbcore::enb::CallbackId::EndFrame);
    context.expect(parameters.calls.empty(),
        "EndFrame reapplied runtime values before the verified barrier");
    controller.HandleCallback(enbcore::enb::CallbackId::BeginFrame);
    context.expect(parameters.values[5].y == 1.0F
            && parameters.values[5].z == 2.0F,
        "next BeginFrame did not reapply with a monotonic generation");
}

void InvalidCameraPublishesOnlyTheFailClosedContract(Context& context)
{
    RecordingParameters parameters;
    FixedCamera camera;
    RuntimeController controller{parameters, camera};
    controller.HandleCallback(enbcore::enb::CallbackId::OnInit);
    controller.HandleCallback(enbcore::enb::CallbackId::PostLoad);
    camera.next.diagnostic = CameraFrameDiagnostic::InvalidFrustum;
    controller.HandleCallback(enbcore::enb::CallbackId::BeginFrame);

    const std::array safe_rows{
        Float4{1.0F, 0.0F, 0.0F, 0.0F},
        Float4{0.0F, 1.0F, 0.0F, 0.0F},
        Float4{0.0F, 0.0F, 1.0F, 0.0F},
        Float4{0.0F, 0.0F, 0.0F, 1.0F},
    };
    for (std::size_t row = 0; row < 4U; ++row) {
        context.expect(parameters.values[row] == safe_rows[row],
            "invalid camera left stale inverse VP data active");
    }
    context.expect(parameters.values[4] == Float4{},
        "invalid camera left a stale world position active");
    context.expect(parameters.values[5].x == 1.0F
            && parameters.values[5].y == 0.0F
            && parameters.values[5].z == 0.0F,
        "invalid camera did not publish frame-valid zero");
    const RuntimeDiagnostics diagnostics = controller.diagnostics();
    context.expect(diagnostics.diagnostic == RuntimeDiagnostic::CameraFrameRejected
            && diagnostics.camera_diagnostic
                == CameraFrameDiagnostic::InvalidFrustum
            && !diagnostics.frame_data_valid,
        "invalid camera diagnostics were not exposed");
}

void BindingFailureRestoresBaselineAndPermanentlyFailsClosed(Context& context)
{
    RecordingParameters parameters;
    const auto baseline = parameters.values;
    FixedCamera camera;
    RuntimeController controller{parameters, camera};
    controller.HandleCallback(enbcore::enb::CallbackId::OnInit);
    controller.HandleCallback(enbcore::enb::CallbackId::PostLoad);
    parameters.fail_set_at = parameters.set_count + 2U;
    controller.HandleCallback(enbcore::enb::CallbackId::BeginFrame);

    context.expect(parameters.values == baseline,
        "failed live transaction was not rolled back to baseline");
    const RuntimeDiagnostics failed = controller.diagnostics();
    context.expect(failed.state == RuntimeSessionState::Failed
            && failed.diagnostic == RuntimeDiagnostic::LiveWriteFailed
            && !failed.frame_data_valid,
        "binding failure did not fail the controller closed");

    parameters.calls.clear();
    controller.HandleCallback(enbcore::enb::CallbackId::BeginFrame);
    context.expect(parameters.calls.empty(),
        "failed controller continued mutating shader parameters");
}

void MidWriteFailureInvalidatesFirstAndRestoresTheWholeBaseline(Context& context)
{
    RecordingParameters parameters;
    const auto baseline = parameters.values;
    FixedCamera camera;
    RuntimeController controller{parameters, camera};
    controller.HandleCallback(enbcore::enb::CallbackId::OnInit);
    controller.HandleCallback(enbcore::enb::CallbackId::PostLoad);
    const std::size_t first_live_set = parameters.set_count;
    parameters.fail_set_at = first_live_set + 3U;
    parameters.calls.clear();

    controller.HandleCallback(enbcore::enb::CallbackId::BeginFrame);

    context.expect(!parameters.calls.empty()
            && parameters.calls.front().key == kShaderParameterKeys[5]
            && parameters.calls.front().value.y == 0.0F,
        "failed transaction did not invalidate Status first");
    context.expect(parameters.values == baseline,
        "mid-write failure did not restore the complete baseline");
    const RuntimeDiagnostics diagnostics = controller.diagnostics();
    context.expect(diagnostics.state == RuntimeSessionState::Failed
            && diagnostics.diagnostic == RuntimeDiagnostic::LiveWriteFailed
            && diagnostics.parameter_set_failures == 1U
            && !diagnostics.baseline_restore_needed,
        "successful rollback did not clear restore-needed state");
}

void FailedRollbackIsRetriedByEveryLifecycleBarrier(Context& context)
{
    const auto exercise = [&context](
                              const enbcore::enb::CallbackId lifecycle,
                              const RuntimeSessionState expected_state,
                              const bool expected_baseline) {
        RecordingParameters parameters;
        const auto baseline = parameters.values;
        FixedCamera camera;
        RuntimeController controller{parameters, camera};
        controller.HandleCallback(enbcore::enb::CallbackId::OnInit);
        controller.HandleCallback(enbcore::enb::CallbackId::PostLoad);
        const std::size_t first_live_set = parameters.set_count;
        parameters.fail_set_calls = {
            first_live_set + 2U,
            first_live_set + 5U,
        };
        controller.HandleCallback(enbcore::enb::CallbackId::BeginFrame);

        RuntimeDiagnostics diagnostics = controller.diagnostics();
        context.expect(diagnostics.state == RuntimeSessionState::Failed
                && diagnostics.baseline_restore_needed,
            "rollback failure did not retain restore-needed state");
        context.expect(parameters.values[5].y == 0.0F,
            "rollback failure exposed valid Status over partial data");

        parameters.fail_set_calls.clear();
        parameters.calls.clear();
        controller.HandleCallback(lifecycle);
        diagnostics = controller.diagnostics();
        context.expect(parameters.values == baseline,
            "lifecycle barrier did not retry the failed baseline restore");
        context.expect(diagnostics.state == expected_state
                && diagnostics.baseline_captured == expected_baseline
                && !diagnostics.baseline_restore_needed,
            "lifecycle retry did not finish in the safe state");
        context.expect(parameters.calls.size() == 7U,
            "lifecycle retry did not perform a complete ordered restore");
    };

    exercise(enbcore::enb::CallbackId::PreSave,
        RuntimeSessionState::SaveQuiesced, true);
    exercise(enbcore::enb::CallbackId::PreReset,
        RuntimeSessionState::ResetQuiesced, false);
    exercise(enbcore::enb::CallbackId::OnExit,
        RuntimeSessionState::Stopped, false);
}

void RepeatedPostLoadRecapturesTheCurrentEffectDefaults(Context& context)
{
    const std::array<Float4, 6> active_reload{
        Float4{101, 102, 103, 104}, Float4{105, 106, 107, 108},
        Float4{109, 110, 111, 112}, Float4{113, 114, 115, 116},
        Float4{117, 118, 119, 120}, Float4{121, 122, 123, 124},
    };
    const std::array<Float4, 6> quiesced_reload{
        Float4{201, 202, 203, 204}, Float4{205, 206, 207, 208},
        Float4{209, 210, 211, 212}, Float4{213, 214, 215, 216},
        Float4{217, 218, 219, 220}, Float4{221, 222, 223, 224},
    };

    RecordingParameters parameters;
    FixedCamera camera;
    RuntimeController controller{parameters, camera};
    controller.HandleCallback(enbcore::enb::CallbackId::OnInit);
    controller.HandleCallback(enbcore::enb::CallbackId::PostLoad);
    controller.HandleCallback(enbcore::enb::CallbackId::BeginFrame);

    parameters.values = active_reload;
    parameters.calls.clear();
    controller.HandleCallback(enbcore::enb::CallbackId::PostLoad);
    context.expect(controller.diagnostics().state == RuntimeSessionState::Active
            && parameters.calls.size() == 13U,
        "active PostLoad did not recapture and rebind the recreated effect");
    controller.HandleCallback(enbcore::enb::CallbackId::PreSave);
    context.expect(parameters.values == active_reload,
        "active reload later restored the stale effect baseline");

    parameters.values = quiesced_reload;
    parameters.calls.clear();
    controller.HandleCallback(enbcore::enb::CallbackId::PostLoad);
    context.expect(controller.diagnostics().state == RuntimeSessionState::Active
            && parameters.calls.size() == 13U,
        "quiesced PostLoad did not recapture and rebind the recreated effect");
    controller.HandleCallback(enbcore::enb::CallbackId::OnExit);
    context.expect(parameters.values == quiesced_reload,
        "quiesced reload later restored the stale effect baseline");
}

void FailedReloadCaptureNeverRestoresThePreviousEffect(Context& context)
{
    RecordingParameters parameters;
    FixedCamera camera;
    RuntimeController controller{parameters, camera};
    controller.HandleCallback(enbcore::enb::CallbackId::OnInit);
    controller.HandleCallback(enbcore::enb::CallbackId::PostLoad);
    controller.HandleCallback(enbcore::enb::CallbackId::BeginFrame);

    const std::array<Float4, 6> replacement{
        Float4{301, 302, 303, 304}, Float4{305, 306, 307, 308},
        Float4{309, 310, 311, 312}, Float4{313, 314, 315, 316},
        Float4{317, 318, 319, 320}, Float4{321, 322, 323, 324},
    };
    parameters.values = replacement;
    parameters.fail_get_at = parameters.get_count + 2U;
    controller.HandleCallback(enbcore::enb::CallbackId::PostLoad);
    RuntimeDiagnostics diagnostics = controller.diagnostics();
    context.expect(diagnostics.state == RuntimeSessionState::Failed
            && !diagnostics.baseline_captured
            && !diagnostics.baseline_restore_needed,
        "failed reload capture retained the stale baseline");

    parameters.calls.clear();
    controller.HandleCallback(enbcore::enb::CallbackId::OnExit);
    diagnostics = controller.diagnostics();
    context.expect(parameters.values == replacement
            && parameters.calls.empty()
            && diagnostics.state == RuntimeSessionState::Stopped,
        "exit restored stale values into an effect whose capture failed");
}

void BaselineCaptureFailureNeverWrites(Context& context)
{
    RecordingParameters parameters;
    parameters.fail_get_at = 3U;
    FixedCamera camera;
    RuntimeController controller{parameters, camera};
    controller.HandleCallback(enbcore::enb::CallbackId::OnInit);
    controller.HandleCallback(enbcore::enb::CallbackId::PostLoad);

    const bool any_write = std::ranges::any_of(
        parameters.calls,
        [](const ParameterCall& call) { return call.write; });
    context.expect(!any_write, "partial baseline capture mutated shader values");
    context.expect(controller.diagnostics().state == RuntimeSessionState::Failed
            && controller.diagnostics().diagnostic
                == RuntimeDiagnostic::BaselineCaptureFailed,
        "baseline capture failure did not fail closed");
}

void ResetAndExitRestoreBeforeTheRuntimeStops(Context& context)
{
    RecordingParameters parameters;
    const auto baseline = parameters.values;
    FixedCamera camera;
    RuntimeController controller{parameters, camera};
    controller.HandleCallback(enbcore::enb::CallbackId::OnInit);
    controller.HandleCallback(enbcore::enb::CallbackId::PostLoad);
    controller.HandleCallback(enbcore::enb::CallbackId::BeginFrame);
    controller.HandleCallback(enbcore::enb::CallbackId::PreReset);
    context.expect(parameters.values == baseline
            && controller.diagnostics().state
                == RuntimeSessionState::ResetQuiesced
            && !controller.diagnostics().baseline_captured,
        "PreReset did not restore and release the baseline");
    controller.HandleCallback(enbcore::enb::CallbackId::PostReset);
    context.expect(controller.diagnostics().state
            == RuntimeSessionState::AwaitingPostLoad,
        "PostReset did not await recreated shader parameters");
    controller.HandleCallback(enbcore::enb::CallbackId::PostLoad);
    controller.HandleCallback(enbcore::enb::CallbackId::BeginFrame);
    controller.HandleCallback(enbcore::enb::CallbackId::OnExit);
    context.expect(parameters.values == baseline
            && controller.diagnostics().state == RuntimeSessionState::Stopped
            && !controller.diagnostics().baseline_captured,
        "OnExit did not restore before stopping");
    parameters.calls.clear();
    controller.HandleCallback(enbcore::enb::CallbackId::BeginFrame);
    context.expect(parameters.calls.empty(),
        "stopped runtime continued writing shader parameters");
}

constexpr truth::runtime::tests::TestCase kTests[]{
    {"PostLoad uses exact UI names and never HLSL identifiers",
        &PostLoadUsesExactUiNamesAndNeverHlslIdentifiers},
    {"BeginFrame publishes camera and exact folded generation",
        &BeginFramePublishesCameraAndExactFoldedGeneration},
    {"PreSave restores baseline and next frame reapplies",
        &PreSaveRestoresBaselineAndNextFrameReapplies},
    {"invalid camera publishes only the fail-closed contract",
        &InvalidCameraPublishesOnlyTheFailClosedContract},
    {"binding failure restores baseline and permanently fails closed",
        &BindingFailureRestoresBaselineAndPermanentlyFailsClosed},
    {"mid-write failure invalidates first and restores the whole baseline",
        &MidWriteFailureInvalidatesFirstAndRestoresTheWholeBaseline},
    {"failed rollback is retried by every lifecycle barrier",
        &FailedRollbackIsRetriedByEveryLifecycleBarrier},
    {"repeated PostLoad recaptures current effect defaults",
        &RepeatedPostLoadRecapturesTheCurrentEffectDefaults},
    {"failed reload capture never restores the previous effect",
        &FailedReloadCaptureNeverRestoresThePreviousEffect},
    {"baseline capture failure never writes", &BaselineCaptureFailureNeverWrites},
    {"reset and exit restore before the runtime stops",
        &ResetAndExitRestoreBeforeTheRuntimeStops},
};

} // namespace

int main()
{
    return truth::runtime::tests::Run(
        "Truth runtime controller cases", kTests, std::size(kTests));
}
