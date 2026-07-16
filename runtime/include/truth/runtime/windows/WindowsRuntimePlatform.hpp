#pragma once

#include <truth/runtime/NativeCameraProvider.hpp>

namespace truth::runtime::windows {

class WindowsRuntimePlatform final : public NativeRuntimePlatform {
public:
    [[nodiscard]] bool QuerySkyrimModule(
        SkyrimModuleInfo& module) noexcept override;
    [[nodiscard]] bool ReadFile(
        std::wstring_view absolute_path,
        std::vector<std::uint8_t>& bytes) noexcept override;
    [[nodiscard]] bool IsExecutableAddress(
        std::uintptr_t address) const noexcept override;
    [[nodiscard]] std::uintptr_t InvokeWorldRootCamera(
        std::uintptr_t function_address) const noexcept override;
    [[nodiscard]] bool Read(
        std::uintptr_t address,
        std::span<std::uint8_t> destination) const noexcept override;
};

} // namespace truth::runtime::windows
