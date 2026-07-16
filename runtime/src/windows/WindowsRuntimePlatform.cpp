#include <truth/runtime/windows/WindowsRuntimePlatform.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <Psapi.h>

#include <algorithm>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

namespace truth::runtime::windows {
namespace {

class FileHandle final {
public:
    explicit FileHandle(const HANDLE value) noexcept:
        value_(value)
    {}

    ~FileHandle() noexcept
    {
        if (value_ != INVALID_HANDLE_VALUE) {
            CloseHandle(value_);
        }
    }

    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;

    [[nodiscard]] HANDLE get() const noexcept { return value_; }

private:
    HANDLE value_{INVALID_HANDLE_VALUE};
};

[[nodiscard]] bool ExecutableProtection(const DWORD protection) noexcept
{
    if ((protection & (PAGE_GUARD | PAGE_NOACCESS)) != 0U) {
        return false;
    }
    const DWORD access = protection & 0xFFU;
    return access == PAGE_EXECUTE || access == PAGE_EXECUTE_READ;
}

[[nodiscard]] bool ExactCurrentProcessRead(
    const std::uintptr_t address,
    void* destination,
    const std::size_t size) noexcept
{
    if (address == 0U || destination == nullptr || size == 0U
        || size > (std::numeric_limits<std::uintptr_t>::max)() - address) {
        return false;
    }
    SIZE_T bytes_read = 0;
    return ReadProcessMemory(
               GetCurrentProcess(),
               reinterpret_cast<const void*>(address),
               destination,
               size,
               &bytes_read)
            != FALSE
        && bytes_read == size;
}

[[nodiscard]] bool ReadFileVersion(
    const std::wstring& path,
    RuntimeVersion& version) noexcept
{
    DWORD ignored = 0;
    const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &ignored);
    if (size == 0U) {
        return false;
    }
    try {
        std::vector<std::uint8_t> bytes(size);
        if (GetFileVersionInfoW(path.c_str(), 0U, size, bytes.data()) == FALSE) {
            return false;
        }
        VS_FIXEDFILEINFO* info = nullptr;
        UINT info_size = 0;
        if (VerQueryValueW(
                bytes.data(),
                L"\\",
                reinterpret_cast<void**>(&info),
                &info_size) == FALSE
            || info == nullptr
            || info_size < sizeof(VS_FIXEDFILEINFO)
            || info->dwSignature != 0xFEEF04BDU) {
            return false;
        }
        version = RuntimeVersion{
            static_cast<std::int32_t>(HIWORD(info->dwFileVersionMS)),
            static_cast<std::int32_t>(LOWORD(info->dwFileVersionMS)),
            static_cast<std::int32_t>(HIWORD(info->dwFileVersionLS)),
            static_cast<std::int32_t>(LOWORD(info->dwFileVersionLS)),
        };
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace

bool WindowsRuntimePlatform::QuerySkyrimModule(
    SkyrimModuleInfo& module) noexcept
{
    module = {};
    const HMODULE current = GetModuleHandleW(nullptr);
    const HMODULE named = GetModuleHandleW(L"SkyrimSE.exe");
    if (current == nullptr || named == nullptr || current != named) {
        return false;
    }

    try {
        std::vector<wchar_t> path_buffer(32'768U, L'\0');
        const DWORD length = GetModuleFileNameW(
            current,
            path_buffer.data(),
            static_cast<DWORD>(path_buffer.size()));
        if (length == 0U || length >= path_buffer.size() - 1U) {
            return false;
        }
        const std::wstring path{path_buffer.data(), length};
        const std::filesystem::path executable{path};
        if (!executable.is_absolute()
            || executable.filename().wstring() != L"SkyrimSE.exe") {
            return false;
        }

        MODULEINFO image{};
        if (GetModuleInformation(
                GetCurrentProcess(), current, &image, sizeof(image)) == FALSE
            || image.lpBaseOfDll == nullptr || image.SizeOfImage == 0U) {
            return false;
        }
        const auto image_base = reinterpret_cast<std::uintptr_t>(image.lpBaseOfDll);
        IMAGE_DOS_HEADER dos{};
        if (!ExactCurrentProcessRead(image_base, &dos, sizeof(dos))
            || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0) {
            return false;
        }
        const auto nt_offset = static_cast<std::uintptr_t>(dos.e_lfanew);
        if (nt_offset > image.SizeOfImage
            || sizeof(IMAGE_NT_HEADERS64) > image.SizeOfImage - nt_offset) {
            return false;
        }
        IMAGE_NT_HEADERS64 nt{};
        if (!ExactCurrentProcessRead(image_base + nt_offset, &nt, sizeof(nt))
            || nt.Signature != IMAGE_NT_SIGNATURE
            || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
            return false;
        }

        RuntimeVersion version;
        if (!ReadFileVersion(path, version)) {
            return false;
        }
        module.module_name = L"SkyrimSE.exe";
        module.executable_path = path;
        module.runtime_version = version;
        module.image_base = image_base;
        module.image_size = image.SizeOfImage;
        module.pe_machine = nt.FileHeader.Machine;
        return true;
    } catch (...) {
        module = {};
        return false;
    }
}

bool WindowsRuntimePlatform::ReadFile(
    const std::wstring_view absolute_path,
    std::vector<std::uint8_t>& bytes) noexcept
{
    bytes.clear();
    try {
        if (absolute_path.empty()
            || !std::filesystem::path{absolute_path}.is_absolute()
            || absolute_path.find(L'\0') != std::wstring_view::npos) {
            return false;
        }
        const std::wstring path{absolute_path};
        const FileHandle file{CreateFileW(
            path.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
            nullptr)};
        if (file.get() == INVALID_HANDLE_VALUE) {
            return false;
        }
        LARGE_INTEGER file_size{};
        if (GetFileSizeEx(file.get(), &file_size) == FALSE
            || file_size.QuadPart <= 0
            || static_cast<std::uint64_t>(file_size.QuadPart)
                > kMaximumAddressLibraryBytes) {
            return false;
        }
        bytes.resize(static_cast<std::size_t>(file_size.QuadPart));
        std::size_t position = 0;
        while (position < bytes.size()) {
            const std::size_t remaining = bytes.size() - position;
            const DWORD requested = static_cast<DWORD>((std::min)(
                remaining,
                static_cast<std::size_t>((std::numeric_limits<DWORD>::max)())));
            DWORD observed = 0;
            if (::ReadFile(
                    file.get(),
                    bytes.data() + position,
                    requested,
                    &observed,
                    nullptr) == FALSE
                || observed == 0U || observed > requested) {
                bytes.clear();
                return false;
            }
            position += observed;
        }
        return true;
    } catch (...) {
        bytes.clear();
        return false;
    }
}

bool WindowsRuntimePlatform::IsExecutableAddress(
    const std::uintptr_t address) const noexcept
{
    if (address == 0U) {
        return false;
    }
    MEMORY_BASIC_INFORMATION region{};
    if (VirtualQuery(
            reinterpret_cast<const void*>(address),
            &region,
            sizeof(region)) != sizeof(region)
        || region.State != MEM_COMMIT
        || !ExecutableProtection(region.Protect)) {
        return false;
    }
    const auto base = reinterpret_cast<std::uintptr_t>(region.BaseAddress);
    return address >= base && address - base < region.RegionSize;
}

std::uintptr_t WindowsRuntimePlatform::InvokeWorldRootCamera(
    const std::uintptr_t function_address) const noexcept
{
    if (!IsExecutableAddress(function_address)) {
        return 0U;
    }
    using WorldRootCamera = std::uintptr_t (*)();
    const auto function = reinterpret_cast<WorldRootCamera>(function_address);
    return function();
}

bool WindowsRuntimePlatform::Read(
    const std::uintptr_t address,
    const std::span<std::uint8_t> destination) const noexcept
{
    if (destination.empty()) {
        return false;
    }
    return ExactCurrentProcessRead(
        address, destination.data(), destination.size());
}

} // namespace truth::runtime::windows
