#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include "truth/render/MasterLook.hpp"
#include "truth/render/SkyViewAdapter.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using Microsoft::WRL::ComPtr;
using truth::render::FilmicToneCurve;
using truth::render::EvaluateSkyViewAdapter;
using truth::render::SkyViewAdapterInput;
using truth::render::SkyViewAdapterOutput;
using truth::render::SkyViewAdapterStatus;
using truth::render::SkyViewMatrix;

constexpr char kAdapterIncludeName[] = "TruthSkyViewAdapter.fxh";
constexpr char kRuntimeIncludeName[] = "TruthRuntimeParameters.fxh";
constexpr char kAdapterEntryPoint[] = "TruthSkyViewAdapterProbeMain";
constexpr char kRuntimeEntryPoint[] = "TruthRuntimeParametersProbeMain";
constexpr float kAbsoluteTolerance = 2.0e-5F;
constexpr float kRelativeTolerance = 4.0e-5F;

struct ProbeParameters {
  float texcoord_x;
  float texcoord_y;
  float padding0_x;
  float padding0_y;
  SkyViewMatrix inverse_view_projection;
  float camera_world_x;
  float camera_world_y;
  float camera_world_z;
  float padding1;
  float aurora_origin_world_x;
  float aurora_origin_world_y;
  float aurora_origin_world_z;
  float engine_world_units_per_aurora_unit;
};

struct Float4 {
  float x;
  float y;
  float z;
  float w;
};

struct ProbeResult {
  Float4 direction_and_valid;
  Float4 camera_and_sentinel;
};

struct RuntimeProbeParameters {
  SkyViewMatrix inverse_view_projection;
  Float4 camera_world;
  Float4 celestial;
  Float4 status;
};

static_assert(sizeof(SkyViewMatrix) == 64U);
static_assert(sizeof(ProbeParameters) == 112U);
static_assert(offsetof(ProbeParameters, texcoord_x) == 0U);
static_assert(offsetof(ProbeParameters, inverse_view_projection) == 16U);
static_assert(offsetof(ProbeParameters, camera_world_x) == 80U);
static_assert(offsetof(ProbeParameters, aurora_origin_world_x) == 96U);
static_assert(offsetof(ProbeParameters, engine_world_units_per_aurora_unit) == 108U);
static_assert(sizeof(ProbeResult) == 32U);
static_assert(sizeof(RuntimeProbeParameters) == 112U);
static_assert(offsetof(RuntimeProbeParameters, inverse_view_projection) == 0U);
static_assert(offsetof(RuntimeProbeParameters, camera_world) == 64U);
static_assert(offsetof(RuntimeProbeParameters, celestial) == 80U);
static_assert(offsetof(RuntimeProbeParameters, status) == 96U);

struct TestCase {
  std::string label;
  ProbeParameters parameters;
};

struct RuntimeTestCase {
  std::string label;
  RuntimeProbeParameters parameters;
  ProbeParameters cpu_parameters;
  bool expected_ready;
};

struct Report {
  std::uint64_t assertions{};
  std::uint64_t failures{};
  float maximum_absolute_error{};
  std::vector<std::string> messages;

  void Expect(const bool condition, std::string message) {
    ++assertions;
    if (condition) return;
    ++failures;
    if (messages.size() < 32U) messages.push_back(std::move(message));
  }
};

[[noreturn]] void Fail(std::string message) {
  throw std::runtime_error(std::move(message));
}

void CheckHr(const HRESULT result, const std::string_view operation) {
  if (SUCCEEDED(result)) return;
  std::ostringstream message;
  message << operation << " failed with HRESULT 0x" << std::hex
          << std::uppercase << static_cast<std::uint32_t>(result);
  Fail(message.str());
}

[[nodiscard]] std::string BlobText(ID3DBlob* blob) {
  if (blob == nullptr || blob->GetBufferPointer() == nullptr) return {};
  const auto* begin = static_cast<const char*>(blob->GetBufferPointer());
  return std::string(begin, begin + blob->GetBufferSize());
}

class IncludeResolver final : public ID3DInclude {
 public:
  explicit IncludeResolver(std::filesystem::path directory)
      : directory_(std::filesystem::weakly_canonical(std::move(directory))) {
    if (!std::filesystem::is_directory(directory_)) {
      Fail("adapter shader directory does not exist");
    }
  }

  HRESULT __stdcall Open(D3D_INCLUDE_TYPE,
                         LPCSTR file_name,
                         LPCVOID,
                         LPCVOID* data,
                         UINT* byte_count) override {
    if (file_name == nullptr || data == nullptr || byte_count == nullptr) {
      return E_ACCESSDENIED;
    }
    const std::string_view requested{file_name};
    if (requested != kAdapterIncludeName && requested != kRuntimeIncludeName) {
      return E_ACCESSDENIED;
    }
    try {
      const auto candidate = std::filesystem::weakly_canonical(
          directory_ / requested);
      if (candidate.parent_path() != directory_
          || !std::filesystem::is_regular_file(candidate)) {
        return E_ACCESSDENIED;
      }
      std::ifstream stream(candidate, std::ios::binary | std::ios::ate);
      if (!stream) return E_FAIL;
      const auto length = stream.tellg();
      if (length < 0
          || static_cast<std::uint64_t>(length)
              > std::numeric_limits<UINT>::max()) {
        return E_FAIL;
      }
      stream.seekg(0, std::ios::beg);
      const auto size = static_cast<std::size_t>(length);
      auto* bytes = new (std::nothrow) char[size == 0U ? 1U : size];
      if (bytes == nullptr) return E_OUTOFMEMORY;
      if (size != 0U
          && !stream.read(bytes, static_cast<std::streamsize>(size))) {
        delete[] bytes;
        return E_FAIL;
      }
      *data = bytes;
      *byte_count = static_cast<UINT>(size);
      return S_OK;
    } catch (...) {
      return E_FAIL;
    }
  }

  HRESULT __stdcall Close(LPCVOID data) override {
    delete[] static_cast<const char*>(data);
    return S_OK;
  }

 private:
  std::filesystem::path directory_;
};

struct CompiledProbe {
  ComPtr<ID3DBlob> bytecode;
  ComPtr<ID3D11ShaderReflection> reflection;
};

[[nodiscard]] CompiledProbe CompileProbe(
    const std::filesystem::path& shader_path,
    const char* entry_point) {
  IncludeResolver includes(shader_path.parent_path());
  ComPtr<ID3DBlob> bytecode;
  ComPtr<ID3DBlob> diagnostics;
  constexpr UINT flags = D3DCOMPILE_ENABLE_STRICTNESS
      | D3DCOMPILE_WARNINGS_ARE_ERRORS
      | D3DCOMPILE_IEEE_STRICTNESS
      | D3DCOMPILE_OPTIMIZATION_LEVEL3;
  const HRESULT result = D3DCompileFromFile(
      shader_path.c_str(), nullptr, &includes, entry_point, "cs_5_0",
      flags, 0U, &bytecode, &diagnostics);
  if (FAILED(result)) {
    Fail("strict adapter probe compilation failed:\n" + BlobText(diagnostics.Get()));
  }
  if (diagnostics != nullptr && diagnostics->GetBufferSize() != 0U) {
    Fail("strict adapter probe emitted diagnostics:\n" + BlobText(diagnostics.Get()));
  }

  ComPtr<ID3D11ShaderReflection> reflection;
  CheckHr(D3DReflect(bytecode->GetBufferPointer(), bytecode->GetBufferSize(),
                     __uuidof(ID3D11ShaderReflection),
                     reinterpret_cast<void**>(reflection.GetAddressOf())),
          "D3DReflect");
  return {std::move(bytecode), std::move(reflection)};
}

