#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

namespace {

using Microsoft::WRL::ComPtr;

constexpr std::uint32_t kRouteIdentity = 0U;
constexpr std::uint32_t kRouteSpatial = 1U;
constexpr std::uint32_t kRouteBridge = 2U;
constexpr std::uint32_t kRouteNative = 3U;

struct Float4 {
    float x;
    float y;
    float z;
    float w;
};

struct ProbeInput {
    Float4 native_color;
    Float4 bridge_color;
    Float4 spatial_color;
    Float4 identity_color;
    Float4 runtime_availability;
};

struct ProbeOutput {
    Float4 wrapper_color;
    Float4 direct_color;
    std::uint32_t route;
    float padding[3];
};

static_assert(sizeof(Float4) == 16U);
static_assert(sizeof(ProbeInput) == 80U);
static_assert(sizeof(ProbeOutput) == 48U);

int failures = 0;

void expect(const bool condition, const std::string_view message)
{
    if (!condition) {
        std::cerr << "expectation failed: " << message << '\n';
        ++failures;
    }
}

[[noreturn]] void fail(const std::string& message)
{
    std::cerr << "fatal: " << message << '\n';
    std::exit(2);
}

void check_hr(const HRESULT hr, const std::string_view operation)
{
    if (FAILED(hr)) {
        fail(std::string(operation) + " failed (hr=0x"
             + std::to_string(static_cast<unsigned long>(hr)) + ")");
    }
}

[[nodiscard]] std::string blob_text(const ComPtr<ID3DBlob>& blob)
{
    if (!blob || blob->GetBufferPointer() == nullptr) {
        return {};
    }
    const auto* data = static_cast<const char*>(blob->GetBufferPointer());
    return std::string(data, data + blob->GetBufferSize());
}

[[nodiscard]] ComPtr<ID3DBlob> compile_probe(
    const std::filesystem::path& probe_path,
    const bool declared_native_available)
{
    const D3D_SHADER_MACRO macros[] = {
        {"TRUTH_CAPABILITY_WARP_NATIVE_AVAILABLE",
         declared_native_available ? "1" : "0"},
        {nullptr, nullptr},
    };
    ComPtr<ID3DBlob> bytecode;
    ComPtr<ID3DBlob> diagnostics;
    constexpr UINT flags = D3DCOMPILE_ENABLE_STRICTNESS
        | D3DCOMPILE_WARNINGS_ARE_ERRORS
        | D3DCOMPILE_IEEE_STRICTNESS
        | D3DCOMPILE_OPTIMIZATION_LEVEL3;
    const HRESULT hr = D3DCompileFromFile(
        probe_path.c_str(), macros, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "TruthCapabilityWarpProbeMain", "cs_5_0", flags, 0U,
        &bytecode, &diagnostics);
    if (FAILED(hr)) {
        fail("strict Truth capability probe compilation failed:\n"
             + blob_text(diagnostics));
    }
    if (diagnostics && diagnostics->GetBufferSize() != 0U) {
        fail("strict Truth capability probe emitted diagnostics:\n"
             + blob_text(diagnostics));
    }
    return bytecode;
}

[[nodiscard]] ComPtr<ID3D11Device> create_warp_device(
    ComPtr<ID3D11DeviceContext>& context)
{
    constexpr std::array<D3D_FEATURE_LEVEL, 2> levels{
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    ComPtr<ID3D11Device> device;
    D3D_FEATURE_LEVEL selected{};
    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_SINGLETHREADED,
        levels.data(), static_cast<UINT>(levels.size()), D3D11_SDK_VERSION,
        &device, &selected, &context);
    if (hr == E_INVALIDARG) {
        hr = D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_SINGLETHREADED,
            levels.data() + 1, 1U, D3D11_SDK_VERSION,
            &device, &selected, &context);
    }
    check_hr(hr, "D3D11CreateDevice(WARP)");
    return device;
}

