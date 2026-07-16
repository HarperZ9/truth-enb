#include "TestHarness.hpp"

#include <truth/runtime/DiagnosticsPublisher.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <thread>

namespace {

using namespace truth::runtime;
using truth::runtime::tests::Context;

[[nodiscard]] PluginDiagnosticsV1 Value(const std::uint64_t generation) noexcept
{
    PluginDiagnosticsV1 value;
    value.flags = static_cast<std::uint32_t>(generation & 0x1FU);
    value.callbacks_received = generation * 3U;
    value.generation = generation;
    value.parameter_get_failures = generation ^ 0x55AA55AA55AA55AAULL;
    value.parameter_set_failures = ~generation;
    value.relocation_id = generation + 36609U;
    value.relocation_address = generation * 17U;
    return value;
}

[[nodiscard]] bool Consistent(const PluginDiagnosticsV1& value) noexcept
{
    const std::uint64_t generation = value.generation;
    return value.size == sizeof(PluginDiagnosticsV1)
        && value.abi_version == kPluginDiagnosticsAbiVersion
        && value.callbacks_received == generation * 3U
        && value.parameter_get_failures
            == (generation ^ 0x55AA55AA55AA55AAULL)
        && value.parameter_set_failures == ~generation
        && value.relocation_id == generation + 36609U
        && value.relocation_address == generation * 17U
        && value.reserved32_0 == 0U
        && value.reserved32_1 == 0U
        && value.reserved64_0 == 0U
        && value.reserved64_1 == 0U
        && value.reserved64_2 == 0U
        && value.reserved64_3 == 0U;
}

void InitialAndPublishedSnapshotsAreComplete(Context& context)
{
    DiagnosticsPublisher publisher;
    PluginDiagnosticsV1 observed;
    context.expect(publisher.Read(observed), "initial diagnostics read failed");
    context.expect(observed.size == sizeof(PluginDiagnosticsV1)
            && observed.abi_version == kPluginDiagnosticsAbiVersion,
        "initial diagnostics ABI header was wrong");

    publisher.Publish(Value(42U));
    context.expect(publisher.Read(observed), "published diagnostics read failed");
    context.expect(Consistent(observed) && observed.generation == 42U,
        "published diagnostics snapshot was torn");
}

void ConcurrentPublicationNeverExposesATornSnapshot(Context& context)
{
    DiagnosticsPublisher publisher;
    publisher.Publish(Value(1U));
    std::atomic<bool> start{false};
    std::atomic<bool> done{false};
    std::atomic<bool> torn{false};
    std::thread writer([&]() {
        while (!start.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        for (std::uint64_t generation = 2U; generation < 50'000U; ++generation) {
            publisher.Publish(Value(generation));
        }
        done.store(true, std::memory_order_release);
    });
    start.store(true, std::memory_order_release);
    do {
        PluginDiagnosticsV1 observed;
        if (publisher.Read(observed) && !Consistent(observed)) {
            torn.store(true, std::memory_order_relaxed);
            break;
        }
    } while (!done.load(std::memory_order_acquire));
    writer.join();
    context.expect(!torn.load(std::memory_order_relaxed),
        "concurrent diagnostics read observed mixed generations");
}

constexpr truth::runtime::tests::TestCase kTests[]{
    {"initial and published snapshots are complete",
        &InitialAndPublishedSnapshotsAreComplete},
    {"concurrent publication never exposes a torn snapshot",
        &ConcurrentPublicationNeverExposesATornSnapshot},
};

} // namespace

int main()
{
    return truth::runtime::tests::Run(
        "Truth diagnostics publisher cases", kTests, std::size(kTests));
}