void ValidateVariable(ID3D11ShaderReflectionConstantBuffer& buffer,
                      const char* name,
                      const UINT expected_offset,
                      const UINT expected_size) {
  auto* variable = buffer.GetVariableByName(name);
  if (variable == nullptr) Fail(std::string{"missing reflected variable: "} + name);
  D3D11_SHADER_VARIABLE_DESC description{};
  CheckHr(variable->GetDesc(&description), std::string{"reflection for "} + name);
  if (description.StartOffset != expected_offset
      || description.Size != expected_size) {
    Fail(std::string{"unexpected reflected ABI for "} + name);
  }
}

void ValidateAdapterReflection(ID3D11ShaderReflection& reflection) {
  D3D11_SHADER_INPUT_BIND_DESC constants{};
  CheckHr(reflection.GetResourceBindingDescByName(
              "TruthSkyViewProbeParameters", &constants),
          "adapter constant-buffer binding reflection");
  if (constants.Type != D3D_SIT_CBUFFER || constants.BindPoint != 2U
      || constants.BindCount != 1U) {
    Fail("adapter probe constant buffer is not bound at b2");
  }
  D3D11_SHADER_INPUT_BIND_DESC output{};
  CheckHr(reflection.GetResourceBindingDescByName(
              "TruthSkyViewProbeResults", &output),
          "adapter output binding reflection");
  if (output.Type != D3D_SIT_UAV_RWSTRUCTURED || output.BindPoint != 0U
      || output.BindCount != 1U) {
    Fail("adapter probe result is not a structured UAV at u0");
  }

  auto* buffer = reflection.GetConstantBufferByName("TruthSkyViewProbeParameters");
  if (buffer == nullptr) Fail("adapter probe constant buffer is absent");
  D3D11_SHADER_BUFFER_DESC description{};
  CheckHr(buffer->GetDesc(&description), "adapter constant-buffer reflection");
  if (description.Size != sizeof(ProbeParameters)) {
    Fail("adapter probe constant-buffer size drifted from the CPU ABI");
  }
  ValidateVariable(*buffer, "TruthSkyViewProbeTexcoord", 0U, 8U);
  ValidateVariable(*buffer, "TruthSkyViewProbeInverseViewProjection", 16U, 64U);
  ValidateVariable(*buffer, "TruthSkyViewProbeCameraWorldPosition", 80U, 12U);
  ValidateVariable(*buffer, "TruthSkyViewProbeAuroraWorldOrigin", 96U, 12U);
  ValidateVariable(*buffer,
                   "TruthSkyViewProbeEngineWorldUnitsPerAuroraUnit",
                   108U, 4U);
}

void ValidateRuntimeReflection(ID3D11ShaderReflection& reflection) {
  D3D11_SHADER_INPUT_BIND_DESC constants{};
  CheckHr(reflection.GetResourceBindingDescByName(
              "TruthRuntimeProbeParameters", &constants),
          "runtime constant-buffer binding reflection");
  if (constants.Type != D3D_SIT_CBUFFER || constants.BindPoint != 2U
      || constants.BindCount != 1U) {
    Fail("runtime probe constant buffer is not bound at b2");
  }
  D3D11_SHADER_INPUT_BIND_DESC output{};
  CheckHr(reflection.GetResourceBindingDescByName(
              "TruthRuntimeProbeResults", &output),
          "runtime output binding reflection");
  if (output.Type != D3D_SIT_UAV_RWSTRUCTURED || output.BindPoint != 0U
      || output.BindCount != 1U) {
    Fail("runtime probe result is not a structured UAV at u0");
  }

  auto* buffer = reflection.GetConstantBufferByName(
      "TruthRuntimeProbeParameters");
  if (buffer == nullptr) Fail("runtime probe constant buffer is absent");
  D3D11_SHADER_BUFFER_DESC description{};
  CheckHr(buffer->GetDesc(&description), "runtime constant-buffer reflection");
  if (description.Size != sizeof(RuntimeProbeParameters)) {
    Fail("runtime probe constant-buffer size drifted from the seven-vector ABI");
  }
  ValidateVariable(*buffer, "TruthRuntimeInverseViewProjectionRow0", 0U, 16U);
  ValidateVariable(*buffer, "TruthRuntimeInverseViewProjectionRow1", 16U, 16U);
  ValidateVariable(*buffer, "TruthRuntimeInverseViewProjectionRow2", 32U, 16U);
  ValidateVariable(*buffer, "TruthRuntimeInverseViewProjectionRow3", 48U, 16U);
  ValidateVariable(*buffer, "TruthRuntimeCameraWorld", 64U, 16U);
  ValidateVariable(*buffer, "TruthRuntimeCelestial", 80U, 16U);
  ValidateVariable(*buffer, "TruthRuntimeStatus", 96U, 16U);
}

struct WarpDevice {
  ComPtr<ID3D11Device> device;
  ComPtr<ID3D11DeviceContext> context;
};

[[nodiscard]] WarpDevice CreateWarpDevice() {
  constexpr std::array levels{
      D3D_FEATURE_LEVEL_11_1,
      D3D_FEATURE_LEVEL_11_0,
  };
  WarpDevice result;
  D3D_FEATURE_LEVEL selected{};
  HRESULT hr = D3D11CreateDevice(
      nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
      D3D11_CREATE_DEVICE_SINGLETHREADED,
      levels.data(), static_cast<UINT>(levels.size()), D3D11_SDK_VERSION,
      &result.device, &selected, &result.context);
  if (hr == E_INVALIDARG) {
    hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
        D3D11_CREATE_DEVICE_SINGLETHREADED,
        levels.data() + 1U, 1U, D3D11_SDK_VERSION,
        &result.device, &selected, &result.context);
  }
  CheckHr(hr, "D3D11CreateDevice(WARP)");
  if (selected < D3D_FEATURE_LEVEL_11_0) {
    Fail("WARP did not provide feature level 11_0");
  }
  return result;
}

