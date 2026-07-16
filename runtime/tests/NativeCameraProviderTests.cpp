#include "TestHarness.hpp"

#include <truth/runtime/NativeCameraProvider.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
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

[[nodiscard]] std::vector<std::uint8_t> Database(
    const RuntimeVersion version,
    const std::uint64_t relocation_id,
    const std::uint64_t offset)
{
    const RuntimeSelection selection = SelectRuntime(version);
    std::vector<std::uint8_t> bytes;
    Append(bytes, static_cast<std::int32_t>(selection.address_library_format));
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
    Append(bytes, relocation_id);
    Append(bytes, offset);
    return bytes;
}

[[nodiscard]] CameraSnapshot Camera()
{
    CameraSnapshot snapshot{};
    snapshot.world_to_camera.values = {
        1.0F, 0.0F, 0.0F, 0.0F,
        0.0F, 1.0F, 0.0F, 0.0F,
        0.0F, 0.0F, 1.0F, 0.0F,
        -100.0F, -200.0F, -300.0F, 1.0F,
    };
    snapshot.projection.view_frustum = {
        -1.0F, 1.0F, 1.0F, -1.0F, 1.0F, 1000.0F, 0U, {0U, 0U, 0U},
    };
    snapshot.projection.viewport = {0.0F, 1.0F, 1.0F, 0.0F};
    return snapshot;
}

class FakePlatform final : public NativeRuntimePlatform {
public:
    SkyrimModuleInfo module{
        L"SkyrimSE.exe",
        L"X:\\Fixture\\Skyrim Special Edition\\SkyrimSE.exe",
        RuntimeVersion{1, 6, 1170, 0},
        0x140000000ULL,
        0x02000000U,
        kPeMachineAmd64,
    };
    std::vector<std::uint8_t> database{
        Database(module.runtime_version, kWorldRootCameraAeId, 0x00123450U)};
    CameraSnapshot camera{Camera()};
    bool query_succeeds{true};
    bool read_file_succeeds{true};
    bool executable{true};
    std::uintptr_t camera_address{0x180000000ULL};
    mutable std::vector<std::uintptr_t> memory_reads;
    mutable std::vector<std::uintptr_t> invocations;
    std::vector<std::wstring> file_reads;

    [[nodiscard]] bool QuerySkyrimModule(
        SkyrimModuleInfo& output) noexcept override
    {
        if (!query_succeeds) {
            return false;
        }
        output = module;
        return true;
    }

    [[nodiscard]] bool ReadFile(
        const std::wstring_view path,
        std::vector<std::uint8_t>& bytes) noexcept override
    {
        file_reads.emplace_back(path);
        if (!read_file_succeeds) {
            return false;
        }
        bytes = database;
        return true;
    }

    [[nodiscard]] bool IsExecutableAddress(
        const std::uintptr_t) const noexcept override
    {
        return executable;
    }

    [[nodiscard]] std::uintptr_t InvokeWorldRootCamera(
        const std::uintptr_t address) const noexcept override
    {
        invocations.push_back(address);
        return camera_address;
    }

    [[nodiscard]] bool Read(
        const std::uintptr_t address,
        const std::span<std::uint8_t> destination) const noexcept override
    {
        memory_reads.push_back(address);
        if (address == camera_address + kNiCameraWorldToCameraOffset
            && destination.size() == sizeof(Matrix4)) {
            std::memcpy(destination.data(), &camera.world_to_camera, destination.size());
            return true;
        }
        if (address == camera_address + kNiCameraFrustumViewportOffset
            && destination.size() == sizeof(NiCameraRuntimeData2Abi)) {
            std::memcpy(destination.data(), &camera.projection, destination.size());
            return true;
        }
        return false;
    }
};

void InitializationUsesTheExactRuntimeDatabaseAndRelocation(Context& context)
{
    FakePlatform platform;
    NativeCameraFrameProvider provider{platform};
    context.expect(provider.Initialize(), "valid native camera source was rejected");
    const NativeCameraDiagnostics& diagnostics = provider.diagnostics();
    context.expect(diagnostics.ready
            && diagnostics.diagnostic == NativeCameraDiagnostic::None,
        "native camera source was not marked ready");
    context.expect(diagnostics.relocation_id == kWorldRootCameraAeId,
        "AE did not choose relocation 36609");
    context.expect(diagnostics.relocation_address
            == platform.module.image_base + 0x00123450U,
        "Address Library offset was not rebased to SkyrimSE.exe");
    context.expect(platform.file_reads.size() == 1U
            && platform.file_reads[0]
                == L"X:\\Fixture\\Skyrim Special Edition\\Data\\SKSE\\Plugins\\versionlib-1-6-1170-0.bin",
        "provider did not open the one exact runtime database path");

    const CameraFrameResult frame = provider.Sample();
    context.expect(frame.valid(), "ready provider rejected the live camera");
    context.expect(platform.invocations.size() == 1U
            && platform.invocations[0] == diagnostics.relocation_address,
        "provider invoked an address other than validated WorldRootCamera");
    context.expect(frame.frame.camera_world.x == 100.0F
            && frame.frame.camera_world.y == 200.0F
            && frame.frame.camera_world.z == 300.0F,
        "provider did not derive the live camera position");
}

