// PostFinishWarpTests.cpp — rendered contracts for TruthPostFinish.fxh
//
// TruthFinishLdr is called from enbeffectpostpass.fx, so the reference sky pass
// never reaches it. These cases run the real HLSL on WARP through
// TruthPostFinishWarpProbe.hlsl.
//
// The contract is that vignette strength is controllable independently of the
// stage gate. Setting TruthPostpassIntensity to zero does remove the vignette,
// but the early return takes TruthTriangularDither with it, and an undithered
// grade bands. A host that owns its own vignette needs the vignette gone and
// the dither kept, which is what case 3 pins.
//
// No golden images. Every case is a relation between two probe points or
// between two outputs of the same probe, so tuning that leaves the contract
// intact cannot drift these.

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

namespace {

using Microsoft::WRL::ComPtr;

struct Float4 {
    float x{};
    float y{};
    float z{};
    float w{};
};

struct ProbeInput {
    Float4 texcoord;
    Float4 display_color;
};

struct ProbeOutput {
    Float4 finished;
    Float4 dither_only;
};

// Mirrors cbuffer TruthPostFinishProbeParams in the probe. 32 bytes.
struct ProbeParams {
    Float4 screen_size{1920.0F, 1080.0F, 0.0F, 0.0F};
    float postpass_intensity{1.0F};
    float vignette_strength{0.18F};
    float grain_shape{0.0F};
    float pad{};
};
static_assert(sizeof(ProbeParams) % 16U == 0U,
              "constant buffer payloads must be 16-byte aligned");

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
    if (!blob || blob->GetBufferSize() == 0U) {
        return {};
    }
    const auto* data = static_cast<const char*>(blob->GetBufferPointer());
    return std::string(data, data + blob->GetBufferSize());
}