template <typename Parameters>
[[nodiscard]] ProbeResult Execute(
    WarpDevice& warp,
    ID3DBlob& bytecode,
    const Parameters& parameters) {
  ComPtr<ID3D11ComputeShader> shader;
  CheckHr(warp.device->CreateComputeShader(
              bytecode.GetBufferPointer(), bytecode.GetBufferSize(),
              nullptr, &shader),
          "CreateComputeShader");

  D3D11_BUFFER_DESC constants_description{};
  constants_description.ByteWidth = sizeof(Parameters);
  constants_description.Usage = D3D11_USAGE_IMMUTABLE;
  constants_description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  D3D11_SUBRESOURCE_DATA constants_data{};
  constants_data.pSysMem = &parameters;
  ComPtr<ID3D11Buffer> constants;
  CheckHr(warp.device->CreateBuffer(
              &constants_description, &constants_data, &constants),
          "CreateBuffer(adapter constants)");

  D3D11_BUFFER_DESC output_description{};
  output_description.ByteWidth = sizeof(ProbeResult);
  output_description.Usage = D3D11_USAGE_DEFAULT;
  output_description.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
  output_description.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
  output_description.StructureByteStride = sizeof(ProbeResult);
  ComPtr<ID3D11Buffer> output;
  CheckHr(warp.device->CreateBuffer(&output_description, nullptr, &output),
          "CreateBuffer(adapter output)");

  D3D11_UNORDERED_ACCESS_VIEW_DESC view_description{};
  view_description.Format = DXGI_FORMAT_UNKNOWN;
  view_description.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
  view_description.Buffer.NumElements = 1U;
  ComPtr<ID3D11UnorderedAccessView> output_view;
  CheckHr(warp.device->CreateUnorderedAccessView(
              output.Get(), &view_description, &output_view),
          "CreateUnorderedAccessView(adapter output)");

  D3D11_BUFFER_DESC staging_description{};
  staging_description.ByteWidth = sizeof(ProbeResult);
  staging_description.Usage = D3D11_USAGE_STAGING;
  staging_description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  ComPtr<ID3D11Buffer> staging;
  CheckHr(warp.device->CreateBuffer(&staging_description, nullptr, &staging),
          "CreateBuffer(adapter staging)");

  ID3D11Buffer* constants_pointer = constants.Get();
  ID3D11UnorderedAccessView* output_pointer = output_view.Get();
  warp.context->CSSetShader(shader.Get(), nullptr, 0U);
  warp.context->CSSetConstantBuffers(2U, 1U, &constants_pointer);
  warp.context->CSSetUnorderedAccessViews(0U, 1U, &output_pointer, nullptr);
  warp.context->Dispatch(1U, 1U, 1U);

  ID3D11Buffer* null_buffer = nullptr;
  ID3D11UnorderedAccessView* null_view = nullptr;
  warp.context->CSSetConstantBuffers(2U, 1U, &null_buffer);
  warp.context->CSSetUnorderedAccessViews(0U, 1U, &null_view, nullptr);
  warp.context->CSSetShader(nullptr, nullptr, 0U);
  warp.context->CopyResource(staging.Get(), output.Get());

  D3D11_MAPPED_SUBRESOURCE mapped{};
  CheckHr(warp.context->Map(staging.Get(), 0U, D3D11_MAP_READ, 0U, &mapped),
          "Map(adapter staging)");
  ProbeResult result{};
  std::memcpy(&result, mapped.pData, sizeof(result));
  warp.context->Unmap(staging.Get(), 0U);
  return result;
}

[[nodiscard]] SkyViewMatrix IdentityMatrix() noexcept {
  return {1.0F, 0.0F, 0.0F, 0.0F,
          0.0F, 1.0F, 0.0F, 0.0F,
          0.0F, 0.0F, 1.0F, 0.0F,
          0.0F, 0.0F, 0.0F, 1.0F};
}

[[nodiscard]] ProbeParameters BaseParameters() noexcept {
  ProbeParameters value{};
  value.texcoord_x = 0.5F;
  value.texcoord_y = 0.5F;
  value.inverse_view_projection = IdentityMatrix();
  value.engine_world_units_per_aurora_unit = 1.0F;
  return value;
}

[[nodiscard]] std::vector<TestCase> Cases() {
  std::vector<TestCase> cases;
  cases.push_back({"identity center", BaseParameters()});

  auto corner = BaseParameters();
  corner.texcoord_x = 0.0F;
  corner.texcoord_y = 0.0F;
  cases.push_back({"NDC upper-left corner", corner});

  auto translated = BaseParameters();
  translated.camera_world_x = 10.0F;
  translated.camera_world_y = -20.0F;
  translated.camera_world_z = 30.0F;
  translated.inverse_view_projection.m03 = 10.0F;
  translated.inverse_view_projection.m13 = -20.0F;
  translated.inverse_view_projection.m23 = 30.0F;
  translated.aurora_origin_world_x = 2.0F;
  translated.aurora_origin_world_y = -4.0F;
  translated.aurora_origin_world_z = 6.0F;
  translated.engine_world_units_per_aurora_unit = 2.0F;
  cases.push_back({"translated camera and origin", translated});

  const float nan = std::numeric_limits<float>::quiet_NaN();
  const float infinity = std::numeric_limits<float>::infinity();
  auto add_invalid = [&](std::string label, ProbeParameters parameters) {
    cases.push_back({std::move(label), parameters});
  };
  auto invalid = BaseParameters(); invalid.texcoord_x = nan;
  add_invalid("NaN texcoord", invalid);
  invalid = BaseParameters(); invalid.texcoord_y = infinity;
  add_invalid("infinite texcoord", invalid);
  invalid = BaseParameters(); invalid.inverse_view_projection.m12 = nan;
  add_invalid("NaN matrix", invalid);
  invalid = BaseParameters(); invalid.camera_world_z = infinity;
  add_invalid("infinite camera", invalid);
  invalid = BaseParameters(); invalid.aurora_origin_world_y = nan;
  add_invalid("NaN origin", invalid);
  invalid = BaseParameters(); invalid.engine_world_units_per_aurora_unit = nan;
  add_invalid("NaN scale", invalid);
  invalid = BaseParameters(); invalid.inverse_view_projection.m33 = 0.0F;
  add_invalid("zero homogeneous w", invalid);
  invalid = BaseParameters(); invalid.inverse_view_projection.m23 = -1.0F;
  add_invalid("zero world ray", invalid);
  invalid = BaseParameters(); invalid.camera_world_x = 100000001.0F;
  add_invalid("camera outside engine range", invalid);
  return cases;
}

[[nodiscard]] RuntimeProbeParameters RuntimeBaseParameters() noexcept {
  RuntimeProbeParameters value{};
  value.inverse_view_projection = IdentityMatrix();
  value.status = {1.0F, 1.0F, 1.0F, 1.0F};
  return value;
}

[[nodiscard]] ProbeParameters RuntimeCpuParameters(
    const RuntimeProbeParameters& runtime) noexcept {
  ProbeParameters value{};
  value.texcoord_x = 0.5F;
  value.texcoord_y = 0.5F;
  value.inverse_view_projection = runtime.inverse_view_projection;
  value.camera_world_x = runtime.camera_world.x;
  value.camera_world_y = runtime.camera_world.y;
  value.camera_world_z = runtime.camera_world.z;
  value.engine_world_units_per_aurora_unit = runtime.status.w;
  return value;
}

