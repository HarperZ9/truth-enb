#pragma once

#include <truth/runtime/PluginRuntime.hpp>

#include <array>
#include <atomic>
#include <cstdint>

namespace truth::runtime {

class DiagnosticsPublisher final {
public:
    DiagnosticsPublisher() noexcept;

    void Publish(PluginDiagnosticsV1 diagnostics) noexcept;
    [[nodiscard]] bool Read(PluginDiagnosticsV1& diagnostics) const noexcept;

private:
    static constexpr std::size_t kWordCount =
        sizeof(PluginDiagnosticsV1) / sizeof(std::uint64_t);

    alignas(64) std::atomic<std::uint64_t> sequence_{0};
    std::array<std::atomic<std::uint64_t>, kWordCount> words_{};
};

} // namespace truth::runtime
