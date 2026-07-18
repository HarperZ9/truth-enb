#include <truth/runtime/RuntimeController.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace truth::runtime {
namespace {

[[nodiscard]] bool Finite(const Float4 value) noexcept
{
    return std::isfinite(value.x)
        && std::isfinite(value.y)
        && std::isfinite(value.z)
        && std::isfinite(value.w);
}

[[nodiscard]] Float4 NormalizeCelestial(const CelestialFrame& celestial) noexcept
{
    const Float4 source = celestial.sun_direction_valid;
    if (!Finite(source) || source.w <= 0.5F) {
        return {};
    }
    const float length_squared =
        (source.x * source.x) + (source.y * source.y) + (source.z * source.z);
    if (!std::isfinite(length_squared) || length_squared <= 1.0e-8F) {
        return {};
    }
    const float inverse_length = 1.0F / std::sqrt(length_squared);
    return Float4{
        source.x * inverse_length,
        source.y * inverse_length,
        source.z * inverse_length,
        1.0F,
    };
}

[[nodiscard]] std::array<Float4, 7> SafePayload(
    const float scale,
    const float folded_generation,
    const bool valid,
    const CameraFrame* frame) noexcept
{
    std::array<Float4, 7> payload{
        Float4{1.0F, 0.0F, 0.0F, 0.0F},
        Float4{0.0F, 1.0F, 0.0F, 0.0F},
        Float4{0.0F, 0.0F, 1.0F, 0.0F},
        Float4{0.0F, 0.0F, 0.0F, 1.0F},
        Float4{},
        Float4{},
        Float4{
            kProtocolVersionWithCelestial,
            valid ? 1.0F : 0.0F,
            folded_generation,
            scale},
    };
    if (valid && frame != nullptr) {
        for (std::size_t row = 0; row < 4U; ++row) {
            payload[row] = frame->inverse_view_projection_rows[row];
        }
        payload[4] = frame->camera_world;
        payload[5] = NormalizeCelestial(frame->celestial);
    }
    return payload;
}

} // namespace

float FoldGenerationForShader(const std::uint64_t generation) noexcept
{
    return static_cast<float>(generation % kExactFloatGenerationModulus);
}

RuntimeController::RuntimeController(
    ShaderParameterApi& parameters,
    CameraFrameProvider& camera,
    const float engine_world_units_per_aurora_unit) noexcept:
    parameters_(parameters),
    camera_(camera),
    engine_world_units_per_aurora_unit_(engine_world_units_per_aurora_unit)
{
    if (!std::isfinite(engine_world_units_per_aurora_unit_)
        || engine_world_units_per_aurora_unit_ < 0.0001F
        || engine_world_units_per_aurora_unit_ > 1000000.0F) {
        diagnostics_.state = RuntimeSessionState::Failed;
        diagnostics_.diagnostic = RuntimeDiagnostic::InvalidWorldScale;
    }
}

bool RuntimeController::RestoreBaseline() noexcept
{
    if (!diagnostics_.baseline_captured) {
        return false;
    }
    if (!diagnostics_.baseline_restore_needed) {
        return true;
    }

    const Float4 invalid_status{
        kProtocolVersion,
        0.0F,
        0.0F,
        engine_world_units_per_aurora_unit_,
    };
    if (!parameters_.SetColor4(
            kShaderCategory, kShaderParameterKeys[6], invalid_status)) {
        ++diagnostics_.parameter_set_failures;
        return false;
    }

    bool restored = true;
    for (std::size_t index = 0; index < 6U; ++index) {
        if (!parameters_.SetColor4(
                kShaderCategory,
                kShaderParameterKeys[index],
                baseline_[index])) {
            ++diagnostics_.parameter_set_failures;
            restored = false;
        }
    }
    if (restored
        && !parameters_.SetColor4(
            kShaderCategory, kShaderParameterKeys[6], baseline_[6])) {
        ++diagnostics_.parameter_set_failures;
        restored = false;
    }
    if (restored) {
        diagnostics_.baseline_restore_needed = false;
    }
    return restored;
}

