#include <truth/runtime/RuntimeContract.hpp>

#include <algorithm>
#include <bit>
#include <limits>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

namespace truth::runtime {
namespace {

class Reader final {
public:
    explicit Reader(const std::span<const std::uint8_t> bytes) noexcept:
        bytes_(bytes)
    {}

    template <typename Integer>
    [[nodiscard]] bool ReadUnsigned(Integer& value) noexcept
    {
        static_assert(std::is_unsigned_v<Integer>);
        if (remaining() < sizeof(Integer)) {
            return false;
        }
        value = 0;
        for (std::size_t index = 0; index < sizeof(Integer); ++index) {
            value |= static_cast<Integer>(bytes_[position_ + index])
                << (index * 8U);
        }
        position_ += sizeof(Integer);
        return true;
    }

    [[nodiscard]] bool ReadSigned32(std::int32_t& value) noexcept
    {
        std::uint32_t encoded = 0;
        if (!ReadUnsigned(encoded)) {
            return false;
        }
        value = std::bit_cast<std::int32_t>(encoded);
        return true;
    }

    [[nodiscard]] bool Skip(const std::size_t size) noexcept
    {
        if (remaining() < size) {
            return false;
        }
        position_ += size;
        return true;
    }

    [[nodiscard]] std::size_t remaining() const noexcept
    {
        return bytes_.size() - position_;
    }

private:
    std::span<const std::uint8_t> bytes_;
    std::size_t position_{0};
};

struct Entry final {
    std::uint64_t id{0};
    std::uint64_t offset{0};
};

enum class DecodeDiagnostic : std::uint8_t {
    None,
    Truncated,
    Invalid,
    Overflow,
};

[[nodiscard]] bool Add(
    const std::uint64_t left,
    const std::uint64_t right,
    std::uint64_t& result) noexcept
{
    if (right > (std::numeric_limits<std::uint64_t>::max)() - left) {
        return false;
    }
    result = left + right;
    return true;
}

[[nodiscard]] bool Subtract(
    const std::uint64_t left,
    const std::uint64_t right,
    std::uint64_t& result) noexcept
{
    if (right > left) {
        return false;
    }
    result = left - right;
    return true;
}

[[nodiscard]] DecodeDiagnostic Decode(
    Reader& reader,
    const std::uint8_t mode,
    const std::uint64_t previous,
    std::uint64_t& result) noexcept
{
    std::uint8_t value8 = 0;
    std::uint16_t value16 = 0;
    std::uint32_t value32 = 0;
    switch (mode) {
    case 0:
        return reader.ReadUnsigned(result)
            ? DecodeDiagnostic::None
            : DecodeDiagnostic::Truncated;
    case 1:
        return Add(previous, 1U, result)
            ? DecodeDiagnostic::None
            : DecodeDiagnostic::Overflow;
    case 2:
        if (!reader.ReadUnsigned(value8)) {
            return DecodeDiagnostic::Truncated;
        }
        return Add(previous, value8, result)
            ? DecodeDiagnostic::None
            : DecodeDiagnostic::Overflow;
    case 3:
        if (!reader.ReadUnsigned(value8)) {
            return DecodeDiagnostic::Truncated;
        }
        return Subtract(previous, value8, result)
            ? DecodeDiagnostic::None
            : DecodeDiagnostic::Overflow;
    case 4:
        if (!reader.ReadUnsigned(value16)) {
            return DecodeDiagnostic::Truncated;
        }
        return Add(previous, value16, result)
            ? DecodeDiagnostic::None
            : DecodeDiagnostic::Overflow;
    case 5:
        if (!reader.ReadUnsigned(value16)) {
            return DecodeDiagnostic::Truncated;
        }
        return Subtract(previous, value16, result)
            ? DecodeDiagnostic::None
            : DecodeDiagnostic::Overflow;
    case 6:
        if (!reader.ReadUnsigned(value16)) {
            return DecodeDiagnostic::Truncated;
        }
        result = value16;
        return DecodeDiagnostic::None;
    case 7:
        if (!reader.ReadUnsigned(value32)) {
            return DecodeDiagnostic::Truncated;
        }
        result = value32;
        return DecodeDiagnostic::None;
    default:
        return DecodeDiagnostic::Invalid;
    }
}

[[nodiscard]] AddressDatabaseDiagnostic MapDecode(
    const DecodeDiagnostic diagnostic) noexcept
{
    switch (diagnostic) {
    case DecodeDiagnostic::Truncated:
        return AddressDatabaseDiagnostic::Truncated;
    case DecodeDiagnostic::Invalid:
        return AddressDatabaseDiagnostic::InvalidEncoding;
    case DecodeDiagnostic::Overflow:
        return AddressDatabaseDiagnostic::ArithmeticOverflow;
    case DecodeDiagnostic::None:
    default:
        return AddressDatabaseDiagnostic::None;
    }
}

[[nodiscard]] AddressDatabaseResult Reject(
    const AddressDatabaseDiagnostic diagnostic) noexcept
{
    return AddressDatabaseResult{diagnostic, 0};
}

} // namespace

RuntimeSelection SelectRuntime(const RuntimeVersion version) noexcept
{
    if (version == RuntimeVersion{1, 5, 97, 0}) {
        return RuntimeSelection{
            RuntimeFamily::SpecialEdition,
            1U,
            kWorldRootCameraSeId,
        };
    }
    if (version.major == 1 && version.minor == 6
        && version.patch >= 0 && version.build >= 0) {
        return RuntimeSelection{
            RuntimeFamily::AnniversaryEdition,
            2U,
            kWorldRootCameraAeId,
        };
    }
    return {};
}

std::wstring BuildAddressLibraryRelativePath(const RuntimeVersion version)
{
    const RuntimeSelection selection = SelectRuntime(version);
    if (!selection.supported()) {
        return {};
    }
    const wchar_t* const file_prefix =
        selection.family == RuntimeFamily::SpecialEdition
        ? L"version-"
        : L"versionlib-";
    return L"Data\\SKSE\\Plugins\\" + std::wstring{file_prefix}
        + std::to_wstring(version.major) + L"-"
        + std::to_wstring(version.minor) + L"-"
        + std::to_wstring(version.patch) + L"-"
        + std::to_wstring(version.build) + L".bin";
}

AddressDatabaseResult ResolveAddressLibraryRelocation(
    const std::span<const std::uint8_t> bytes,
    const RuntimeVersion expected_runtime,
    const std::uintptr_t module_base,
    const std::size_t module_size) noexcept
{
    const RuntimeSelection selection = SelectRuntime(expected_runtime);
    if (!selection.supported()) {
        return Reject(AddressDatabaseDiagnostic::UnsupportedRuntime);
    }
    if (module_base == 0U || module_size == 0U
        || module_size > (std::numeric_limits<std::uintptr_t>::max)() - module_base) {
        return Reject(AddressDatabaseDiagnostic::InvalidModuleSize);
    }

    try {
        Reader reader{bytes};
        std::int32_t format = 0;
        if (!reader.ReadSigned32(format)) {
            return Reject(AddressDatabaseDiagnostic::Truncated);
        }
        if (format != selection.address_library_format) {
            return Reject(AddressDatabaseDiagnostic::WrongFormat);
        }

        RuntimeVersion observed;
        if (!reader.ReadSigned32(observed.major)
            || !reader.ReadSigned32(observed.minor)
            || !reader.ReadSigned32(observed.patch)
            || !reader.ReadSigned32(observed.build)) {
            return Reject(AddressDatabaseDiagnostic::Truncated);
        }
        if (observed != expected_runtime) {
            return Reject(AddressDatabaseDiagnostic::RuntimeMismatch);
        }

        std::int32_t name_length = 0;
        if (!reader.ReadSigned32(name_length)) {
            return Reject(AddressDatabaseDiagnostic::Truncated);
        }
        constexpr std::size_t maximum_name_length = 4096U;
        if (name_length < 0
            || static_cast<std::size_t>(name_length) > maximum_name_length) {
            return Reject(AddressDatabaseDiagnostic::InvalidNameLength);
        }
        const auto encoded_name_length = static_cast<std::size_t>(name_length);
        if (reader.remaining() < encoded_name_length
                + (2U * sizeof(std::int32_t))) {
            return Reject(AddressDatabaseDiagnostic::Truncated);
        }
        constexpr char expected_module_name[] = "SkyrimSE.exe";
        constexpr std::size_t expected_module_name_length =
            sizeof(expected_module_name) - 1U;
        if (encoded_name_length != expected_module_name_length) {
            return Reject(AddressDatabaseDiagnostic::ModuleNameMismatch);
        }
        bool module_name_matches = true;
        for (const char expected : expected_module_name) {
            if (expected == '\0') {
                break;
            }
            std::uint8_t observed_character = 0;
            if (!reader.ReadUnsigned(observed_character)) {
                return Reject(AddressDatabaseDiagnostic::Truncated);
            }
            module_name_matches = module_name_matches
                && observed_character == static_cast<std::uint8_t>(expected);
        }
        if (!module_name_matches) {
            return Reject(AddressDatabaseDiagnostic::ModuleNameMismatch);
        }

        std::int32_t pointer_size = 0;
        std::int32_t entry_count = 0;
        if (!reader.ReadSigned32(pointer_size)
            || !reader.ReadSigned32(entry_count)) {
            return Reject(AddressDatabaseDiagnostic::Truncated);
        }
        if (pointer_size != 8) {
            return Reject(AddressDatabaseDiagnostic::InvalidPointerSize);
        }
        constexpr std::int32_t maximum_entries = 4'000'000;
        if (entry_count <= 0 || entry_count > maximum_entries
            || static_cast<std::size_t>(entry_count) > reader.remaining()) {
            return Reject(AddressDatabaseDiagnostic::InvalidEntryCount);
        }

        std::vector<Entry> entries;
        entries.reserve(static_cast<std::size_t>(entry_count));
        std::uint64_t previous_id = 0;
        std::uint64_t previous_offset = 0;
        for (std::int32_t index = 0; index < entry_count; ++index) {
            std::uint8_t type = 0;
            if (!reader.ReadUnsigned(type)) {
                return Reject(AddressDatabaseDiagnostic::Truncated);
            }
            const std::uint8_t id_mode = type & 0x0FU;
            const std::uint8_t offset_mode = type >> 4U;
            if (id_mode > 7U) {
                return Reject(AddressDatabaseDiagnostic::InvalidEncoding);
            }

            std::uint64_t id = 0;
            DecodeDiagnostic decoded = Decode(reader, id_mode, previous_id, id);
            if (decoded != DecodeDiagnostic::None) {
                return Reject(MapDecode(decoded));
            }

            const bool scaled = (offset_mode & 8U) != 0U;
            if (scaled && previous_offset % 8U != 0U) {
                return Reject(AddressDatabaseDiagnostic::InvalidEncoding);
            }
            const std::uint64_t base = scaled
                ? previous_offset / 8U
                : previous_offset;
            std::uint64_t offset = 0;
            decoded = Decode(reader, offset_mode & 7U, base, offset);
            if (decoded != DecodeDiagnostic::None) {
                return Reject(MapDecode(decoded));
            }
            if (scaled) {
                if (offset > (std::numeric_limits<std::uint64_t>::max)() / 8U) {
                    return Reject(AddressDatabaseDiagnostic::ArithmeticOverflow);
                }
                offset *= 8U;
            }
            if (offset >= module_size) {
                return Reject(AddressDatabaseDiagnostic::OffsetOutsideModule);
            }
            entries.push_back({id, offset});
            previous_id = id;
            previous_offset = offset;
        }
        if (reader.remaining() != 0U) {
            return Reject(AddressDatabaseDiagnostic::TrailingData);
        }

        std::ranges::sort(entries, {}, &Entry::id);
        const auto duplicate = std::adjacent_find(
            entries.begin(),
            entries.end(),
            [](const Entry left, const Entry right) {
                return left.id == right.id;
            });
        if (duplicate != entries.end()) {
            return Reject(AddressDatabaseDiagnostic::DuplicateId);
        }
        const auto found = std::lower_bound(
            entries.begin(),
            entries.end(),
            Entry{selection.world_root_camera_id, 0},
            [](const Entry left, const Entry right) {
                return left.id < right.id;
            });
        if (found == entries.end()
            || found->id != selection.world_root_camera_id) {
            return Reject(AddressDatabaseDiagnostic::RelocationMissing);
        }
        const auto offset = static_cast<std::uintptr_t>(found->offset);
        return AddressDatabaseResult{
            AddressDatabaseDiagnostic::None,
            module_base + offset,
        };
    } catch (const std::bad_alloc&) {
        return Reject(AddressDatabaseDiagnostic::AllocationFailure);
    } catch (...) {
        return Reject(AddressDatabaseDiagnostic::Truncated);
    }
}

} // namespace truth::runtime
