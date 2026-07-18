#pragma once

#include <truth/runtime/CameraFrame.hpp>
#include <truth/runtime/RuntimeContract.hpp>
#include <truth/runtime/RuntimeController.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace truth::runtime {

inline constexpr std::size_t kMaximumAddressLibraryBytes = 32U * 1024U * 1024U;
inline constexpr std::uint16_t kPeMachineAmd64 = 0x8664U;

struct SkyrimModuleInfo final {
    std::wstring module_name;
    std::wstring executable_path;
    RuntimeVersion runtime_version{};
    std::uintptr_t image_base{0};
    std::size_t image_size{0};
    std::uint16_t pe_machine{0};
};

class NativeRuntimePlatform : public ProcessMemoryReader {
public:
    ~NativeRuntimePlatform() override = default;

    [[nodiscard]] virtual bool QuerySkyrimModule(
        SkyrimModuleInfo& module) noexcept = 0;
    [[nodiscard]] virtual bool ReadFile(
        std::wstring_view absolute_path,
        std::vector<std::uint8_t>& bytes) noexcept = 0;
    [[nodiscard]] virtual bool IsExecutableAddress(
        std::uintptr_t address) const noexcept = 0;
    [[nodiscard]] virtual std::uintptr_t InvokeWorldRootCamera(
        std::uintptr_t function_address) const noexcept = 0;
    [[nodiscard]] virtual bool QuerySunDirection(
        Float4& direction) const noexcept
    {
        direction = {};
        return false;
    }
};

enum class NativeCameraDiagnostic : std::uint16_t {
    None = 0,
    ModuleUnavailable = 1,
    WrongModuleName = 2,
    WrongExecutablePath = 3,
    UnsupportedArchitecture = 4,
    InvalidModuleRange = 5,
    UnsupportedRuntime = 6,
    AddressLibraryPathInvalid = 7,
    AddressLibraryReadFailed = 8,
    AddressLibrarySizeInvalid = 9,
    AddressLibraryRejected = 10,
    RelocationNotExecutable = 11,
};

struct NativeCameraDiagnostics final {
    NativeCameraDiagnostic diagnostic{NativeCameraDiagnostic::ModuleUnavailable};
    AddressDatabaseDiagnostic address_database_diagnostic{
        AddressDatabaseDiagnostic::None};
    RuntimeVersion runtime_version{};
    std::uint64_t relocation_id{0};
    std::uintptr_t relocation_address{0};
    std::wstring address_library_path;
    bool ready{false};
};

class NativeCameraFrameProvider final : public CameraFrameProvider {
public:
    explicit NativeCameraFrameProvider(NativeRuntimePlatform& platform) noexcept;

    [[nodiscard]] bool Initialize() noexcept;
    [[nodiscard]] CameraFrameResult Sample() noexcept override;
    [[nodiscard]] const NativeCameraDiagnostics& diagnostics() const noexcept;

private:
    NativeRuntimePlatform& platform_;
    NativeCameraDiagnostics diagnostics_{};
};

} // namespace truth::runtime
