#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace truth::runtime {

struct RuntimeVersion final {
    std::int32_t major{0};
    std::int32_t minor{0};
    std::int32_t patch{0};
    std::int32_t build{0};

    [[nodiscard]] constexpr bool operator==(
        const RuntimeVersion&) const noexcept = default;
};

enum class RuntimeFamily : std::uint8_t {
    Unsupported = 0,
    SpecialEdition = 1,
    AnniversaryEdition = 2,
};

struct RuntimeSelection final {
    RuntimeFamily family{RuntimeFamily::Unsupported};
    std::uint8_t address_library_format{0};
    std::uint64_t world_root_camera_id{0};

    [[nodiscard]] constexpr bool supported() const noexcept
    {
        return family != RuntimeFamily::Unsupported;
    }
};

inline constexpr std::uint64_t kWorldRootCameraSeId = 35601U;
inline constexpr std::uint64_t kWorldRootCameraAeId = 36609U;

[[nodiscard]] RuntimeSelection SelectRuntime(RuntimeVersion version) noexcept;
[[nodiscard]] std::wstring BuildAddressLibraryRelativePath(
    RuntimeVersion version);

enum class AddressDatabaseDiagnostic : std::uint16_t {
    None = 0,
    UnsupportedRuntime = 1,
    Truncated = 2,
    WrongFormat = 3,
    RuntimeMismatch = 4,
    InvalidNameLength = 5,
    InvalidPointerSize = 6,
    InvalidEntryCount = 7,
    InvalidEncoding = 8,
    ArithmeticOverflow = 9,
    OffsetOutsideModule = 10,
    DuplicateId = 11,
    TrailingData = 12,
    RelocationMissing = 13,
    AllocationFailure = 14,
    InvalidModuleSize = 15,
    ModuleNameMismatch = 16,
};

struct AddressDatabaseResult final {
    AddressDatabaseDiagnostic diagnostic{AddressDatabaseDiagnostic::UnsupportedRuntime};
    std::uintptr_t resolved_address{0};

    [[nodiscard]] constexpr bool accepted() const noexcept
    {
        return diagnostic == AddressDatabaseDiagnostic::None;
    }
};

[[nodiscard]] AddressDatabaseResult ResolveAddressLibraryRelocation(
    std::span<const std::uint8_t> bytes,
    RuntimeVersion expected_runtime,
    std::uintptr_t module_base,
    std::size_t module_size) noexcept;

} // namespace truth::runtime