[[nodiscard]] std::vector<RuntimeTestCase> RuntimeCases() {
  std::vector<RuntimeTestCase> cases;
  auto identity = RuntimeBaseParameters();
  cases.push_back({"runtime identity rows", identity,
                   RuntimeCpuParameters(identity), true});

  auto asymmetric = RuntimeBaseParameters();
  asymmetric.inverse_view_projection = {
      1.3F, 0.2F, -0.1F, 11.0F,
      -0.4F, 0.9F, 0.3F, -13.0F,
      0.2F, -0.3F, 1.1F, 17.0F,
      0.001F, -0.002F, 0.003F, 1.0F,
  };
  asymmetric.camera_world = {3.0F, -5.0F, 7.0F, 0.0F};
  asymmetric.status = {1.0F, 1.0F, 29.0F, 2.0F};
  cases.push_back({"runtime asymmetric row-major matrix", asymmetric,
                   RuntimeCpuParameters(asymmetric), true});

  auto invalid_protocol = RuntimeBaseParameters();
  invalid_protocol.status.x = 0.0F;
  cases.push_back({"runtime fail-closed protocol", invalid_protocol,
                   RuntimeCpuParameters(invalid_protocol), false});

  auto unsupported_minor_protocol = RuntimeBaseParameters();
  unsupported_minor_protocol.status.x = 1.5F;
  cases.push_back({"runtime rejects undocumented minor protocol",
                   unsupported_minor_protocol,
                   RuntimeCpuParameters(unsupported_minor_protocol), false});

  auto invalid_matrix = RuntimeBaseParameters();
  invalid_matrix.inverse_view_projection.m12 =
      std::numeric_limits<float>::quiet_NaN();
  cases.push_back({"runtime non-finite matrix", invalid_matrix,
                   RuntimeCpuParameters(invalid_matrix), true});
  return cases;
}

[[nodiscard]] SkyViewAdapterInput ToCpuInput(
    const ProbeParameters& value) noexcept {
  return {
      value.texcoord_x,
      value.texcoord_y,
      value.inverse_view_projection,
      value.camera_world_x,
      value.camera_world_y,
      value.camera_world_z,
      value.aurora_origin_world_x,
      value.aurora_origin_world_y,
      value.aurora_origin_world_z,
      value.engine_world_units_per_aurora_unit,
  };
}

void CompareFloat(Report& report,
                  const std::string& label,
                  const char* component,
                  const float cpu,
                  const float gpu) {
  if (!std::isfinite(gpu)) {
    report.Expect(false, label + " GPU " + component + " is non-finite");
    return;
  }
  const float error = std::fabs(cpu - gpu);
  report.maximum_absolute_error = std::max(report.maximum_absolute_error, error);
  const float scale = std::max(std::fabs(cpu), std::fabs(gpu));
  const float allowed = kAbsoluteTolerance + (kRelativeTolerance * scale);
  std::ostringstream message;
  message << label << " " << component << " diverged: cpu=" << cpu
          << " gpu=" << gpu << " error=" << error << " allowed=" << allowed;
  report.Expect(error <= allowed, message.str());
}

void CompareCase(Report& report,
                 const TestCase& test_case,
                 const ProbeResult& gpu) {
  SkyViewAdapterOutput cpu{};
  const auto evaluation = EvaluateSkyViewAdapter(
      ToCpuInput(test_case.parameters), cpu);
  const bool cpu_valid = evaluation.status == SkyViewAdapterStatus::evaluated;
  const bool gpu_valid = std::isfinite(gpu.direction_and_valid.w)
      && gpu.direction_and_valid.w > 0.5F;
  report.Expect(cpu_valid == gpu_valid,
                test_case.label + " CPU/GPU validity diverged");
  report.Expect(std::isfinite(gpu.direction_and_valid.x)
                    && std::isfinite(gpu.direction_and_valid.y)
                    && std::isfinite(gpu.direction_and_valid.z)
                    && std::isfinite(gpu.camera_and_sentinel.x)
                    && std::isfinite(gpu.camera_and_sentinel.y)
                    && std::isfinite(gpu.camera_and_sentinel.z),
                test_case.label + " GPU emitted a non-finite fallback/output");
  report.Expect(gpu.camera_and_sentinel.w == 47.0F,
                test_case.label + " dispatch sentinel drifted");
  if (!cpu_valid || !gpu_valid) return;
  CompareFloat(report, test_case.label, "direction.x", cpu.view_world_x,
               gpu.direction_and_valid.x);
  CompareFloat(report, test_case.label, "direction.y", cpu.view_world_y,
               gpu.direction_and_valid.y);
  CompareFloat(report, test_case.label, "direction.z", cpu.view_world_z,
               gpu.direction_and_valid.z);
  CompareFloat(report, test_case.label, "camera.x", cpu.camera_aurora_x,
               gpu.camera_and_sentinel.x);
  CompareFloat(report, test_case.label, "camera.y", cpu.camera_aurora_y,
               gpu.camera_and_sentinel.y);
  CompareFloat(report, test_case.label, "camera.z", cpu.camera_aurora_z,
               gpu.camera_and_sentinel.z);
}

void CompareRuntimeCase(Report& report,
                        const RuntimeTestCase& test_case,
                        const ProbeResult& gpu) {
  const float expected_sentinel = test_case.expected_ready ? 47.0F : -47.0F;
  report.Expect(gpu.camera_and_sentinel.w == expected_sentinel,
                test_case.label + " readiness sentinel drifted");
  if (!test_case.expected_ready) {
    report.Expect(gpu.direction_and_valid.w == 0.0F,
                  test_case.label + " did not fail closed");
    report.Expect(gpu.direction_and_valid.x == 0.0F
                      && gpu.direction_and_valid.y == 0.0F
                      && gpu.direction_and_valid.z == 1.0F
                      && gpu.camera_and_sentinel.x == 0.0F
                      && gpu.camera_and_sentinel.y == 0.0F
                      && gpu.camera_and_sentinel.z == 0.0F,
                  test_case.label + " mutated the fallback output");
    return;
  }
  CompareCase(report,
              TestCase{test_case.label, test_case.cpu_parameters}, gpu);
}

struct ReflectedBuffer {
  UINT bind_point{};
  std::vector<std::byte> bytes;
  ID3D11ShaderReflectionConstantBuffer* reflection{};
};

struct ProductionTextures {
  Float4 color{0.18F, 0.62F, 2.0F, 1.0F};
  Float4 bloom{};
  Float4 lens{};
  Float4 adaptation{1.0F, 0.0F, 0.0F, 0.0F};
};

[[nodiscard]] bool EqualPathPart(const std::filesystem::path& left,
                                 const std::filesystem::path& right) {
  return _wcsicmp(left.c_str(), right.c_str()) == 0;
}

[[nodiscard]] bool IsWithin(const std::filesystem::path& root,
                            const std::filesystem::path& candidate) {
  auto root_part = root.begin();
  auto candidate_part = candidate.begin();
  for (; root_part != root.end(); ++root_part, ++candidate_part) {
    if (candidate_part == candidate.end()
        || !EqualPathPart(*root_part, *candidate_part)) {
      return false;
    }
  }
  return true;
}

class ProductionIncludeResolver final : public ID3DInclude {
 public:
  explicit ProductionIncludeResolver(std::filesystem::path root)
      : root_(std::filesystem::weakly_canonical(std::move(root))) {
    if (!std::filesystem::is_directory(root_)) {
      Fail("production shader root does not exist");
    }
  }