bool RuntimeController::PublishPayload(
    const std::array<Float4, 7>& payload) noexcept
{
    if (!diagnostics_.baseline_captured) {
        return false;
    }
    diagnostics_.baseline_restore_needed = true;

    Float4 invalid_status = payload[6];
    invalid_status.y = 0.0F;
    if (!parameters_.SetColor4(
            kShaderCategory, kShaderParameterKeys[6], invalid_status)) {
        ++diagnostics_.parameter_set_failures;
        static_cast<void>(RestoreBaseline());
        return false;
    }
    for (std::size_t index = 0; index < 6U; ++index) {
        if (!parameters_.SetColor4(
                kShaderCategory,
                kShaderParameterKeys[index],
                payload[index])) {
            ++diagnostics_.parameter_set_failures;
            static_cast<void>(RestoreBaseline());
            return false;
        }
    }
    if (!parameters_.SetColor4(
            kShaderCategory, kShaderParameterKeys[6], payload[6])) {
        ++diagnostics_.parameter_set_failures;
        static_cast<void>(RestoreBaseline());
        return false;
    }
    return true;
}

void RuntimeController::HandleCallback(
    const enbcore::enb::CallbackId callback) noexcept
{
    ++diagnostics_.callbacks_received;
    if (diagnostics_.state == RuntimeSessionState::Stopped) {
        return;
    }
    const bool lifecycle_barrier =
        callback == enbcore::enb::CallbackId::PreSave
        || callback == enbcore::enb::CallbackId::PreReset
        || callback == enbcore::enb::CallbackId::OnExit;
    if (diagnostics_.state == RuntimeSessionState::Failed
        && !lifecycle_barrier) {
        return;
    }

    switch (callback) {
    case enbcore::enb::CallbackId::OnInit:
        if (diagnostics_.state == RuntimeSessionState::Cold) {
            diagnostics_.state = RuntimeSessionState::AwaitingPostLoad;
            diagnostics_.diagnostic = RuntimeDiagnostic::None;
        }
        return;

    case enbcore::enb::CallbackId::PostLoad: {
        if (diagnostics_.state != RuntimeSessionState::AwaitingPostLoad
            && diagnostics_.state != RuntimeSessionState::Cold
            && diagnostics_.state != RuntimeSessionState::Active
            && diagnostics_.state != RuntimeSessionState::SaveQuiesced) {
            diagnostics_.diagnostic = RuntimeDiagnostic::InvalidCallbackForState;
            return;
        }
        // PostLoad means ENB has recreated or reloaded the parameter set. The
        // previous baseline belongs to the previous effect and must never be
        // written into this one, even if the new capture fails part-way.
        diagnostics_.baseline_captured = false;
        diagnostics_.baseline_restore_needed = false;
        diagnostics_.frame_data_valid = false;
        std::array<Float4, 7> candidate{};
        for (std::size_t index = 0; index < candidate.size(); ++index) {
            if (!parameters_.GetColor4(
                    kShaderCategory,
                    kShaderParameterKeys[index],
                    candidate[index])) {
                ++diagnostics_.parameter_get_failures;
                diagnostics_.state = RuntimeSessionState::Failed;
                diagnostics_.diagnostic = RuntimeDiagnostic::BaselineCaptureFailed;
                return;
            }
            if (!Finite(candidate[index])) {
                diagnostics_.state = RuntimeSessionState::Failed;
                diagnostics_.diagnostic = RuntimeDiagnostic::BaselineInvalid;
                return;
            }
        }
        baseline_ = candidate;
        diagnostics_.baseline_captured = true;
        const auto safe = SafePayload(
            engine_world_units_per_aurora_unit_, 0.0F, false, nullptr);
        if (!PublishPayload(safe)) {
            diagnostics_.state = RuntimeSessionState::Failed;
            diagnostics_.diagnostic = RuntimeDiagnostic::LiveWriteFailed;
            diagnostics_.frame_data_valid = false;
            return;
        }
        diagnostics_.state = RuntimeSessionState::Active;
        diagnostics_.diagnostic = RuntimeDiagnostic::None;
        diagnostics_.camera_diagnostic = CameraFrameDiagnostic::None;
        diagnostics_.frame_data_valid = false;
        return;
    }

    case enbcore::enb::CallbackId::BeginFrame: {
        if (diagnostics_.state != RuntimeSessionState::Active
            && diagnostics_.state != RuntimeSessionState::SaveQuiesced) {
            return;
        }
        const CameraFrameResult sampled = camera_.Sample();
        std::array<Float4, 7> payload{};
        if (sampled.valid()) {
            ++diagnostics_.generation;
            payload = SafePayload(
                engine_world_units_per_aurora_unit_,
                FoldGenerationForShader(diagnostics_.generation),
                true,
                &sampled.frame);
        } else {
            payload = SafePayload(
                engine_world_units_per_aurora_unit_, 0.0F, false, nullptr);
        }
        if (!PublishPayload(payload)) {
            diagnostics_.state = RuntimeSessionState::Failed;
            diagnostics_.diagnostic = RuntimeDiagnostic::LiveWriteFailed;
            diagnostics_.frame_data_valid = false;
            return;
        }
        diagnostics_.state = RuntimeSessionState::Active;
        diagnostics_.camera_diagnostic = sampled.diagnostic;
        diagnostics_.frame_data_valid = sampled.valid();
        diagnostics_.diagnostic = sampled.valid()
            ? RuntimeDiagnostic::None
            : RuntimeDiagnostic::CameraFrameRejected;
        return;
    }

    case enbcore::enb::CallbackId::PreSave:
        if (diagnostics_.state != RuntimeSessionState::Active
            && diagnostics_.state != RuntimeSessionState::SaveQuiesced
            && diagnostics_.state != RuntimeSessionState::Failed) {
            diagnostics_.diagnostic = RuntimeDiagnostic::InvalidCallbackForState;
            return;
        }
        if (!diagnostics_.baseline_captured) {
            return;
        }
        if (!RestoreBaseline()) {
            diagnostics_.state = RuntimeSessionState::Failed;
            diagnostics_.diagnostic = RuntimeDiagnostic::BaselineRestoreFailed;
            diagnostics_.frame_data_valid = false;
            return;
        }
        diagnostics_.state = RuntimeSessionState::SaveQuiesced;
        diagnostics_.diagnostic = RuntimeDiagnostic::None;
        diagnostics_.frame_data_valid = false;
        return;

    case enbcore::enb::CallbackId::PreReset:
        if (diagnostics_.baseline_captured
            && !RestoreBaseline()) {
            diagnostics_.state = RuntimeSessionState::Failed;
            diagnostics_.diagnostic = RuntimeDiagnostic::BaselineRestoreFailed;
            return;
        }
        diagnostics_.baseline_captured = false;
        diagnostics_.baseline_restore_needed = false;
        diagnostics_.frame_data_valid = false;
        diagnostics_.state = RuntimeSessionState::ResetQuiesced;
        diagnostics_.diagnostic = RuntimeDiagnostic::None;
        return;

    case enbcore::enb::CallbackId::PostReset:
        if (diagnostics_.state == RuntimeSessionState::ResetQuiesced) {
            diagnostics_.state = RuntimeSessionState::AwaitingPostLoad;
            diagnostics_.diagnostic = RuntimeDiagnostic::None;
        }
        return;

    case enbcore::enb::CallbackId::OnExit:
        if (diagnostics_.baseline_captured
            && !RestoreBaseline()) {
            diagnostics_.state = RuntimeSessionState::Failed;
            diagnostics_.diagnostic = RuntimeDiagnostic::BaselineRestoreFailed;
            return;
        }
        diagnostics_.baseline_captured = false;
        diagnostics_.baseline_restore_needed = false;
        diagnostics_.frame_data_valid = false;
        diagnostics_.state = RuntimeSessionState::Stopped;
        diagnostics_.diagnostic = RuntimeDiagnostic::None;
        return;

    case enbcore::enb::CallbackId::EndFrame:
        return;
    }
}

RuntimeDiagnostics RuntimeController::diagnostics() const noexcept
{
    return diagnostics_;
}

} // namespace truth::runtime
