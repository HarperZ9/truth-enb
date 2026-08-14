// ScreenSpaceWarpTests.cpp — rendered safety contracts for TruthScreenSpace.fxh
//
// Task 3, Step 1. truth_scene_contracts is a source-text checker: it greps the
// shader sources for required and forbidden tokens. That proves structure, not
// behaviour. These cases run the real HLSL on WARP and assert the safety
// contracts against actual output.
//
// Every case here is an identity contract. An effect that should not fire must
// return the scene it was handed, so none of them needs a golden image and none
// of them can drift with a tuning change that leaves the guards intact.

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

constexpr std::uint32_t kTextureExtent = 8U;
constexpr std::uint32_t kTexelCount = kTextureExtent * kTextureExtent;

struct Float4 {
    float x{};
    float y{};
    float z{};
    float w{};
};

struct ProbeInput {
    Float4 scene;
    Float4 texel_and_depth;
    Float4 view_direction;
    Float4 validity;
    Float4 ao;
    Float4 ssr;
    Float4 diffusion;
    Float4 texcoord;
};

struct ProbeOutput {
    Float4 scene_in;
    Float4 after_ao;
    Float4 after_ssr;
    Float4 after_diffusion;
    Float4 after_all;
    std::uint32_t geometry_valid{};
    std::uint32_t quality_tier{};
    std::uint32_t ao_directions{};
    std::uint32_t ssr_steps{};
};