  HRESULT __stdcall Open(D3D_INCLUDE_TYPE,
                         LPCSTR file_name,
                         LPCVOID,
                         LPCVOID* data,
                         UINT* byte_count) override {
    if (file_name == nullptr || data == nullptr || byte_count == nullptr) {
      return E_INVALIDARG;
    }
    try {
      const std::filesystem::path requested{file_name};
      if (requested.is_absolute()) return E_ACCESSDENIED;
      for (const auto& base : {root_, root_ / "truth"}) {
        const auto candidate = std::filesystem::weakly_canonical(
            base / requested);
        if (!IsWithin(root_, candidate)
            || !std::filesystem::is_regular_file(candidate)) {
          continue;
        }
        std::ifstream stream(candidate, std::ios::binary | std::ios::ate);
        if (!stream) return E_FAIL;
        const auto length = stream.tellg();
        if (length < 0
            || static_cast<std::uint64_t>(length)
                > std::numeric_limits<UINT>::max()) {
          return E_FAIL;
        }
        stream.seekg(0, std::ios::beg);
        const auto size = static_cast<std::size_t>(length);
        auto* bytes = new (std::nothrow) char[std::max<std::size_t>(size, 1U)];
        if (bytes == nullptr) return E_OUTOFMEMORY;
        if (size != 0U
            && !stream.read(bytes, static_cast<std::streamsize>(size))) {
          delete[] bytes;
          return E_FAIL;
        }
        *data = bytes;
        *byte_count = static_cast<UINT>(size);
        return S_OK;
      }
      return E_ACCESSDENIED;
    } catch (...) {
      return E_FAIL;
    }
  }

  HRESULT __stdcall Close(LPCVOID data) override {
    delete[] static_cast<const char*>(data);
    return S_OK;
  }

 private:
  std::filesystem::path root_;
};

[[nodiscard]] CompiledProbe CompileProductionPixel(
    const std::filesystem::path& effect_path) {
  ProductionIncludeResolver includes(effect_path.parent_path());
  ComPtr<ID3DBlob> bytecode;
  ComPtr<ID3DBlob> diagnostics;
  constexpr UINT flags = D3DCOMPILE_ENABLE_STRICTNESS
      | D3DCOMPILE_WARNINGS_ARE_ERRORS
      | D3DCOMPILE_IEEE_STRICTNESS
      | D3DCOMPILE_OPTIMIZATION_LEVEL3;
  const HRESULT result = D3DCompileFromFile(
      effect_path.c_str(), nullptr, &includes,
      "TruthEnbPixelMain", "ps_5_0", flags, 0U,
      &bytecode, &diagnostics);
  if (FAILED(result)) {
    Fail("strict production pixel compilation failed:\n"
         + BlobText(diagnostics.Get()));
  }
  if (diagnostics != nullptr && diagnostics->GetBufferSize() != 0U) {
    Fail("strict production pixel emitted diagnostics:\n"
         + BlobText(diagnostics.Get()));
  }
  ComPtr<ID3D11ShaderReflection> reflection;
  CheckHr(D3DReflect(bytecode->GetBufferPointer(), bytecode->GetBufferSize(),
                     __uuidof(ID3D11ShaderReflection),
                     reinterpret_cast<void**>(reflection.GetAddressOf())),
          "D3DReflect(production pixel)");
  return {std::move(bytecode), std::move(reflection)};
}

[[nodiscard]] ComPtr<ID3DBlob> CompileProductionVertex() {
  constexpr char source[] =
      "struct O{float4 p:SV_Position;float2 uv:TEXCOORD0;};"
      "O main(uint id:SV_VertexID){"
      "float2 p=id==0?float2(-1,-1):(id==1?float2(-1,3):float2(3,-1));"
      "O o;o.p=float4(p,0,1);o.uv=float2((p.x+1)*0.5,1-(p.y+1)*0.5);"
      "return o;}";
  ComPtr<ID3DBlob> bytecode;
  ComPtr<ID3DBlob> diagnostics;
  constexpr UINT flags = D3DCOMPILE_ENABLE_STRICTNESS
      | D3DCOMPILE_WARNINGS_ARE_ERRORS
      | D3DCOMPILE_IEEE_STRICTNESS
      | D3DCOMPILE_OPTIMIZATION_LEVEL3;
  const HRESULT result = D3DCompile(
      source, sizeof(source) - 1U, "TruthProductionFullscreenVertex",
      nullptr, nullptr, "main", "vs_5_0", flags, 0U,
      &bytecode, &diagnostics);
  if (FAILED(result)) {
    Fail("production fullscreen vertex compilation failed:\n"
         + BlobText(diagnostics.Get()));
  }
  if (diagnostics != nullptr && diagnostics->GetBufferSize() != 0U) {
    Fail("production fullscreen vertex emitted diagnostics:\n"
         + BlobText(diagnostics.Get()));
  }
  return bytecode;
}

[[nodiscard]] ReflectedBuffer ReflectGlobals(
    ID3D11ShaderReflection& reflection) {
  D3D11_SHADER_INPUT_BIND_DESC binding{};
  CheckHr(reflection.GetResourceBindingDescByName("$Globals", &binding),
          "production $Globals resource reflection");
  if (binding.Type != D3D_SIT_CBUFFER || binding.BindCount != 1U) {
    Fail("production $Globals is not one constant buffer");
  }
  auto* buffer = reflection.GetConstantBufferByName("$Globals");
  if (buffer == nullptr) Fail("production $Globals is absent");
  D3D11_SHADER_BUFFER_DESC description{};
  CheckHr(buffer->GetDesc(&description), "production $Globals reflection");
  if (description.Size != 368U) {
    std::string layout = "production $Globals reflected "
        + std::to_string(description.Size) + " bytes:";
    for (UINT index = 0U; index < description.Variables; ++index) {
      D3D11_SHADER_VARIABLE_DESC variable_description{};
      CheckHr(buffer->GetVariableByIndex(index)->GetDesc(&variable_description),
              "production variable diagnostic reflection");
      layout += " " + std::string{variable_description.Name}
          + "@" + std::to_string(variable_description.StartOffset)
          + "+" + std::to_string(variable_description.Size);
    }
    Fail(layout);
  }
  ReflectedBuffer result{
      binding.BindPoint,
      std::vector<std::byte>(description.Size, std::byte{0}),
      buffer,
  };
  for (UINT index = 0U; index < description.Variables; ++index) {
    auto* variable = buffer->GetVariableByIndex(index);
    D3D11_SHADER_VARIABLE_DESC variable_description{};
    CheckHr(variable->GetDesc(&variable_description),
            "production variable reflection");
    if (variable_description.DefaultValue != nullptr) {
      std::memcpy(result.bytes.data() + variable_description.StartOffset,
                  variable_description.DefaultValue,
                  variable_description.Size);
    }
  }
  ValidateVariable(*buffer, "TruthRuntimeInverseViewProjectionRow0", 0U, 16U);
  ValidateVariable(*buffer, "TruthRuntimeInverseViewProjectionRow3", 48U, 16U);
  ValidateVariable(*buffer, "TruthRuntimeCameraWorld", 64U, 16U);
  ValidateVariable(*buffer, "TruthRuntimeCelestial", 80U, 16U);
  ValidateVariable(*buffer, "TruthRuntimeStatus", 96U, 16U);
  ValidateVariable(*buffer, "TruthMasterEnabled", 112U, 4U);
  ValidateVariable(*buffer, "TruthAuroraWorldOrigin", 192U, 12U);
  ValidateVariable(*buffer, "Timer", 208U, 16U);
  ValidateVariable(*buffer, "EInteriorFactor", 224U, 4U);
  ValidateVariable(*buffer, "ENBParams01", 352U, 16U);
  return result;
}

