#include <truth/runtime/PluginRuntime.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

class Module final {
public:
    explicit Module(HMODULE handle) noexcept:
        handle_(handle)
    {}

    ~Module() noexcept
    {
        if (handle_ != nullptr) {
            FreeLibrary(handle_);
        }
    }

    Module(const Module&) = delete;
    Module& operator=(const Module&) = delete;

    [[nodiscard]] HMODULE get() const noexcept { return handle_; }

private:
    HMODULE handle_{nullptr};
};

} // namespace

int wmain(const int argc, wchar_t** argv)
{
    if (argc != 2 || argv == nullptr || argv[1] == nullptr) {
        std::cerr << "expected one .dllplugin path\n";
        return 2;
    }
    const std::filesystem::path path{argv[1]};
    if (path.extension() != L".dllplugin") {
        std::cerr << "target did not use the .dllplugin extension\n";
        return 3;
    }
    const Module module{LoadLibraryW(path.c_str())};
    if (module.get() == nullptr) {
        std::cerr << "failed to load standalone plugin: " << GetLastError() << '\n';
        return 4;
    }
    const FARPROC raw = GetProcAddress(
        module.get(), "TruthEnbRuntimeGetDiagnosticsV1");
    if (raw == nullptr) {
        std::cerr << "diagnostics export missing\n";
        return 5;
    }
    using GetDiagnostics = BOOL(WINAPI*)(
        truth::runtime::PluginDiagnosticsV1*, std::uint32_t);
    const auto get_diagnostics = reinterpret_cast<GetDiagnostics>(raw);
    truth::runtime::PluginDiagnosticsV1 diagnostics;
    if (get_diagnostics(&diagnostics, sizeof(diagnostics) - 1U) != FALSE) {
        std::cerr << "undersized diagnostics buffer was admitted\n";
        return 6;
    }
    if (get_diagnostics(&diagnostics, sizeof(diagnostics)) == FALSE) {
        std::cerr << "diagnostics export rejected exact ABI size\n";
        return 7;
    }
    if (diagnostics.size != sizeof(diagnostics)
        || diagnostics.abi_version
            != truth::runtime::kPluginDiagnosticsAbiVersion) {
        std::cerr << "diagnostics ABI header was wrong\n";
        return 8;
    }
    if ((diagnostics.flags & truth::runtime::PluginDiagnosticFlags::HostResolved)
        != 0U) {
        std::cerr << "test process was misidentified as an ENB SDK host\n";
        return 9;
    }
    std::cout << "standalone .dllplugin load and diagnostics ABI passed\n";
    return 0;
}