// Texture content for one case. Depth and mask are single-channel; scene and
// normal are four-channel.
struct SceneTextures {
    std::array<Float4, kTexelCount> scene{};
    std::array<float, kTexelCount> depth{};
    std::array<Float4, kTexelCount> normal{};
    std::array<float, kTexelCount> mask{};
};

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
    const std::filesystem::path& probe_path, const std::string& tier)
{
    const D3D_SHADER_MACRO macros[] = {
        {"TRUTH_QUALITY_TIER", tier.c_str()},
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
        "TruthScreenSpaceWarpProbeMain", "cs_5_0", flags, 0U,
        &bytecode, &diagnostics);
    if (FAILED(hr)) {
        fail("strict Truth screen-space probe compilation failed (tier " + tier
             + "):\n" + blob_text(diagnostics));
    }
    if (diagnostics && diagnostics->GetBufferSize() != 0U) {
        fail("strict Truth screen-space probe emitted diagnostics (tier " + tier
             + "):\n" + blob_text(diagnostics));
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

[[nodiscard]] ComPtr<ID3D11ShaderResourceView> create_texture_view(
    const ComPtr<ID3D11Device>& device,
    const DXGI_FORMAT format,
    const void* pixels,
    const UINT pixel_stride,
    const std::string_view label)
{
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = kTextureExtent;
    desc.Height = kTextureExtent;
    desc.MipLevels = 1U;
    desc.ArraySize = 1U;
    desc.Format = format;
    desc.SampleDesc.Count = 1U;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA data{};
    data.pSysMem = pixels;
    data.SysMemPitch = kTextureExtent * pixel_stride;

    ComPtr<ID3D11Texture2D> texture;
    check_hr(device->CreateTexture2D(&desc, &data, &texture),
             std::string("CreateTexture2D(") + std::string(label) + ")");

    D3D11_SHADER_RESOURCE_VIEW_DESC view_desc{};
    view_desc.Format = format;
    view_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    view_desc.Texture2D.MipLevels = 1U;
    ComPtr<ID3D11ShaderResourceView> view;
    check_hr(device->CreateShaderResourceView(texture.Get(), &view_desc, &view),
             std::string("CreateShaderResourceView(") + std::string(label) + ")");
    return view;
}

[[nodiscard]] std::vector<ProbeOutput> run_probe(
    const std::filesystem::path& probe_path,
    const std::string& tier,
    const SceneTextures& textures,
    const std::vector<ProbeInput>& inputs)
{
    ComPtr<ID3D11DeviceContext> context;
    const ComPtr<ID3D11Device> device = create_warp_device(context);
    const ComPtr<ID3DBlob> bytecode = compile_probe(probe_path, tier);
    ComPtr<ID3D11ComputeShader> shader;
    check_hr(device->CreateComputeShader(
                 bytecode->GetBufferPointer(), bytecode->GetBufferSize(),
                 nullptr, &shader),
             "CreateComputeShader");

    const ComPtr<ID3D11ShaderResourceView> scene_view = create_texture_view(
        device, DXGI_FORMAT_R32G32B32A32_FLOAT, textures.scene.data(),
        static_cast<UINT>(sizeof(Float4)), "scene");
    const ComPtr<ID3D11ShaderResourceView> depth_view = create_texture_view(
        device, DXGI_FORMAT_R32_FLOAT, textures.depth.data(),
        static_cast<UINT>(sizeof(float)), "depth");
    const ComPtr<ID3D11ShaderResourceView> normal_view = create_texture_view(
        device, DXGI_FORMAT_R32G32B32A32_FLOAT, textures.normal.data(),
        static_cast<UINT>(sizeof(Float4)), "normal");
    const ComPtr<ID3D11ShaderResourceView> mask_view = create_texture_view(
        device, DXGI_FORMAT_R32_FLOAT, textures.mask.data(),
        static_cast<UINT>(sizeof(float)), "mask");

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

    D3D11_SAMPLER_DESC sampler_desc{};
    sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
    ComPtr<ID3D11SamplerState> sampler;
    check_hr(device->CreateSamplerState(&sampler_desc, &sampler),
             "CreateSamplerState");

    ID3D11ShaderResourceView* resources[] = {
        scene_view.Get(), depth_view.Get(), normal_view.Get(),
        mask_view.Get(), input_view.Get()};
    ID3D11UnorderedAccessView* output_pointer = output_view.Get();
    ID3D11SamplerState* sampler_pointer = sampler.Get();

    context->CSSetShader(shader.Get(), nullptr, 0U);
    context->CSSetShaderResources(0U, 5U, resources);
    context->CSSetSamplers(0U, 1U, &sampler_pointer);
    context->CSSetUnorderedAccessViews(0U, 1U, &output_pointer, nullptr);
    context->Dispatch(static_cast<UINT>(inputs.size()), 1U, 1U);

    ID3D11ShaderResourceView* null_resources[5] = {};
    ID3D11UnorderedAccessView* null_output = nullptr;
    ID3D11SamplerState* null_sampler = nullptr;
    context->CSSetShaderResources(0U, 5U, null_resources);
    context->CSSetSamplers(0U, 1U, &null_sampler);
    context->CSSetUnorderedAccessViews(0U, 1U, &null_output, nullptr);
    context->CSSetShader(nullptr, nullptr, 0U);
    context->CopyResource(staging.Get(), output.Get());

    D3D11_MAPPED_SUBRESOURCE mapped{};
    check_hr(context->Map(staging.Get(), 0U, D3D11_MAP_READ, 0U, &mapped),
             "Map(staging)");
    std::vector<ProbeOutput> results(inputs.size());
    std::memcpy(results.data(), mapped.pData, sizeof(ProbeOutput) * results.size());
    context->Unmap(staging.Get(), 0U);
    return results;
}

[[nodiscard]] bool identical_rgb(const Float4 actual, const Float4 expected)
{
    return actual.x == expected.x && actual.y == expected.y && actual.z == expected.z;
}

[[nodiscard]] bool near_rgb(
    const Float4 actual, const Float4 expected, const float tolerance)
{
    return std::fabs(actual.x - expected.x) <= tolerance
        && std::fabs(actual.y - expected.y) <= tolerance
        && std::fabs(actual.z - expected.z) <= tolerance;
}

[[nodiscard]] std::string describe(const Float4 value)
{
    return "(" + std::to_string(value.x) + ", " + std::to_string(value.y)
        + ", " + std::to_string(value.z) + ")";
}

// ── Scene builders ───────────────────────────────────────────────────────

constexpr Float4 kSceneColor{0.35F, 0.52F, 0.71F, 1.0F};
constexpr float kFlatDepth = 0.5F;
constexpr float kSkyThreshold = 0.999F;

// The scene texture is deliberately high-contrast rather than uniform. A flat
// colour field cannot distinguish "the effect was skipped" from "the effect ran
// and averaged identical neighbours", which makes an identity assertion
// vacuous. Every texel that is not the probe centre is driven far away from the
// centre colour, so any gather that should not have happened moves the result.
[[nodiscard]] SceneTextures make_flat_scene(const float mask_value)
{
    SceneTextures textures{};
    for (std::uint32_t y = 0U; y < kTextureExtent; ++y) {
        for (std::uint32_t x = 0U; x < kTextureExtent; ++x) {
            const std::uint32_t index = y * kTextureExtent + x;
            const bool is_centre = (x == kTextureExtent / 2U)
                && (y == kTextureExtent / 2U);
            textures.scene[index] = is_centre
                ? kSceneColor
                : Float4{1.0F - kSceneColor.x, 1.0F - kSceneColor.y,
                         1.0F - kSceneColor.z, 1.0F};
            textures.depth[index] = kFlatDepth;
            textures.normal[index] = Float4{0.0F, 0.0F, 1.0F, 1.0F};
            textures.mask[index] = mask_value;
        }
    }
    return textures;
}

// A hard depth discontinuity down the middle. The probe centre sits far back at
// kEdgeCentreDepth and the left half sits sharply nearer, so those texels would
// register as occluding geometry if they were accepted. They belong to a
// different surface, and the bilateral weight has to reject them.
//
// The near side must be in FRONT of the centre for this case to test anything:
// geometry behind the centre never occludes it, so an edge placed further away
// would pass no matter what the weight did.
constexpr float kEdgeCentreDepth = 0.90F;
constexpr float kEdgeNearDepth = 0.50F;

[[nodiscard]] SceneTextures make_depth_edge_scene()
{
    SceneTextures textures = make_flat_scene(0.0F);
    for (std::uint32_t y = 0U; y < kTextureExtent; ++y) {
        for (std::uint32_t x = 0U; x < kTextureExtent; ++x) {
            const std::uint32_t index = y * kTextureExtent + x;
            textures.depth[index] = (x < kTextureExtent / 2U)
                ? kEdgeNearDepth
                : kEdgeCentreDepth;
        }
    }
    return textures;
}

[[nodiscard]] ProbeInput make_input()
{
    ProbeInput input{};
    input.scene = kSceneColor;
    input.texel_and_depth = Float4{
        1.0F / static_cast<float>(kTextureExtent),
        1.0F / static_cast<float>(kTextureExtent),
        kFlatDepth,
        kSkyThreshold};
    input.view_direction = Float4{0.0F, 0.0F, 1.0F, 0.0F};
    input.validity = Float4{1.0F, 1.0F, 0.0F, 0.0F};
    input.ao = Float4{1.0F, 1.0F, 8.0F, 0.0F};
    input.ssr = Float4{1.0F, 4.0F, 0.5F, 0.0F};
    input.diffusion = Float4{1.0F, 4.0F, 0.0F, 0.0F};
    input.texcoord = Float4{0.5F, 0.5F, 0.0F, 0.0F};
    return input;
}

// ── Cases ────────────────────────────────────────────────────────────────

void case_ao_flat_surface_is_neutral(const std::filesystem::path& probe)
{
    const SceneTextures textures = make_flat_scene(0.0F);
    const std::vector<ProbeInput> inputs{make_input()};
    const std::vector<ProbeOutput> results = run_probe(probe, "2", textures, inputs);

    const ProbeOutput& out = results.front();
    expect(out.geometry_valid == 1U,
        "ao-flat-surface-is-neutral: geometry should be valid at depth 0.5");
    expect(near_rgb(out.after_ao, out.scene_in, 1.0e-3F),
        "ao-flat-surface-is-neutral: an unoccluded flat surface must not darken; got "
            + describe(out.after_ao) + " from " + describe(out.scene_in));
}

void case_ao_depth_edge_is_rejected(const std::filesystem::path& probe)
{
    const SceneTextures textures = make_depth_edge_scene();
    ProbeInput input = make_input();
    input.texel_and_depth.z = kEdgeCentreDepth;
    const std::vector<ProbeInput> inputs{input};
    const std::vector<ProbeOutput> results = run_probe(probe, "2", textures, inputs);

    const ProbeOutput& out = results.front();
    expect(out.geometry_valid == 1U,
        "ao-depth-edge-is-rejected: geometry should be valid at the centre depth");
    expect(near_rgb(out.after_ao, out.scene_in, 1.0e-3F),
        "ao-depth-edge-is-rejected: samples across a depth discontinuity must be "
        "rejected, not counted as occlusion; got "
            + describe(out.after_ao) + " from " + describe(out.scene_in));
}

void case_ssr_miss_preserves_scene(const std::filesystem::path& probe)
{
    // A flat surface facing the viewer reflects away from all recorded geometry,
    // so the march terminates without a hit.
    const SceneTextures textures = make_flat_scene(0.0F);
    const std::vector<ProbeInput> inputs{make_input()};
    const std::vector<ProbeOutput> results = run_probe(probe, "2", textures, inputs);

    const ProbeOutput& out = results.front();
    expect(out.ssr_steps > 0U,
        "ssr-miss-preserves-scene: tier 2 must have a live SSR march, otherwise "
        "this case proves nothing");
    expect(near_rgb(out.after_ssr, out.scene_in, 1.0e-3F),
        "ssr-miss-preserves-scene: a miss must return the scene unchanged; got "
            + describe(out.after_ssr) + " from " + describe(out.scene_in));
}

void case_sss_non_skin_preserves_scene(const std::filesystem::path& probe)
{
    // Mask is zero everywhere: nothing here is skin.
    const SceneTextures textures = make_flat_scene(0.0F);
    const std::vector<ProbeInput> inputs{make_input()};
    const std::vector<ProbeOutput> results = run_probe(probe, "2", textures, inputs);

    const ProbeOutput& out = results.front();
    expect(identical_rgb(out.after_diffusion, out.scene_in),
        "sss-non-skin-preserves-scene: a zero skin mask must return the scene "
        "bit for bit; got " + describe(out.after_diffusion) + " from "
            + describe(out.scene_in));
}

// Positive control. Every other case in this file asserts that an effect did
// NOT change the scene, and all of them would also pass if the probe were
// mis-wired: unbound textures, a shader that never runs, a harness that reads
// back zeros. This case proves the opposite direction is observable, so the
// identity assertions mean something.
//
// It is also why the non-skin case cannot be verified by mutation. Diffusion
// declines a zero mask at four independent points: the early return, the
// per-sample mask test, the accumulated-weight floor, and the final blend
// factor. No single one of them is load-bearing, so no single mutation changes
// the result. Robust code, but it makes a positive control the only real check.
void case_sss_skin_does_diffuse(const std::filesystem::path& probe)
{
    const SceneTextures textures = make_flat_scene(1.0F);  // everything is skin
    const std::vector<ProbeInput> inputs{make_input()};
    const std::vector<ProbeOutput> results = run_probe(probe, "2", textures, inputs);

    const ProbeOutput& out = results.front();
    expect(!near_rgb(out.after_diffusion, out.scene_in, 1.0e-3F),
        "sss-skin-does-diffuse: a fully valid skin mask over contrasting "
        "neighbours must move the centre; got " + describe(out.after_diffusion)
            + " from " + describe(out.scene_in)
            + " (if this matches, the probe is not exercising the shader)");
}

// SSR is compiled out below tier 2. Proving that keeps the tier contract
// honest: the analytic tiers must not carry a march at all.
void case_ssr_absent_below_tier_two(const std::filesystem::path& probe)
{
    const SceneTextures textures = make_flat_scene(0.0F);
    const std::vector<ProbeInput> inputs{make_input()};

    for (const char* tier : {"0", "1"}) {
        const std::vector<ProbeOutput> results = run_probe(probe, tier, textures, inputs);
        const ProbeOutput& out = results.front();
        expect(out.ssr_steps == 0U,
            std::string("ssr-absent-below-tier-two: tier ") + tier
                + " must declare zero SSR steps");
        expect(identical_rgb(out.after_ssr, out.scene_in),
            std::string("ssr-absent-below-tier-two: tier ") + tier
                + " must return the scene bit for bit; got "
                + describe(out.after_ssr) + " from " + describe(out.scene_in));
    }
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "usage: truth_screen_space_warp_tests <probe.hlsl>\n";
        return 2;
    }
    const std::filesystem::path probe(argv[1]);
    if (!std::filesystem::exists(probe)) {
        std::cerr << "probe shader is absent: " << probe.string() << '\n';
        return 2;
    }

    case_ao_flat_surface_is_neutral(probe);
    case_ao_depth_edge_is_rejected(probe);
    case_ssr_miss_preserves_scene(probe);
    case_sss_non_skin_preserves_scene(probe);
    case_sss_skin_does_diffuse(probe);
    case_ssr_absent_below_tier_two(probe);

    if (failures != 0) {
        std::cerr << failures << " screen-space contract(s) failed\n";
        return 1;
    }
    std::cout << "PASS: Truth screen-space safety contracts\n";
    return 0;
}