template <class T>
void SetVariable(ReflectedBuffer& buffer, const char* name, const T& value) {
  auto* variable = buffer.reflection->GetVariableByName(name);
  if (variable == nullptr) Fail(std::string{"missing production variable "} + name);
  D3D11_SHADER_VARIABLE_DESC description{};
  CheckHr(variable->GetDesc(&description),
          std::string{"production reflection for "} + name);
  if (description.Size != sizeof(T)
      || description.StartOffset > buffer.bytes.size()
      || description.Size > buffer.bytes.size() - description.StartOffset) {
    Fail(std::string{"unexpected production variable layout for "} + name);
  }
  std::memcpy(buffer.bytes.data() + description.StartOffset, &value, sizeof(T));
}

void SetBool(ReflectedBuffer& buffer, const char* name, const bool value) {
  SetVariable(buffer, name, static_cast<std::uint32_t>(value ? 1U : 0U));
}

void ValidateProductionResources(ID3D11ShaderReflection& reflection) {
  struct ExpectedResource {
    const char* name;
    D3D_SHADER_INPUT_TYPE type;
    UINT bind_point;
  };
  constexpr ExpectedResource expected[]{
      {"Sampler0", D3D_SIT_SAMPLER, 0U},
      {"Sampler1", D3D_SIT_SAMPLER, 1U},
      {"TextureColor", D3D_SIT_TEXTURE, 0U},
      {"TextureBloom", D3D_SIT_TEXTURE, 1U},
      {"TextureLens", D3D_SIT_TEXTURE, 2U},
      {"TextureAdaptation", D3D_SIT_TEXTURE, 3U},
  };
  for (const auto& resource : expected) {
    D3D11_SHADER_INPUT_BIND_DESC binding{};
    CheckHr(reflection.GetResourceBindingDescByName(resource.name, &binding),
            std::string{"production resource reflection for "} + resource.name);
    if (binding.Type != resource.type || binding.BindPoint != resource.bind_point
        || binding.BindCount != 1U) {
      Fail(std::string{"production resource binding drifted for "}
           + resource.name);
    }
  }
}

struct ProductionRenderer {
  ComPtr<ID3D11VertexShader> vertex_shader;
  ComPtr<ID3D11PixelShader> pixel_shader;
  ComPtr<ID3D11Texture2D> target;
  ComPtr<ID3D11RenderTargetView> target_view;
  ComPtr<ID3D11Texture2D> staging;
  ComPtr<ID3D11SamplerState> point_sampler;
  ComPtr<ID3D11SamplerState> linear_sampler;
};

[[nodiscard]] ProductionRenderer CreateProductionRenderer(
    WarpDevice& warp,
    ID3DBlob& pixel_bytecode,
    ID3DBlob& vertex_bytecode) {
  ProductionRenderer result;
  CheckHr(warp.device->CreateVertexShader(
              vertex_bytecode.GetBufferPointer(),
              vertex_bytecode.GetBufferSize(), nullptr,
              &result.vertex_shader),
          "CreateVertexShader(production)");
  CheckHr(warp.device->CreatePixelShader(
              pixel_bytecode.GetBufferPointer(),
              pixel_bytecode.GetBufferSize(), nullptr,
              &result.pixel_shader),
          "CreatePixelShader(production)");

  D3D11_TEXTURE2D_DESC target_description{};
  target_description.Width = 1U;
  target_description.Height = 1U;
  target_description.MipLevels = 1U;
  target_description.ArraySize = 1U;
  target_description.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
  target_description.SampleDesc.Count = 1U;
  target_description.Usage = D3D11_USAGE_DEFAULT;
  target_description.BindFlags = D3D11_BIND_RENDER_TARGET;
  CheckHr(warp.device->CreateTexture2D(
              &target_description, nullptr, &result.target),
          "CreateTexture2D(production target)");
  CheckHr(warp.device->CreateRenderTargetView(
              result.target.Get(), nullptr, &result.target_view),
          "CreateRenderTargetView(production)");
  target_description.Usage = D3D11_USAGE_STAGING;
  target_description.BindFlags = 0U;
  target_description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  CheckHr(warp.device->CreateTexture2D(
              &target_description, nullptr, &result.staging),
          "CreateTexture2D(production staging)");

  D3D11_SAMPLER_DESC sampler{};
  sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
  sampler.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
  sampler.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
  sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
  sampler.MaxLOD = D3D11_FLOAT32_MAX;
  CheckHr(warp.device->CreateSamplerState(&sampler, &result.point_sampler),
          "CreateSamplerState(point)");
  sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
  CheckHr(warp.device->CreateSamplerState(&sampler, &result.linear_sampler),
          "CreateSamplerState(linear)");
  return result;
}

[[nodiscard]] ComPtr<ID3D11Buffer> UploadGlobals(
    WarpDevice& warp,
    const ReflectedBuffer& source) {
  D3D11_BUFFER_DESC description{};
  description.ByteWidth = static_cast<UINT>(source.bytes.size());
  description.Usage = D3D11_USAGE_IMMUTABLE;
  description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  D3D11_SUBRESOURCE_DATA data{};
  data.pSysMem = source.bytes.data();
  ComPtr<ID3D11Buffer> result;
  CheckHr(warp.device->CreateBuffer(&description, &data, &result),
          "CreateBuffer(production globals)");
  return result;
}

[[nodiscard]] ComPtr<ID3D11ShaderResourceView> UploadTexture(
    WarpDevice& warp,
    const Float4& pixel) {
  D3D11_TEXTURE2D_DESC description{};
  description.Width = 1U;
  description.Height = 1U;
  description.MipLevels = 1U;
  description.ArraySize = 1U;
  description.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
  description.SampleDesc.Count = 1U;
  description.Usage = D3D11_USAGE_IMMUTABLE;
  description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
  D3D11_SUBRESOURCE_DATA data{};
  data.pSysMem = &pixel;
  data.SysMemPitch = sizeof(Float4);
  ComPtr<ID3D11Texture2D> texture;
  CheckHr(warp.device->CreateTexture2D(&description, &data, &texture),
          "CreateTexture2D(production input)");
  ComPtr<ID3D11ShaderResourceView> view;
  CheckHr(warp.device->CreateShaderResourceView(
              texture.Get(), nullptr, &view),
          "CreateShaderResourceView(production input)");
  return view;
}

