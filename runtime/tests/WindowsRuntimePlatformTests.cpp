#include "TestHarness.hpp"

#include <truth/runtime/windows/WindowsRuntimePlatform.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace {

using truth::runtime::SkyrimModuleInfo;
using truth::runtime::tests::Context;
using truth::runtime::windows::WindowsRuntimePlatform;

std::uint32_t data_marker = 0xA1B2C3D4U;

std::uintptr_t CameraLocatorFixture()
{
    return 0x123456789ABCDEF0ULL;
}

void ProcessMemoryReadsAreBoundedAndNonThrowing(Context& context)
{
    WindowsRuntimePlatform platform;
    std::array<std::uint8_t, sizeof(data_marker)> bytes{};
    context.expect(platform.Read(
            reinterpret_cast<std::uintptr_t>(&data_marker), bytes),
        "current-process readable memory was rejected");
    std::uint32_t observed = 0;
    std::memcpy(&observed, bytes.data(), sizeof(observed));
    context.expect(observed == data_marker, "process memory bytes were wrong");
    context.expect(!platform.Read(0U, bytes), "null memory address was admitted");
    context.expect(!platform.Read(
            (std::numeric_limits<std::uintptr_t>::max)() - 1U, bytes),
        "overflowing memory range was admitted");
}

void ExecutableAdmissionAndLocatorInvocationAreExact(Context& context)
{
    WindowsRuntimePlatform platform;
    const auto function_address =
        reinterpret_cast<std::uintptr_t>(&CameraLocatorFixture);
    context.expect(platform.IsExecutableAddress(function_address),
        "known executable address was rejected");
    context.expect(!platform.IsExecutableAddress(
            reinterpret_cast<std::uintptr_t>(&data_marker)),
        "writable data was admitted as executable code");
    context.expect(platform.InvokeWorldRootCamera(function_address)
            == 0x123456789ABCDEF0ULL,
        "validated no-argument camera locator ABI was invoked incorrectly");
    context.expect(platform.InvokeWorldRootCamera(0U) == 0U,
        "null camera locator was invoked");
}

void NonSkyrimTestProcessIsNeverMisidentified(Context& context)
{
    WindowsRuntimePlatform platform;
    SkyrimModuleInfo module;
    context.expect(!platform.QuerySkyrimModule(module),
        "test executable was misidentified as SkyrimSE.exe");
}

constexpr truth::runtime::tests::TestCase kTests[]{
    {"process memory reads are bounded and non-throwing",
        &ProcessMemoryReadsAreBoundedAndNonThrowing},
    {"executable admission and locator invocation are exact",
        &ExecutableAdmissionAndLocatorInvocationAreExact},
    {"non-Skyrim test process is never misidentified",
        &NonSkyrimTestProcessIsNeverMisidentified},
};

} // namespace

int main()
{
    return truth::runtime::tests::Run(
        "Truth Windows runtime platform cases", kTests, std::size(kTests));
}
