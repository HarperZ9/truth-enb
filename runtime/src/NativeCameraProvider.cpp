#include <truth/runtime/NativeCameraProvider.hpp>

#include <filesystem>
#include <limits>

namespace truth::runtime {
namespace {

[[nodiscard]] bool ValidImageRange(
    const std::uintptr_t base,
    const std::size_t size) noexcept
{
    return base != 0U && size != 0U
        && size <= (std::numeric_limits<std::uintptr_t>::max)() - base;
}

} // namespace

NativeCameraFrameProvider::NativeCameraFrameProvider(
    NativeRuntimePlatform& platform) noexcept:
    platform_(platform)
{}

bool NativeCameraFrameProvider::Initialize() noexcept
{
    diagnostics_ = {};
    SkyrimModuleInfo module;
    if (!platform_.QuerySkyrimModule(module)) {
        diagnostics_.diagnostic = NativeCameraDiagnostic::ModuleUnavailable;
        return false;
    }
    diagnostics_.runtime_version = module.runtime_version;
    if (module.module_name != L"SkyrimSE.exe") {
        diagnostics_.diagnostic = NativeCameraDiagnostic::WrongModuleName;
        return false;
    }
    if (module.pe_machine != kPeMachineAmd64) {
        diagnostics_.diagnostic = NativeCameraDiagnostic::UnsupportedArchitecture;
        return false;
    }
    if (!ValidImageRange(module.image_base, module.image_size)) {
        diagnostics_.diagnostic = NativeCameraDiagnostic::InvalidModuleRange;
        return false;
    }

    const RuntimeSelection selection = SelectRuntime(module.runtime_version);
    if (!selection.supported()) {
        diagnostics_.diagnostic = NativeCameraDiagnostic::UnsupportedRuntime;
        return false;
    }
    diagnostics_.relocation_id = selection.world_root_camera_id;

    try {
        const std::filesystem::path executable{module.executable_path};
        if (!executable.is_absolute()
            || executable.filename().wstring() != L"SkyrimSE.exe") {
            diagnostics_.diagnostic = NativeCameraDiagnostic::WrongExecutablePath;
            return false;
        }
        const std::filesystem::path relative{
            BuildAddressLibraryRelativePath(module.runtime_version)};
        if (relative.empty() || relative.is_absolute()) {
            diagnostics_.diagnostic = NativeCameraDiagnostic::AddressLibraryPathInvalid;
            return false;
        }
        const std::filesystem::path game_directory = executable.parent_path();
        const std::filesystem::path database =
            (game_directory / relative).lexically_normal();
        const std::filesystem::path round_trip =
            database.lexically_relative(game_directory);
        if (round_trip.empty() || round_trip != relative.lexically_normal()) {
            diagnostics_.diagnostic = NativeCameraDiagnostic::AddressLibraryPathInvalid;
            return false;
        }
        diagnostics_.address_library_path = database.wstring();

        std::vector<std::uint8_t> bytes;
        if (!platform_.ReadFile(diagnostics_.address_library_path, bytes)) {
            diagnostics_.diagnostic = NativeCameraDiagnostic::AddressLibraryReadFailed;
            return false;
        }
        if (bytes.empty() || bytes.size() > kMaximumAddressLibraryBytes) {
            diagnostics_.diagnostic = NativeCameraDiagnostic::AddressLibrarySizeInvalid;
            return false;
        }
        const AddressDatabaseResult database_result =
            ResolveAddressLibraryRelocation(
                bytes,
                module.runtime_version,
                module.image_base,
                module.image_size);
        diagnostics_.address_database_diagnostic = database_result.diagnostic;
        if (!database_result.accepted()) {
            diagnostics_.diagnostic = NativeCameraDiagnostic::AddressLibraryRejected;
            return false;
        }
        if (!platform_.IsExecutableAddress(database_result.resolved_address)) {
            diagnostics_.diagnostic = NativeCameraDiagnostic::RelocationNotExecutable;
            return false;
        }
        diagnostics_.relocation_address = database_result.resolved_address;
        diagnostics_.diagnostic = NativeCameraDiagnostic::None;
        diagnostics_.ready = true;
        return true;
    } catch (...) {
        diagnostics_.diagnostic = NativeCameraDiagnostic::AddressLibraryPathInvalid;
        diagnostics_.ready = false;
        return false;
    }
}

CameraFrameResult NativeCameraFrameProvider::Sample() noexcept
{
    if (!diagnostics_.ready || diagnostics_.relocation_address == 0U) {
        return CameraFrameResult{CameraFrameDiagnostic::RuntimeNotReady, {}};
    }
    const std::uintptr_t camera = platform_.InvokeWorldRootCamera(
        diagnostics_.relocation_address);
    if (camera == 0U) {
        return CameraFrameResult{CameraFrameDiagnostic::CameraLocatorFailed, {}};
    }
    return ReadCameraFrame(platform_, camera);
}

const NativeCameraDiagnostics&
NativeCameraFrameProvider::diagnostics() const noexcept
{
    return diagnostics_;
}

} // namespace truth::runtime