[[nodiscard]] Float4 RenderProduction(
    WarpDevice& warp,
    ProductionRenderer& renderer,
    const ReflectedBuffer& globals,
    const ProductionTextures& textures) {
  const auto globals_buffer = UploadGlobals(warp, globals);
  std::array<ComPtr<ID3D11ShaderResourceView>, 4U> owned_views{
      UploadTexture(warp, textures.color),
      UploadTexture(warp, textures.bloom),
      UploadTexture(warp, textures.lens),
      UploadTexture(warp, textures.adaptation),
  };
  std::array<ID3D11ShaderResourceView*, 4U> views{};
  for (std::size_t index = 0U; index < views.size(); ++index) {
    views[index] = owned_views[index].Get();
  }
  std::array<ID3D11SamplerState*, 2U> samplers{
      renderer.point_sampler.Get(), renderer.linear_sampler.Get()};
  D3D11_VIEWPORT viewport{};
  viewport.Width = 1.0F;
  viewport.Height = 1.0F;
  viewport.MaxDepth = 1.0F;
  ID3D11RenderTargetView* target = renderer.target_view.Get();
  ID3D11Buffer* constants = globals_buffer.Get();
  warp.context->OMSetRenderTargets(1U, &target, nullptr);
  warp.context->RSSetViewports(1U, &viewport);
  warp.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  warp.context->VSSetShader(renderer.vertex_shader.Get(), nullptr, 0U);
  warp.context->PSSetShader(renderer.pixel_shader.Get(), nullptr, 0U);
  warp.context->PSSetConstantBuffers(globals.bind_point, 1U, &constants);
  warp.context->PSSetShaderResources(0U, static_cast<UINT>(views.size()),
                                     views.data());
  warp.context->PSSetSamplers(0U, static_cast<UINT>(samplers.size()),
                              samplers.data());
  warp.context->Draw(3U, 0U);
  warp.context->CopyResource(renderer.staging.Get(), renderer.target.Get());

  std::array<ID3D11ShaderResourceView*, 4U> null_views{};
  ID3D11Buffer* null_buffer = nullptr;
  warp.context->PSSetShaderResources(0U, static_cast<UINT>(null_views.size()),
                                     null_views.data());
  warp.context->PSSetConstantBuffers(globals.bind_point, 1U, &null_buffer);
  warp.context->PSSetShader(nullptr, nullptr, 0U);
  warp.context->VSSetShader(nullptr, nullptr, 0U);
  warp.context->OMSetRenderTargets(0U, nullptr, nullptr);

  D3D11_MAPPED_SUBRESOURCE mapped{};
  CheckHr(warp.context->Map(renderer.staging.Get(), 0U,
                            D3D11_MAP_READ, 0U, &mapped),
          "Map(production output)");
  Float4 result{};
  std::memcpy(&result, mapped.pData, sizeof(result));
  warp.context->Unmap(renderer.staging.Get(), 0U);
  return result;
}

void ConfigureProductionFrame(ReflectedBuffer& globals) {
  SetVariable(globals, "Timer", Float4{0.25F, 60.0F, 0.0F, 1.0F / 60.0F});
  SetVariable(globals, "EInteriorFactor", 0.0F);
  SetVariable(globals, "ENBParams01", Float4{});
  SetBool(globals, "TruthUseEnbBloom", false);
  SetBool(globals, "TruthUseEnbLens", false);
}

[[nodiscard]] Float4 ExpectedTone(const Float4 scene, const float ev) {
  const float scale = std::exp2(ev);
  return {
      FilmicToneCurve(std::max(scene.x, 0.0F) * scale),
      FilmicToneCurve(std::max(scene.y, 0.0F) * scale),
      FilmicToneCurve(std::max(scene.z, 0.0F) * scale),
      1.0F,
  };
}

void CompareProduction(Report& report,
                       const std::string& label,
                       const Float4 actual,
                       const Float4 expected) {
  CompareFloat(report, label, "red", expected.x, actual.x);
  CompareFloat(report, label, "green", expected.y, actual.y);
  CompareFloat(report, label, "blue", expected.z, actual.z);
  CompareFloat(report, label, "alpha", expected.w, actual.w);
}

void ExpectFiniteBounded(Report& report,
                         const std::string& label,
                         const Float4 value) {
  report.Expect(std::isfinite(value.x) && std::isfinite(value.y)
                    && std::isfinite(value.z) && std::isfinite(value.w),
                label + " emitted a non-finite pixel");
  report.Expect(value.x >= 0.0F && value.x <= 1.0F
                    && value.y >= 0.0F && value.y <= 1.0F
                    && value.z >= 0.0F && value.z <= 1.0F
                    && value.w == 1.0F,
                label + " left the display/alpha bounds");
}

void EnableRuntime(ReflectedBuffer& globals) {
  SetVariable(globals, "TruthRuntimeInverseViewProjectionRow0",
              Float4{1.0F, 0.0F, 0.0F, 0.0F});
  SetVariable(globals, "TruthRuntimeInverseViewProjectionRow1",
              Float4{0.0F, 1.0F, 0.0F, 0.0F});
  SetVariable(globals, "TruthRuntimeInverseViewProjectionRow2",
              Float4{0.0F, 0.0F, 1.0F, 0.0F});
  SetVariable(globals, "TruthRuntimeInverseViewProjectionRow3",
              Float4{0.0F, 0.0F, 0.0F, 1.0F});
  SetVariable(globals, "TruthRuntimeCameraWorld", Float4{});
  SetVariable(globals, "TruthRuntimeCelestial",
              Float4{0.0F, 0.0F, 1.0F, 1.0F});
  SetVariable(globals, "TruthRuntimeStatus",
              Float4{1.1F, 1.0F, 17.0F, 1.0F});
}

