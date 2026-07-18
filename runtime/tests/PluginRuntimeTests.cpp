#include "TestHarness.hpp"

#include <truth/runtime/PluginRuntime.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

namespace {

using namespace truth::runtime;
using truth::runtime::tests::Context;

template <typename Integer>
void Append(std::vector<std::uint8_t>& bytes, const Integer value)
{
    using Unsigned = std::make_unsigned_t<Integer>;
    const Unsigned encoded = static_cast<Unsigned>(value);
    for (std::size_t index = 0; index < sizeof(Integer); ++index) {
        bytes.push_back(static_cast<std::uint8_t>(encoded >> (index * 8U)));
    }
}

[[nodiscard]] std::vector<std::uint8_t> Database()
{
    const RuntimeVersion version{1, 6, 1170, 0};
    std::vector<std::uint8_t> bytes;
    Append(bytes, std::int32_t{2});
    Append(bytes, version.major);
    Append(bytes, version.minor);
    Append(bytes, version.patch);
    Append(bytes, version.build);
    const std::string name = "SkyrimSE.exe";
    Append(bytes, static_cast<std::int32_t>(name.size()));
    bytes.insert(bytes.end(), name.begin(), name.end());
    Append(bytes, std::int32_t{8});
    Append(bytes, std::int32_t{1});
    bytes.push_back(0x00U);
    Append(bytes, kWorldRootCameraAeId);
    Append(bytes, std::uint64_t{0x1000U});
    return bytes;
}

class Platform final : public NativeRuntimePlatform {
public:
    CameraSnapshot camera{};

    Platform()
    {
        camera.world_to_camera.values = {
            1.0F, 0.0F, 0.0F, 0.0F,
            0.0F, 1.0F, 0.0F, 0.0F,
            0.0F, 0.0F, 1.0F, 0.0F,
            -10.0F, -20.0F, -30.0F, 1.0F,
        };
        camera.projection.view_frustum = {
            -1.0F, 1.0F, 1.0F, -1.0F, 1.0F, 100.0F, 0U, {0U, 0U, 0U},
        };
        camera.projection.viewport = {0.0F, 1.0F, 1.0F, 0.0F};
    }

    [[nodiscard]] bool QuerySkyrimModule(
        SkyrimModuleInfo& output) noexcept override
    {
        output = {
            L"SkyrimSE.exe",
            L"X:\\Fixture\\SkyrimSE.exe",
            RuntimeVersion{1, 6, 1170, 0},
            0x140000000ULL,
            0x100000U,
            kPeMachineAmd64,
        };
        return true;
    }

    [[nodiscard]] bool ReadFile(
        std::wstring_view,
        std::vector<std::uint8_t>& output) noexcept override
    {
        output = Database();
        return true;
    }

    [[nodiscard]] bool IsExecutableAddress(std::uintptr_t) const noexcept override
    {
        return true;
    }

    [[nodiscard]] std::uintptr_t InvokeWorldRootCamera(
        std::uintptr_t) const noexcept override
    {
        return 0x180000000ULL;
    }