void SeUsesItsExactIdAndStillRejectsVr(Context& context)
{
    FakePlatform platform;
    platform.module.runtime_version = {1, 5, 97, 0};
    platform.database = Database(
        platform.module.runtime_version, kWorldRootCameraSeId, 0x00234560U);
    NativeCameraFrameProvider provider{platform};
    context.expect(provider.Initialize(), "SE 1.5.97 source was rejected");
    context.expect(provider.diagnostics().relocation_id == kWorldRootCameraSeId,
        "SE did not select relocation 35601");
    context.expect(platform.file_reads[0].ends_with(
            L"Data\\SKSE\\Plugins\\version-1-5-97-0.bin"),
        "SE database path was not exact");

    FakePlatform vr;
    vr.module.module_name = L"SkyrimVR.exe";
    vr.module.executable_path = L"X:\\Fixture\\Skyrim VR\\SkyrimVR.exe";
    vr.module.runtime_version = {1, 4, 15, 0};
    NativeCameraFrameProvider rejected{vr};
    context.expect(!rejected.Initialize(), "Skyrim VR was admitted");
    context.expect(vr.file_reads.empty() && vr.invocations.empty(),
        "VR rejection touched the database or camera locator");
}

void ModuleDatabaseAndExecutableFailuresFailClosed(Context& context)
{
    FakePlatform missing_module;
    missing_module.query_succeeds = false;
    NativeCameraFrameProvider missing_provider{missing_module};
    context.expect(!missing_provider.Initialize()
            && missing_provider.diagnostics().diagnostic
                == NativeCameraDiagnostic::ModuleUnavailable,
        "missing Skyrim module was admitted");

    FakePlatform wrong_name;
    wrong_name.module.module_name = L"TESV.exe";
    NativeCameraFrameProvider wrong_name_provider{wrong_name};
    context.expect(!wrong_name_provider.Initialize()
            && wrong_name_provider.diagnostics().diagnostic
                == NativeCameraDiagnostic::WrongModuleName,
        "wrong module name was admitted");

    FakePlatform wrong_database;
    wrong_database.database = Database(
        {1, 6, 1130, 0}, kWorldRootCameraAeId, 0x1000U);
    NativeCameraFrameProvider wrong_database_provider{wrong_database};
    context.expect(!wrong_database_provider.Initialize()
            && wrong_database_provider.diagnostics().diagnostic
                == NativeCameraDiagnostic::AddressLibraryRejected
            && wrong_database_provider.diagnostics().address_database_diagnostic
                == AddressDatabaseDiagnostic::RuntimeMismatch,
        "wrong-runtime database was admitted");

    FakePlatform data_address;
    data_address.executable = false;
    NativeCameraFrameProvider data_address_provider{data_address};
    context.expect(!data_address_provider.Initialize()
            && data_address_provider.diagnostics().diagnostic
                == NativeCameraDiagnostic::RelocationNotExecutable,
        "non-executable relocation was admitted");
    context.expect(data_address.invocations.empty(),
        "rejected relocation was invoked");
}

void SamplingBeforeReadinessOrWithNullCameraIsSafe(Context& context)
{
    FakePlatform platform;
    NativeCameraFrameProvider provider{platform};
    context.expect(provider.Sample().diagnostic
            == CameraFrameDiagnostic::RuntimeNotReady,
        "uninitialized provider attempted a camera read");
    context.expect(platform.invocations.empty() && platform.memory_reads.empty(),
        "uninitialized provider touched engine memory");

    context.expect(provider.Initialize(), "valid provider did not initialize");
    platform.camera_address = 0U;
    context.expect(provider.Sample().diagnostic
            == CameraFrameDiagnostic::CameraLocatorFailed,
        "null WorldRootCamera result was admitted");
    context.expect(platform.memory_reads.empty(),
        "null camera result was dereferenced");
}

constexpr truth::runtime::tests::TestCase kTests[]{
    {"initialization uses exact runtime database and relocation",
        &InitializationUsesTheExactRuntimeDatabaseAndRelocation},
    {"SE uses exact ID and VR is rejected", &SeUsesItsExactIdAndStillRejectsVr},
    {"module, database, and executable failures fail closed",
        &ModuleDatabaseAndExecutableFailuresFailClosed},
    {"sampling before readiness or with null camera is safe",
        &SamplingBeforeReadinessOrWithNullCameraIsSafe},
};

} // namespace

int main()
{
    return truth::runtime::tests::Run(
        "Truth native camera provider cases", kTests, std::size(kTests));
}