[[nodiscard]] ComPtr<ID3DBlob> compile_probe(const std::filesystem::path& probe_path)
{
    ComPtr<ID3DBlob> bytecode;
    ComPtr<ID3DBlob> diagnostics;
    constexpr UINT flags = D3DCOMPILE_ENABLE_STRICTNESS
        | D3DCOMPILE_WARNINGS_ARE_ERRORS
        | D3DCOMPILE_IEEE_STRICTNESS
        | D3DCOMPILE_OPTIMIZATION_LEVEL3;
    const HRESULT hr = D3DCompileFromFile(
        probe_path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "TruthPostFinishWarpProbeMain", "cs_5_0", flags, 0U,
        &bytecode, &diagnostics);
    if (FAILED(hr)) {
        fail("strict Truth post-finish probe compilation failed:\n"
             + blob_text(diagnostics));
    }
    if (diagnostics && diagnostics->GetBufferSize() != 0U) {
        fail("strict Truth post-finish probe emitted diagnostics:\n"
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
    const ProbeParams& params,
    const std::vector<ProbeInput>& inputs)
{
    ComPtr<ID3D11DeviceContext> context;
    const ComPtr<ID3D11Device> device = create_warp_device(context);
    const ComPtr<ID3DBlob> bytecode = compile_probe(probe_path);
    ComPtr<ID3D11ComputeShader> shader;
    check_hr(device->CreateComputeShader(
                 bytecode->GetBufferPointer(), bytecode->GetBufferSize(),
                 nullptr, &shader),
             "CreateComputeShader");

    D3D11_BUFFER_DESC params_desc{};
    params_desc.ByteWidth = static_cast<UINT>(sizeof(ProbeParams));
    params_desc.Usage = D3D11_USAGE_IMMUTABLE;
    params_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    D3D11_SUBRESOURCE_DATA params_data{};
    params_data.pSysMem = &params;
    ComPtr<ID3D11Buffer> params_buffer;
    check_hr(device->CreateBuffer(&params_desc, &params_data, &params_buffer),
             "CreateBuffer(params)");

    D3D11_BUFFER_DESC input_desc{};
    input_desc.ByteWidth = static_cast<UINT>(sizeof(ProbeInput) * inputs.size());
    input_desc.Usage = D3D11_USAGE_DEFAULT;
    input_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    input_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    input_desc.StructureByteStride = sizeof(ProbeInput);
    D3D11_SUBRESOURCE_DATA input_data{};
    input_data.pSysMem = inputs.data();
    ComPtr<ID3D11Buffer> input;
    check_hr(device->CreateBuffer(&input_desc, &input_data, &input),
             "CreateBuffer(input)");

    D3D11_SHADER_RESOURCE_VIEW_DESC input_view_desc{};
    input_view_desc.Format = DXGI_FORMAT_UNKNOWN;
    input_view_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    input_view_desc.Buffer.NumElements = static_cast<UINT>(inputs.size());
    ComPtr<ID3D11ShaderResourceView> input_view;
    check_hr(device->CreateShaderResourceView(
                 input.Get(), &input_view_desc, &input_view),
             "CreateShaderResourceView(input)");

    D3D11_BUFFER_DESC output_desc{};
    output_desc.ByteWidth = static_cast<UINT>(sizeof(ProbeOutput) * inputs.size());
    output_desc.Usage = D3D11_USAGE_DEFAULT;
    output_desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    output_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    output_desc.StructureByteStride = sizeof(ProbeOutput);
    ComPtr<ID3D11Buffer> output;
    check_hr(device->CreateBuffer(&output_desc, nullptr, &output),
             "CreateBuffer(output)");

    D3D11_UNORDERED_ACCESS_VIEW_DESC output_view_desc{};
    output_view_desc.Format = DXGI_FORMAT_UNKNOWN;
    output_view_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    output_view_desc.Buffer.NumElements = static_cast<UINT>(inputs.size());
    ComPtr<ID3D11UnorderedAccessView> output_view;
    check_hr(device->CreateUnorderedAccessView(
                 output.Get(), &output_view_desc, &output_view),
             "CreateUnorderedAccessView(output)");

    D3D11_BUFFER_DESC staging_desc{};
    staging_desc.ByteWidth = output_desc.ByteWidth;
    staging_desc.Usage = D3D11_USAGE_STAGING;
    staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Buffer> staging;
    check_hr(device->CreateBuffer(&staging_desc, nullptr, &staging),
             "CreateBuffer(staging)");

    ID3D11ShaderResourceView* input_pointer = input_view.Get();
    ID3D11UnorderedAccessView* output_pointer = output_view.Get();
    ID3D11Buffer* params_pointer = params_buffer.Get();

    context->CSSetShader(shader.Get(), nullptr, 0U);
    context->CSSetConstantBuffers(0U, 1U, &params_pointer);
    context->CSSetShaderResources(0U, 1U, &input_pointer);
    context->CSSetUnorderedAccessViews(0U, 1U, &output_pointer, nullptr);
    context->Dispatch(static_cast<UINT>(inputs.size()), 1U, 1U);

    ID3D11ShaderResourceView* null_input = nullptr;
    ID3D11UnorderedAccessView* null_output = nullptr;
    ID3D11Buffer* null_params = nullptr;
    context->CSSetShaderResources(0U, 1U, &null_input);
    context->CSSetUnorderedAccessViews(0U, 1U, &null_output, nullptr);
    context->CSSetConstantBuffers(0U, 1U, &null_params);
    context->CSSetShader(nullptr, nullptr, 0U);
    context->CopyResource(staging.Get(), output.Get());

    D3D11_MAPPED_SUBRESOURCE mapped{};
    check_hr(context->Map(staging.Get(), 0U, D3D11_MAP_READ, 0U, &mapped),
             "Map(staging)");
    std::vector<ProbeOutput> results(inputs.size());
    std::memcpy(results.data(), mapped.pData, output_desc.ByteWidth);
    context->Unmap(staging.Get(), 0U);
    return results;
}

// The centre has no vignette by construction (dot(centered, centered) is zero);
// the corner has the most. Grey at 0.5 keeps every result away from the
// saturate clamps at both ends.
constexpr std::size_t kCentre = 0U;
constexpr std::size_t kCorner = 1U;

[[nodiscard]] std::vector<ProbeInput> probe_points()
{
    return {
        ProbeInput{{0.5F, 0.5F, 0.0F, 0.0F}, {0.5F, 0.5F, 0.5F, 0.0F}},
        ProbeInput{{0.0F, 0.0F, 0.0F, 0.0F}, {0.5F, 0.5F, 0.5F, 0.0F}},
    };
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "usage: truth_post_finish_warp_tests <probe.hlsl>\n";
        return 2;
    }
    const std::filesystem::path probe_path{argv[1]};
    const auto inputs = probe_points();

    // Dither is bounded by one 255th in each direction, so two dithered values
    // differ by at most two 255ths. The vignette signal at the corner is an
    // order of magnitude larger than that.
    constexpr float kDitherBound = 2.0F / 255.0F;
    constexpr float kSignalFloor = 0.01F;

    // Case 1: at the shipped default the vignette is present and darkens the
    // corner. This is the regression lock; it must hold before and after.
    {
        ProbeParams params{};
        params.vignette_strength = 0.18F;
        const auto out = run_probe(probe_path, params, inputs);
        const float centre = out[kCentre].finished.x;
        const float corner = out[kCorner].finished.x;
        expect(corner < centre - kSignalFloor,
               "at the default strength the corner must be darker than the centre; "
               "corner=" + std::to_string(corner) + " centre=" + std::to_string(centre));
    }

    // Case 2: zeroing the strength removes the vignette. The corner and the
    // centre then differ only by dither.
    {
        ProbeParams params{};
        params.vignette_strength = 0.0F;
        const auto out = run_probe(probe_path, params, inputs);
        const float centre = out[kCentre].finished.x;
        const float corner = out[kCorner].finished.x;
        expect(std::fabs(corner - centre) <= kDitherBound,
               "at zero strength the corner and centre must differ only by dither; "
               "corner=" + std::to_string(corner) + " centre=" + std::to_string(centre));
    }

    // Case 3: with strength and grain both zero, the stage collapses to dither
    // alone. This is the contract the Effects 11 variant depends on. Zeroing
    // TruthPostpassIntensity would also remove the vignette but would return
    // before the dither, which is exactly what this forbids.
    {
        ProbeParams params{};
        params.vignette_strength = 0.0F;
        params.grain_shape = 0.0F;
        const auto out = run_probe(probe_path, params, inputs);
        for (std::size_t index = 0U; index < out.size(); ++index) {
            const float finished = out[index].finished.x;
            const float dither_only = out[index].dither_only.x;
            expect(finished == dither_only,
                   "with strength and grain at zero the stage must equal dither "
                   "alone at probe point " + std::to_string(index)
                   + "; finished=" + std::to_string(finished)
                   + " dither_only=" + std::to_string(dither_only));
        }
        expect(out[kCorner].dither_only.x != 0.5F || out[kCentre].dither_only.x != 0.5F,
               "dither must perturb at least one probe point, otherwise case 3 "
               "would pass against a stage that silently returned its input");
    }

    if (failures == 0) {
        std::cout << "truth_post_finish_warp_tests: all cases passed\n";
    }
    return failures == 0 ? 0 : 1;
}
