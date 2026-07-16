#pragma once

#include <enbcore/enb/LoadedHostResolver.hpp>

#include <truth/runtime/EnbParameterBridge.hpp>
#include <truth/runtime/NativeCameraProvider.hpp>

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace truth::runtime {

inline constexpr std::uint32_t kPluginDiagnosticsAbiVersion = 1U;

enum PluginDiagnosticFlags : std::uint32_t {
    HostResolved = 1U << 0U,
    InitializationAttempted = 1U << 1U,
    NativeCameraReady = 1U << 2U,
    BaselineCaptured = 1U << 3U,
    FrameDataValid = 1U << 4U,
};

struct PluginDiagnosticsV1 final {
    std::uint32_t size{sizeof(PluginDiagnosticsV1)};
    std::uint32_t abi_version{kPluginDiagnosticsAbiVersion};
    std::uint32_t host_resolution_code{0};
    std::uint32_t flags{0};
    std::uint32_t session_state{0};
    std::uint32_t runtime_diagnostic{0};
    std::uint32_t camera_diagnostic{0};
    std::uint32_t native_camera_diagnostic{0};
    std::uint32_t address_database_diagnostic{0};
    std::uint32_t enb_parameter_diagnostic{0};
    std::uint32_t reserved32_0{0};
    std::uint32_t reserved32_1{0};
    std::uint64_t callbacks_received{0};
    std::uint64_t generation{0};
    std::uint64_t parameter_get_failures{0};
    std::uint64_t parameter_set_failures{0};
    std::uint64_t relocation_id{0};
    std::uint64_t relocation_address{0};
    std::uint64_t reserved64_0{0};
    std::uint64_t reserved64_1{0};
    std::uint64_t reserved64_2{0};
    std::uint64_t reserved64_3{0};
};

static_assert(std::is_standard_layout_v<PluginDiagnosticsV1>);
static_assert(std::is_trivially_copyable_v<PluginDiagnosticsV1>);
static_assert(offsetof(PluginDiagnosticsV1, callbacks_received) == 0x30U);
static_assert(offsetof(PluginDiagnosticsV1, relocation_id) == 0x50U);
static_assert(sizeof(PluginDiagnosticsV1) == 0x80U);

class PluginRuntime final {
public:
    explicit PluginRuntime(NativeRuntimePlatform& platform) noexcept;

    void SetHost(enbcore::enb::HostResolutionResult host) noexcept;
    void HandleCallback(enbcore::enb::CallbackId callback) noexcept;
    [[nodiscard]] PluginDiagnosticsV1 Snapshot() const noexcept;

private:
    EnbShaderParameterApi parameters_{};
    NativeCameraFrameProvider camera_;
    RuntimeController controller_;
    enbcore::enb::HostResolutionCode host_code_{
        enbcore::enb::HostResolutionCode::HostNotFound};
    bool host_resolved_{false};
    bool initialization_attempted_{false};
};

} // namespace truth::runtime