void ExerciseProductionPixel(Report& report,
                             WarpDevice& warp,
                             ProductionRenderer& renderer,
                             const ReflectedBuffer& defaults) {
  ProductionTextures textures;
  ReflectedBuffer globals = defaults;
  ConfigureProductionFrame(globals);

  const Float4 default_pixel = RenderProduction(
      warp, renderer, globals, textures);
  CompareProduction(report, "production defaults", default_pixel,
                    ExpectedTone(textures.color, 0.0F));

  for (const float exposure : {-8.0F, 8.0F}) {
    ReflectedBuffer exposure_globals = globals;
    SetVariable(exposure_globals, "TruthManualExposureEv", exposure);
    CompareProduction(
        report,
        exposure < 0.0F ? "manual exposure minimum" : "manual exposure maximum",
        RenderProduction(warp, renderer, exposure_globals, textures),
        ExpectedTone(textures.color, exposure));
  }

  ReflectedBuffer passthrough_globals = globals;
  SetBool(passthrough_globals, "TruthMasterEnabled", false);
  const Float4 passthrough_scene{0.08F, 0.45F, 0.91F, 0.25F};
  ProductionTextures passthrough_textures = textures;
  passthrough_textures.color = passthrough_scene;
  CompareProduction(report, "master passthrough",
                    RenderProduction(warp, renderer, passthrough_globals,
                                     passthrough_textures),
                    Float4{passthrough_scene.x, passthrough_scene.y,
                           passthrough_scene.z, 1.0F});

  ReflectedBuffer optical_globals = passthrough_globals;
  SetBool(optical_globals, "TruthUseEnbBloom", true);
  SetBool(optical_globals, "TruthUseEnbLens", true);
  SetVariable(optical_globals, "ENBParams01",
              Float4{0.5F, 2.0F, 0.0F, 0.0F});
  ProductionTextures optical_textures = textures;
  optical_textures.color = {0.1F, 0.2F, 0.3F, 1.0F};
  optical_textures.lens = {0.05F, 0.05F, 0.05F, 0.0F};
  optical_textures.bloom = {0.8F, 0.7F, 0.6F, 0.0F};
  CompareProduction(report, "ENB lens and bloom bindings",
                    RenderProduction(warp, renderer, optical_globals,
                                     optical_textures),
                    Float4{0.5F, 0.5F, 0.5F, 1.0F});

  const Float4 invalid_runtime_pixel = RenderProduction(
      warp, renderer, globals, textures);
  CompareProduction(report, "missing runtime remains color-neutral",
                    invalid_runtime_pixel, default_pixel);

  ReflectedBuffer valid_runtime_globals = globals;
  EnableRuntime(valid_runtime_globals);
  CompareProduction(report, "valid runtime remains color-neutral",
                    RenderProduction(warp, renderer, valid_runtime_globals,
                                     textures),
                    default_pixel);

  ReflectedBuffer interior_globals = valid_runtime_globals;
  SetVariable(interior_globals, "EInteriorFactor", 1.0F);
  CompareProduction(report, "interior runtime remains color-neutral",
                    RenderProduction(warp, renderer, interior_globals,
                                     textures),
                    default_pixel);

  ReflectedBuffer invalid_status_globals = globals;
  SetVariable(invalid_status_globals, "TruthRuntimeStatus",
              Float4{1.0F, 1.0F,
                     std::numeric_limits<float>::quiet_NaN(), 1.0F});
  CompareProduction(report, "non-finite runtime status remains color-neutral",
                    RenderProduction(warp, renderer, invalid_status_globals,
                                     textures),
                    default_pixel);

  ReflectedBuffer unsupported_protocol_globals = valid_runtime_globals;
  SetVariable(unsupported_protocol_globals, "TruthRuntimeStatus",
              Float4{1.5F, 1.0F, 17.0F, 1.0F});
  CompareProduction(report, "undocumented protocol remains color-neutral",
                    RenderProduction(warp, renderer,
                                     unsupported_protocol_globals,
                                     textures),
                    default_pixel);

  const float nan = std::numeric_limits<float>::quiet_NaN();
  const float infinity = std::numeric_limits<float>::infinity();
  for (const bool enabled : {false, true}) {
    for (const float invalid : {nan, infinity, -infinity}) {
      ReflectedBuffer case_globals = globals;
      SetBool(case_globals, "TruthMasterEnabled", enabled);
      ProductionTextures invalid_scene = textures;
      invalid_scene.color.x = invalid;
      ExpectFiniteBounded(
          report,
          enabled ? "enabled non-finite scene" : "disabled non-finite scene",
          RenderProduction(warp, renderer, case_globals, invalid_scene));
    }
  }

  ReflectedBuffer invalid_adaptation_globals = globals;
  SetVariable(invalid_adaptation_globals, "TruthAutoExposureBlend", 1.0F);
  ProductionTextures invalid_adaptation = textures;
  invalid_adaptation.adaptation.x = nan;
  ExpectFiniteBounded(report, "non-finite adaptation",
                      RenderProduction(warp, renderer,
                                       invalid_adaptation_globals,
                                       invalid_adaptation));

  for (const char* parameter : {
           "TruthManualExposureEv", "TruthAutoExposureBlend"}) {
    ReflectedBuffer invalid_parameter_globals = globals;
    SetVariable(invalid_parameter_globals, parameter, nan);
    ExpectFiniteBounded(
        report, std::string{"non-finite UI parameter "} + parameter,
        RenderProduction(warp, renderer, invalid_parameter_globals, textures));
  }
  for (const char* parameter : {
           "TruthSkyReplacementStrength", "TruthSkyDepthThreshold",
           "TruthSkyDepthFeather", "TruthSkyRadianceScale",
           "TruthWeatherDensity", "TruthCloudCoverage", "TruthCloudDensity",
           "TruthFogDensity", "TruthAuroraActivity", "TruthAuroraMask",
           "TruthSkyWindX", "TruthSkyWindY"}) {
    ReflectedBuffer invalid_parameter_globals = valid_runtime_globals;
    SetVariable(invalid_parameter_globals, parameter, nan);
    ExpectFiniteBounded(
        report, std::string{"prepass-owned parameter isolation "} + parameter,
        RenderProduction(warp, renderer, invalid_parameter_globals,
                         textures));
  }
}

}  // namespace

int wmain(const int argument_count, wchar_t* arguments[]) {
  try {
    if (argument_count != 4) {
      std::cerr << "usage: truth_sky_view_adapter_warp_tests "
                   "<TruthSkyViewAdapterProbe.hlsl> "
                   "<TruthRuntimeParametersProbe.hlsl> "
                   "<enbeffect.fx>\n";
      return 2;
    }
    const auto adapter_shader_path =
        std::filesystem::weakly_canonical(arguments[1]);
    const auto runtime_shader_path =
        std::filesystem::weakly_canonical(arguments[2]);
    const auto effect_path =
        std::filesystem::weakly_canonical(arguments[3]);
    if (!std::filesystem::is_regular_file(adapter_shader_path)) {
      Fail("adapter probe shader does not exist");
    }
    if (!std::filesystem::is_regular_file(runtime_shader_path)) {
      Fail("runtime parameter probe shader does not exist");
    }
    if (!std::filesystem::is_regular_file(effect_path)) {
      Fail("production ENB effect does not exist");
    }
    const CompiledProbe adapter_probe = CompileProbe(
        adapter_shader_path, kAdapterEntryPoint);
    const CompiledProbe runtime_probe = CompileProbe(
        runtime_shader_path, kRuntimeEntryPoint);
    const CompiledProbe production_pixel = CompileProductionPixel(effect_path);
    const auto production_vertex = CompileProductionVertex();
    ValidateAdapterReflection(*adapter_probe.reflection.Get());
    ValidateRuntimeReflection(*runtime_probe.reflection.Get());
    ValidateProductionResources(*production_pixel.reflection.Get());
    const ReflectedBuffer production_globals =
        ReflectGlobals(*production_pixel.reflection.Get());
    WarpDevice warp = CreateWarpDevice();
    ProductionRenderer production_renderer = CreateProductionRenderer(
        warp, *production_pixel.bytecode.Get(), *production_vertex.Get());
    Report report;
    const auto cases = Cases();
    for (const TestCase& test_case : cases) {
      CompareCase(report, test_case,
                  Execute(warp, *adapter_probe.bytecode.Get(),
                          test_case.parameters));
    }
    const auto runtime_cases = RuntimeCases();
    for (const RuntimeTestCase& test_case : runtime_cases) {
      CompareRuntimeCase(report, test_case,
                         Execute(warp, *runtime_probe.bytecode.Get(),
                                 test_case.parameters));
    }
    ExerciseProductionPixel(
        report, warp, production_renderer, production_globals);
    if (report.failures != 0U) {
      for (const auto& message : report.messages) {
        std::cerr << "[FAIL] " << message << '\n';
      }
      std::cerr << report.failures << " failures across "
                << report.assertions << " assertions\n";
      return 1;
    }
    std::cout << "Truth sky-view CPU<->D3D11 WARP parity passed: "
              << cases.size() << " direct cases + " << runtime_cases.size()
              << " runtime ABI cases + production ENB pixel cases, "
              << report.assertions
              << " assertions, max_abs_error="
              << report.maximum_absolute_error << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "[FAIL] Truth sky-view WARP harness: " << error.what() << '\n';
    return 1;
  }
}
