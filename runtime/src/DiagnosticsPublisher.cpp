#include <truth/runtime/DiagnosticsPublisher.hpp>

#include <array>
#include <bit>

namespace truth::runtime {

static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
static_assert(sizeof(PluginDiagnosticsV1) % sizeof(std::uint64_t) == 0U);

DiagnosticsPublisher::DiagnosticsPublisher() noexcept
{
    Publish(PluginDiagnosticsV1{});
}

void DiagnosticsPublisher::Publish(PluginDiagnosticsV1 diagnostics) noexcept
{
    diagnostics.size = sizeof(PluginDiagnosticsV1);
    diagnostics.abi_version = kPluginDiagnosticsAbiVersion;
    diagnostics.reserved32_0 = 0U;
    diagnostics.reserved32_1 = 0U;
    diagnostics.reserved64_0 = 0U;
    diagnostics.reserved64_1 = 0U;
    diagnostics.reserved64_2 = 0U;
    diagnostics.reserved64_3 = 0U;
    const auto words =
        std::bit_cast<std::array<std::uint64_t, kWordCount>>(diagnostics);

    sequence_.fetch_add(1U, std::memory_order_acq_rel);
    for (std::size_t index = 0; index < words.size(); ++index) {
        words_[index].store(words[index], std::memory_order_relaxed);
    }
    sequence_.fetch_add(1U, std::memory_order_release);
}

bool DiagnosticsPublisher::Read(PluginDiagnosticsV1& diagnostics) const noexcept
{
    constexpr std::size_t maximum_attempts = 64U;
    for (std::size_t attempt = 0; attempt < maximum_attempts; ++attempt) {
        const std::uint64_t before = sequence_.load(std::memory_order_acquire);
        if ((before & 1U) != 0U) {
            continue;
        }
        std::array<std::uint64_t, kWordCount> words{};
        for (std::size_t index = 0; index < words.size(); ++index) {
            words[index] = words_[index].load(std::memory_order_relaxed);
        }
        const std::uint64_t after = sequence_.load(std::memory_order_acquire);
        if (before == after && (after & 1U) == 0U) {
            diagnostics = std::bit_cast<PluginDiagnosticsV1>(words);
            return true;
        }
    }
    return false;
}

} // namespace truth::runtime
