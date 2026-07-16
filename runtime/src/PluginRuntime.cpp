#include <truth/runtime/PluginRuntime.hpp>

namespace truth::runtime {

PluginRuntime::PluginRuntime(NativeRuntimePlatform& platform) noexcept:
    camera_(platform),
    controller_(parameters_, camera_)
{}

void PluginRuntime::SetHost(
    const enbcore::enb::HostResolutionResult host) noexcept
{
    host_code_ = host.code;
    host_resolved_ = host.exports_resolved
        && (host.code == enbcore::enb::HostResolutionCode::Ready
            || host.code == enbcore::enb::HostResolutionCode::NotReady)
        && host.exports.get_parameter != nullptr
        && host.exports.set_parameter != nullptr;
    parameters_.SetExports(host_resolved_
        ? host.exports
        : enbcore::enb::SdkExports{});
}

void PluginRuntime::HandleCallback(
    const enbcore::enb::CallbackId callback) noexcept
{
    if (!host_resolved_) {
        return;
    }
    auto scope = parameters_.EnterCallback();
    if (!scope.active()) {
        return;
    }
    if (!initialization_attempted_
        && (callback == enbcore::enb::CallbackId::OnInit
            || callback == enbcore::enb::CallbackId::PostLoad
            || callback == enbcore::enb::CallbackId::BeginFrame)) {
        initialization_attempted_ = true;
        static_cast<void>(camera_.Initialize());
    }
    controller_.HandleCallback(callback);
}

PluginDiagnosticsV1 PluginRuntime::Snapshot() const noexcept
{
    const RuntimeDiagnostics runtime = controller_.diagnostics();
    const NativeCameraDiagnostics& native = camera_.diagnostics();
    PluginDiagnosticsV1 result;
    result.host_resolution_code = static_cast<std::uint32_t>(host_code_);
    if (host_resolved_) {
        result.flags |= PluginDiagnosticFlags::HostResolved;
    }
    if (initialization_attempted_) {
        result.flags |= PluginDiagnosticFlags::InitializationAttempted;
    }
    if (native.ready) {
        result.flags |= PluginDiagnosticFlags::NativeCameraReady;
    }
    if (runtime.baseline_captured) {
        result.flags |= PluginDiagnosticFlags::BaselineCaptured;
    }
    if (runtime.frame_data_valid) {
        result.flags |= PluginDiagnosticFlags::FrameDataValid;
    }
    result.session_state = static_cast<std::uint32_t>(runtime.state);
    result.runtime_diagnostic = static_cast<std::uint32_t>(runtime.diagnostic);
    result.camera_diagnostic = static_cast<std::uint32_t>(runtime.camera_diagnostic);
    result.native_camera_diagnostic =
        static_cast<std::uint32_t>(native.diagnostic);
    result.address_database_diagnostic =
        static_cast<std::uint32_t>(native.address_database_diagnostic);
    result.enb_parameter_diagnostic =
        static_cast<std::uint32_t>(parameters_.diagnostic());
    result.callbacks_received = runtime.callbacks_received;
    result.generation = runtime.generation;
    result.parameter_get_failures = runtime.parameter_get_failures;
    result.parameter_set_failures = runtime.parameter_set_failures;
    result.relocation_id = native.relocation_id;
    result.relocation_address = native.relocation_address;
    return result;
}

} // namespace truth::runtime