[[nodiscard]] std::vector<ProbeOutput> run_probe(
    const std::filesystem::path& probe_path,
    const bool declared_native_available,
    const std::vector<ProbeInput>& inputs)
{
    ComPtr<ID3D11DeviceContext> context;
    const ComPtr<ID3D11Device> device = create_warp_device(context);
    const ComPtr<ID3DBlob> bytecode = compile_probe(
        probe_path, declared_native_available);
    ComPtr<ID3D11ComputeShader> shader;
    check_hr(device->CreateComputeShader(
                 bytecode->GetBufferPointer(), bytecode->GetBufferSize(), nullptr, &shader),
             "CreateComputeShader");

    D3D11_BUFFER_DESC input_desc{};
    input_desc.ByteWidth = static_cast<UINT>(sizeof(ProbeInput) * inputs.size());
    input_desc.Usage = D3D11_USAGE_DEFAULT;
    input_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    input_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    input_desc.StructureByteStride = sizeof(ProbeInput);
    D3D11_SUBRESOURCE_DATA input_data{};
    input_data.pSysMem = inputs.data();
    ComPtr<ID3D11Buffer> input;
    check_hr(device->CreateBuffer(&input_desc, &input_data, &input), "CreateBuffer(input)");

    D3D11_SHADER_RESOURCE_VIEW_DESC input_view_desc{};
    input_view_desc.Format = DXGI_FORMAT_UNKNOWN;
    input_view_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    input_view_desc.Buffer.NumElements = static_cast<UINT>(inputs.size());
    ComPtr<ID3D11ShaderResourceView> input_view;
    check_hr(device->CreateShaderResourceView(input.Get(), &input_view_desc, &input_view),
             "CreateShaderResourceView(input)");

    D3D11_BUFFER_DESC output_desc{};
    output_desc.ByteWidth = static_cast<UINT>(sizeof(ProbeOutput) * inputs.size());
    output_desc.Usage = D3D11_USAGE_DEFAULT;
    output_desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    output_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    output_desc.StructureByteStride = sizeof(ProbeOutput);
    ComPtr<ID3D11Buffer> output;
    check_hr(device->CreateBuffer(&output_desc, nullptr, &output), "CreateBuffer(output)");

    D3D11_UNORDERED_ACCESS_VIEW_DESC output_view_desc{};
    output_view_desc.Format = DXGI_FORMAT_UNKNOWN;
    output_view_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    output_view_desc.Buffer.NumElements = static_cast<UINT>(inputs.size());
    ComPtr<ID3D11UnorderedAccessView> output_view;
    check_hr(device->CreateUnorderedAccessView(output.Get(), &output_view_desc, &output_view),
             "CreateUnorderedAccessView(output)");

    D3D11_BUFFER_DESC staging_desc{};
    staging_desc.ByteWidth = output_desc.ByteWidth;
    staging_desc.Usage = D3D11_USAGE_STAGING;
    staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Buffer> staging;
    check_hr(device->CreateBuffer(&staging_desc, nullptr, &staging), "CreateBuffer(staging)");

    ID3D11ShaderResourceView* input_pointer = input_view.Get();
    ID3D11UnorderedAccessView* output_pointer = output_view.Get();
    context->CSSetShader(shader.Get(), nullptr, 0U);
    context->CSSetShaderResources(0U, 1U, &input_pointer);
    context->CSSetUnorderedAccessViews(0U, 1U, &output_pointer, nullptr);
    context->Dispatch(static_cast<UINT>(inputs.size()), 1U, 1U);

    ID3D11ShaderResourceView* null_input = nullptr;
    ID3D11UnorderedAccessView* null_output = nullptr;
    context->CSSetShaderResources(0U, 1U, &null_input);
    context->CSSetUnorderedAccessViews(0U, 1U, &null_output, nullptr);
    context->CSSetShader(nullptr, nullptr, 0U);
    context->CopyResource(staging.Get(), output.Get());

    D3D11_MAPPED_SUBRESOURCE mapped{};
    check_hr(context->Map(staging.Get(), 0U, D3D11_MAP_READ, 0U, &mapped), "Map(staging)");
    std::vector<ProbeOutput> results(inputs.size());
    std::memcpy(results.data(), mapped.pData, sizeof(ProbeOutput) * results.size());
    context->Unmap(staging.Get(), 0U);
    return results;
}

[[nodiscard]] bool equal_color(const Float4 actual, const Float4 expected)
{
    constexpr float tolerance = 1.0e-6F;
    return std::fabs(actual.x - expected.x) <= tolerance
        && std::fabs(actual.y - expected.y) <= tolerance
        && std::fabs(actual.z - expected.z) <= tolerance
        && std::fabs(actual.w - expected.w) <= tolerance;
}

void expect_result(
    const ProbeOutput& actual,
    const std::uint32_t expected_route,
    const Float4 expected_color,
    const std::string_view label)
{
    expect(actual.route == expected_route, std::string(label) + " route");
    expect(equal_color(actual.direct_color, expected_color),
           std::string(label) + " direct RGBA");
    expect(equal_color(actual.wrapper_color, expected_color),
           std::string(label) + " wrapper RGBA");
}

}  // namespace

int main(const int argument_count, char** arguments)
{
    if (argument_count != 2) {
        fail("usage: truth_capability_warp_tests <probe.hlsl>");
    }

    const std::filesystem::path probe_path = arguments[1];
    if (!std::filesystem::is_regular_file(probe_path)) {
        fail("Truth capability probe source is absent");
    }

    const Float4 native{0.11F, 0.22F, 0.33F, 0.44F};
    const Float4 bridge{0.55F, 0.66F, 0.77F, 0.88F};
    const Float4 spatial{0.19F, 0.29F, 0.39F, 0.49F};
    const Float4 identity{0.91F, 0.81F, 0.71F, 0.61F};
    const auto input = [&](const Float4 availability) {
        return ProbeInput{native, bridge, spatial, identity, availability};
    };

    const std::vector<ProbeInput> ordered_inputs{
        input(Float4{1.0F, 1.0F, 1.0F, 1.0F}),
        input(Float4{0.0F, 1.0F, 1.0F, 1.0F}),
        input(Float4{0.0F, 0.0F, 1.0F, 1.0F}),
        input(Float4{0.0F, 0.0F, 0.0F, 1.0F}),
    };
    const std::vector<ProbeOutput> ordered_results = run_probe(
        probe_path, true, ordered_inputs);
    expect(ordered_results.size() == ordered_inputs.size(), "ordered result count");
    if (ordered_results.size() == ordered_inputs.size()) {
        expect_result(ordered_results[0], kRouteNative, native, "all available selects native");
        expect_result(ordered_results[1], kRouteBridge, bridge, "no native selects Bridge");
        expect_result(ordered_results[2], kRouteSpatial, spatial, "only spatial selects spatial");
        expect_result(ordered_results[3], kRouteIdentity, identity, "none selects identity");
    }

    const std::vector<ProbeOutput> unavailable_native_results = run_probe(
        probe_path, false,
        std::vector<ProbeInput>{input(Float4{1.0F, 1.0F, 1.0F, 1.0F})});
    expect(unavailable_native_results.size() == 1U, "unavailable native result count");
    if (unavailable_native_results.size() == 1U) {
        expect_result(unavailable_native_results[0], kRouteBridge, bridge,
                      "declared native unavailable selects Bridge");
    }

    if (failures != 0) {
        std::cerr << failures << " assertion(s) failed\n";
        return 1;
    }
    std::cout << "Truth capability WARP: native, Bridge, spatial, identity, and unavailable-native routes verified\n";
    return 0;
}
