#include <truth/runtime/DiagnosticsPublisher.hpp>
#include <truth/runtime/PluginRuntime.hpp>
#include <truth/runtime/windows/WindowsRuntimePlatform.hpp>

#include <enbcore/enb/LoadedHostResolver.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <cstdint>

namespace {

truth::runtime::windows::WindowsRuntimePlatform runtime_platform;
truth::runtime::PluginRuntime plugin_runtime{runtime_platform};
truth::runtime::DiagnosticsPublisher diagnostics_publisher;

#if defined(_MSC_VER)
void __stdcall TruthEnbCallback(const enbcore::enb::CallbackId callback) noexcept
#elif defined(__GNUC__) && defined(__i386__)
void __attribute__((stdcall)) TruthEnbCallback(
    const enbcore::enb::CallbackId callback) noexcept
#else
void TruthEnbCallback(const enbcore::enb::CallbackId callback) noexcept
#endif
{
    plugin_runtime.HandleCallback(callback);
    diagnostics_publisher.Publish(plugin_runtime.Snapshot());
}

void ResolveAndRegisterHost() noexcept
{
    enbcore::enb::WindowsLoadedModulePlatform platform;
    const enbcore::enb::HostResolutionResult host =
        enbcore::enb::ResolveLoadedEnbHost(platform);
    plugin_runtime.SetHost(host);
    diagnostics_publisher.Publish(plugin_runtime.Snapshot());
    if (host.exports_resolved
        && (host.code == enbcore::enb::HostResolutionCode::Ready
            || host.code == enbcore::enb::HostResolutionCode::NotReady)
        && host.exports.set_callback_function != nullptr) {
        host.exports.set_callback_function(&TruthEnbCallback);
    }
}

} // namespace

extern "C" __declspec(dllexport) BOOL WINAPI
TruthEnbRuntimeGetDiagnosticsV1(
    truth::runtime::PluginDiagnosticsV1* const output,
    const std::uint32_t output_size) noexcept
{
    if (output == nullptr || output_size < sizeof(*output)) {
        return FALSE;
    }
    truth::runtime::PluginDiagnosticsV1 snapshot;
    if (!diagnostics_publisher.Read(snapshot)) {
        return FALSE;
    }
    *output = snapshot;
    return TRUE;
}

BOOL WINAPI DllMain(
    HINSTANCE instance,
    const DWORD reason,
    LPVOID) noexcept
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        ResolveAndRegisterHost();
    }
    return TRUE;
}