    [[nodiscard]] bool Read(
        const std::uintptr_t address,
        const std::span<std::uint8_t> destination) const noexcept override
    {
        if (address == 0x180000000ULL + kNiCameraWorldToCameraOffset) {
            std::memcpy(destination.data(), &camera.world_to_camera, destination.size());
            return true;
        }
        if (address == 0x180000000ULL + kNiCameraFrustumViewportOffset) {
            std::memcpy(destination.data(), &camera.projection, destination.size());
            return true;
        }
        return false;
    }
};

std::array<Float4, 7> shader_values{
    Float4{1, 2, 3, 4}, Float4{5, 6, 7, 8},
    Float4{9, 10, 11, 12}, Float4{13, 14, 15, 16},
    Float4{17, 18, 19, 20}, Float4{21, 22, 23, 24},
    Float4{25, 26, 27, 28},
};
std::size_t sdk_calls = 0;

[[nodiscard]] std::size_t KeyIndex(const char* key)
{
    for (std::size_t index = 0; index < kShaderParameterKeys.size(); ++index) {
        if (kShaderParameterKeys[index] == key) {
            return index;
        }
    }
    return kShaderParameterKeys.size();
}

enbcore::enb::SdkBoolean GetParameter(
    char* filename,
    char* category,
    char* key,
    enbcore::enb::Parameter* output)
{
    ++sdk_calls;
    if (filename != nullptr || std::string_view{category} != kShaderCategory
        || output == nullptr) {
        return 0;
    }
    const std::size_t index = KeyIndex(key);
    if (index >= shader_values.size()) {
        return 0;
    }
    output->type = enbcore::enb::ParameterKind::Color4;
    output->size = 16U;
    std::memcpy(output->data.data(), &shader_values[index], 16U);
    return 1;
}

enbcore::enb::SdkBoolean SetParameter(
    char* filename,
    char* category,
    char* key,
    enbcore::enb::Parameter* input)
{
    ++sdk_calls;
    if (filename != nullptr || std::string_view{category} != kShaderCategory
        || input == nullptr || input->type != enbcore::enb::ParameterKind::Color4
        || input->size != 16U) {
        return 0;
    }
    const std::size_t index = KeyIndex(key);
    if (index >= shader_values.size()) {
        return 0;
    }
    std::memcpy(&shader_values[index], input->data.data(), 16U);
    return 1;
}

[[nodiscard]] enbcore::enb::HostResolutionResult Host()
{
    enbcore::enb::HostResolutionResult host;
    host.code = enbcore::enb::HostResolutionCode::Ready;
    host.exports_resolved = true;
    host.exports.get_parameter = &GetParameter;
    host.exports.set_parameter = &SetParameter;
    return host;
}

void PluginRuntimeKeepsAllSdkIoInsideCallbacks(Context& context)
{
    sdk_calls = 0;
    Platform platform;
    PluginRuntime runtime{platform};
    runtime.SetHost(Host());
    context.expect(sdk_calls == 0U, "host setup performed shader parameter I/O");
    runtime.HandleCallback(enbcore::enb::CallbackId::OnInit);
    context.expect(sdk_calls == 0U, "OnInit touched shader parameters");
    runtime.HandleCallback(enbcore::enb::CallbackId::PostLoad);
    context.expect(sdk_calls == 15U,
        "PostLoad did not execute one capture/publish transaction");
    runtime.HandleCallback(enbcore::enb::CallbackId::BeginFrame);
    context.expect(sdk_calls == 23U,
        "BeginFrame did not execute one live transaction");

    const PluginDiagnosticsV1 diagnostics = runtime.Snapshot();
    context.expect((diagnostics.flags & HostResolved) != 0U
            && (diagnostics.flags & InitializationAttempted) != 0U
            && (diagnostics.flags & NativeCameraReady) != 0U
            && (diagnostics.flags & BaselineCaptured) != 0U
            && (diagnostics.flags & FrameDataValid) != 0U,
        "plugin diagnostics omitted a ready capability");
    context.expect(diagnostics.generation == 1U
            && diagnostics.callbacks_received == 3U,
        "plugin diagnostics counters were wrong");
    context.expect(diagnostics.enb_parameter_diagnostic
            == static_cast<std::uint32_t>(EnbParameterDiagnostic::None),
        "callback-scoped parameter bridge reported an error");
}

void DiagnosticsAbiIsExactAndReservedSpaceIsZero(Context& context)
{
    static_assert(sizeof(PluginDiagnosticsV1) == 128U);
    static_assert(offsetof(PluginDiagnosticsV1, callbacks_received) == 48U);
    static_assert(offsetof(PluginDiagnosticsV1, relocation_address) == 88U);
    Platform platform;
    PluginRuntime runtime{platform};
    runtime.SetHost(Host());
    const PluginDiagnosticsV1 diagnostics = runtime.Snapshot();
    context.expect(diagnostics.size == sizeof(PluginDiagnosticsV1)
            && diagnostics.abi_version == 1U,
        "diagnostics ABI header was wrong");
    context.expect(diagnostics.reserved32_0 == 0U
            && diagnostics.reserved32_1 == 0U
            && diagnostics.reserved64_0 == 0U
            && diagnostics.reserved64_1 == 0U
            && diagnostics.reserved64_2 == 0U
            && diagnostics.reserved64_3 == 0U,
        "diagnostics reserved space was not zero");
}

constexpr truth::runtime::tests::TestCase kTests[]{
    {"plugin runtime keeps all SDK I/O inside callbacks",
        &PluginRuntimeKeepsAllSdkIoInsideCallbacks},
    {"diagnostics ABI is exact and reserved space is zero",
        &DiagnosticsAbiIsExactAndReservedSpaceIsZero},
};

} // namespace

int main()
{
    return truth::runtime::tests::Run(
        "Truth plugin runtime cases", kTests, std::size(kTests));
}
