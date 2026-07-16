#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <bcrypt.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include "truth/render/ReferenceRenderer.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <fstream>
#include <iomanip>
#include <limits>
#include <mutex>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace truth::render {
namespace {

using Microsoft::WRL::ComPtr;

constexpr std::uint32_t kMinimumDimension = 16U;
constexpr std::uint32_t kMaximumDimension = 8192U;

struct alignas(16) SceneParameters {
  float sun_elevation;
  float weather_density;
  float cloud_coverage;
  float cloud_density;
  float fog_density;
  float aurora_activity;
  float night_factor;
  float phase;
  float exposure_ev;
  float sun_azimuth;
  float padding[2];
  float camera_x;
  float camera_y;
  float camera_z;
  float cloud_type;
};

static_assert(sizeof(SceneParameters) == 64U);

[[nodiscard]] SceneParameters ParametersFor(const ReferenceScene scene) noexcept {
  switch (scene) {
    case ReferenceScene::day:
      return {0.55F, 0.08F, 0.36F, 0.62F, 0.015F, 0.0F, 0.0F,
              0.18F, 1.00F, 0.28F, {0.0F, 0.0F}, 0.0F, 0.0F, 0.20F, 0.42F};
    case ReferenceScene::dusk:
      return {0.015F, 0.18F, 0.42F, 0.58F, 0.035F, 0.18F, 0.34F,
              0.37F, 1.30F, -0.52F, {0.0F, 0.0F}, 0.0F, 0.0F, 0.20F, 0.58F};
    case ReferenceScene::quiet_clear_night:
      return {-0.34F, 0.04F, 0.05F, 0.16F, 0.008F, 0.18F, 1.0F,
              0.63F, 1.42F, 0.12F, {0.0F, 0.0F}, 0.0F, 0.0F, 0.20F, 0.64F};
    case ReferenceScene::active_clear_night:
      return {-0.34F, 0.04F, 0.05F, 0.16F, 0.008F, 0.82F, 1.0F,
              0.63F, 1.42F, 0.12F, {0.0F, 0.0F}, 0.0F, 0.0F, 0.20F, 0.64F};
    case ReferenceScene::cloudy_night_aurora:
      return {-0.34F, 0.48F, 0.72F, 0.80F, 0.018F, 0.82F, 1.0F,
              0.63F, 1.30F, 0.12F, {0.0F, 0.0F}, 0.0F, 0.0F, 0.20F, 0.72F};
    case ReferenceScene::storm:
      return {0.18F, 0.88F, 0.52F, 0.96F, 0.065F, 0.0F, 0.04F,
              0.42F, 0.88F, 0.74F, {0.0F, 0.0F}, 0.0F, 0.0F, 0.20F, 0.88F};
    case ReferenceScene::translated_day_probe:
      return {0.55F, 0.08F, 0.36F, 0.62F, 0.015F, 0.0F, 0.0F,
              0.18F, 1.00F, 0.28F, {0.0F, 0.0F}, 0.65F, -0.35F, 0.20F, 0.42F};
  }
  return {};
}

[[nodiscard]] std::string HexCode(const std::uint32_t code) {
  std::ostringstream stream;
  stream << "0x" << std::uppercase << std::hex << std::setw(8)
         << std::setfill('0') << code;
  return stream.str();
}

[[nodiscard]] bool CompileShader(
    const std::filesystem::path& shader_path,
    const char* entry_point,
    const char* target,
    ComPtr<ID3DBlob>& bytecode,
    std::string& diagnostic,
    const char* const define_name = nullptr,
    const char* const define_value = nullptr) {
  struct CacheEntry {
    std::filesystem::path path;
    std::string entry_point;
    std::string target;
    std::string define_name;
    std::string define_value;
    ComPtr<ID3DBlob> bytecode;
  };
  static std::mutex cache_mutex;
  static std::vector<CacheEntry> cache;

  const auto normalized_path = std::filesystem::absolute(shader_path).lexically_normal();
  {
    const std::scoped_lock lock{cache_mutex};
    for (const auto& entry : cache) {
      if (entry.path == normalized_path
          && entry.entry_point == entry_point
          && entry.target == target
          && entry.define_name == (define_name == nullptr ? "" : define_name)
          && entry.define_value == (define_value == nullptr ? "" : define_value)) {
        bytecode = entry.bytecode;
        return true;
      }
    }
  }

  ComPtr<ID3DBlob> errors;
  std::array<D3D_SHADER_MACRO, 2U> macros{};
  const D3D_SHADER_MACRO* macro_pointer{};
  if (define_name != nullptr && define_value != nullptr) {
    macros[0] = {define_name, define_value};
    macros[1] = {nullptr, nullptr};
    macro_pointer = macros.data();
  }
  constexpr UINT flags = D3DCOMPILE_ENABLE_STRICTNESS
      | D3DCOMPILE_WARNINGS_ARE_ERRORS
      | D3DCOMPILE_IEEE_STRICTNESS
      | D3DCOMPILE_OPTIMIZATION_LEVEL1;
  const HRESULT result = D3DCompileFromFile(
      normalized_path.c_str(),
      macro_pointer,
      D3D_COMPILE_STANDARD_FILE_INCLUDE,
      entry_point,
      target,
      flags,
      0U,
      &bytecode,
      &errors);
  if (SUCCEEDED(result)) {
    const std::scoped_lock lock{cache_mutex};
    cache.push_back({
        normalized_path,
        entry_point,
        target,
        define_name == nullptr ? "" : define_name,
        define_value == nullptr ? "" : define_value,
        bytecode,
    });
    return true;
  }

  diagnostic = "D3DCompileFromFile failed for ";
  diagnostic += shader_path.string();
  diagnostic += " (";
  diagnostic += entry_point;
  diagnostic += ", ";
  diagnostic += target;
  diagnostic += ", HRESULT=";
  diagnostic += HexCode(static_cast<std::uint32_t>(result));
  diagnostic += ")";
  if (errors != nullptr && errors->GetBufferSize() != 0U) {
    diagnostic += ": ";
    diagnostic.append(
        static_cast<const char*>(errors->GetBufferPointer()),
        errors->GetBufferSize());
  }
  return false;
}

[[nodiscard]] std::string Sha256Hex(
    const std::vector<std::uint8_t>& bytes,
    std::string& diagnostic) {
  BCRYPT_ALG_HANDLE algorithm{};
  BCRYPT_HASH_HANDLE hash{};
  std::vector<UCHAR> hash_object;
  std::array<UCHAR, 32> digest{};

  const auto close_handles = [&]() noexcept {
    if (hash != nullptr) {
      BCryptDestroyHash(hash);
    }
    if (algorithm != nullptr) {
      BCryptCloseAlgorithmProvider(algorithm, 0U);
    }
  };

  NTSTATUS status = BCryptOpenAlgorithmProvider(
      &algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0U);
  if (!BCRYPT_SUCCESS(status)) {
    diagnostic = "BCryptOpenAlgorithmProvider(SHA-256) failed: "
        + HexCode(static_cast<std::uint32_t>(status));
    close_handles();
    return {};
  }

  DWORD object_size{};
  DWORD result_size{};
  status = BCryptGetProperty(
      algorithm,
      BCRYPT_OBJECT_LENGTH,
      reinterpret_cast<PUCHAR>(&object_size),
      sizeof(object_size),
      &result_size,
      0U);
  if (!BCRYPT_SUCCESS(status) || result_size != sizeof(object_size)) {
    diagnostic = "BCryptGetProperty(SHA-256 object length) failed: "
        + HexCode(static_cast<std::uint32_t>(status));
    close_handles();
    return {};
  }
  hash_object.resize(object_size);

  status = BCryptCreateHash(
      algorithm,
      &hash,
      hash_object.data(),
      static_cast<ULONG>(hash_object.size()),
      nullptr,
      0U,
      0U);
  if (!BCRYPT_SUCCESS(status)) {
    diagnostic = "BCryptCreateHash(SHA-256) failed: "
        + HexCode(static_cast<std::uint32_t>(status));
    close_handles();
    return {};
  }
  if (bytes.size() > std::numeric_limits<ULONG>::max()) {
    diagnostic = "GPU readback was too large for the SHA-256 provider";
    close_handles();
    return {};
  }

  status = BCryptHashData(
      hash,
      const_cast<PUCHAR>(reinterpret_cast<const UCHAR*>(bytes.data())),
      static_cast<ULONG>(bytes.size()),
      0U);
  if (BCRYPT_SUCCESS(status)) {
    status = BCryptFinishHash(
        hash, digest.data(), static_cast<ULONG>(digest.size()), 0U);
  }
  if (!BCRYPT_SUCCESS(status)) {
    diagnostic = "BCrypt SHA-256 digest failed: "
        + HexCode(static_cast<std::uint32_t>(status));
    close_handles();
    return {};
  }

  std::ostringstream stream;
  stream << std::hex << std::setfill('0');
  for (const auto value : digest) {
    stream << std::setw(2) << static_cast<unsigned int>(value);
  }
  close_handles();
  return stream.str();
}

[[nodiscard]] ReferenceRenderResult Fail(
    const ReferenceRenderStatus status,
    std::string diagnostic) {
  ReferenceRenderResult result;
  result.status = status;
  result.diagnostic = std::move(diagnostic);
  return result;
}

}  // namespace

std::string_view ReferenceSceneName(const ReferenceScene scene) noexcept {
  switch (scene) {
    case ReferenceScene::day:
      return "day";
    case ReferenceScene::dusk:
      return "dusk";
    case ReferenceScene::quiet_clear_night:
      return "quiet-clear-night";
    case ReferenceScene::active_clear_night:
      return "active-clear-night";
    case ReferenceScene::cloudy_night_aurora:
      return "cloudy-night-aurora";
    case ReferenceScene::storm:
      return "storm";
    case ReferenceScene::translated_day_probe:
      return "translated-day-probe";
  }
  return {};
}

[[nodiscard]] ReferenceRenderResult RenderWarpPass(
    const ReferenceScene scene,
    const std::filesystem::path& shader_path,
    const std::uint32_t width,
    const std::uint32_t height,
    const char* const pixel_entry,
    const char* const aurora_quality_define = nullptr) noexcept {
  try {
    const auto start_time = std::chrono::steady_clock::now();
    if (ReferenceSceneName(scene).empty()) {
      return Fail(ReferenceRenderStatus::invalid_request, "unknown reference scene");
    }
    if (width < kMinimumDimension || width > kMaximumDimension
        || height < kMinimumDimension || height > kMaximumDimension) {
      return Fail(ReferenceRenderStatus::invalid_request,
                  "reference dimensions must be between 16 and 8192 pixels");
    }

    ComPtr<ID3DBlob> vertex_bytecode;
    ComPtr<ID3DBlob> pixel_bytecode;
    std::string diagnostic;
    if (!CompileShader(shader_path, "TruthReferenceVertexMain", "vs_5_0",
                       vertex_bytecode, diagnostic)) {
      return Fail(ReferenceRenderStatus::shader_compile_failed, std::move(diagnostic));
    }
    if (!CompileShader(shader_path, pixel_entry, "ps_5_0",
                       pixel_bytecode, diagnostic,
                       aurora_quality_define == nullptr
                           ? nullptr
                           : "TRUTH_AURORA_QUALITY",
                       aurora_quality_define)) {
      return Fail(ReferenceRenderStatus::shader_compile_failed, std::move(diagnostic));
    }
    const auto shader_compile_end_time = std::chrono::steady_clock::now();
    const auto render_start_time = shader_compile_end_time;

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL feature_level{};
    constexpr D3D_FEATURE_LEVEL requested_levels[]{D3D_FEATURE_LEVEL_11_0};
    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_WARP,
        nullptr,
        D3D11_CREATE_DEVICE_SINGLETHREADED,
        requested_levels,
        static_cast<UINT>(std::size(requested_levels)),
        D3D11_SDK_VERSION,
        &device,
        &feature_level,
        &context);
    if (FAILED(hr) || feature_level != D3D_FEATURE_LEVEL_11_0) {
      return Fail(ReferenceRenderStatus::device_failed,
                  "D3D11CreateDevice(WARP) failed: "
                      + HexCode(static_cast<std::uint32_t>(hr)));
    }

    ComPtr<ID3D11VertexShader> vertex_shader;
    ComPtr<ID3D11PixelShader> pixel_shader;
    hr = device->CreateVertexShader(
        vertex_bytecode->GetBufferPointer(),
        vertex_bytecode->GetBufferSize(),
        nullptr,
        &vertex_shader);
    if (SUCCEEDED(hr)) {
      hr = device->CreatePixelShader(
          pixel_bytecode->GetBufferPointer(),
          pixel_bytecode->GetBufferSize(),
          nullptr,
          &pixel_shader);
    }
    if (FAILED(hr)) {
      return Fail(ReferenceRenderStatus::gpu_failed,
                  "D3D11 shader creation failed: "
                      + HexCode(static_cast<std::uint32_t>(hr)));
    }

    D3D11_TEXTURE2D_DESC render_description{};
    render_description.Width = width;
    render_description.Height = height;
    render_description.MipLevels = 1U;
    render_description.ArraySize = 1U;
    render_description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    render_description.SampleDesc.Count = 1U;
    render_description.Usage = D3D11_USAGE_DEFAULT;
    render_description.BindFlags = D3D11_BIND_RENDER_TARGET;

    ComPtr<ID3D11Texture2D> render_texture;
    ComPtr<ID3D11RenderTargetView> render_target;
    hr = device->CreateTexture2D(&render_description, nullptr, &render_texture);
    if (SUCCEEDED(hr)) {
      hr = device->CreateRenderTargetView(render_texture.Get(), nullptr, &render_target);
    }
    if (FAILED(hr)) {
      return Fail(ReferenceRenderStatus::gpu_failed,
                  "D3D11 offscreen target creation failed: "
                      + HexCode(static_cast<std::uint32_t>(hr)));
    }

    D3D11_TEXTURE2D_DESC staging_description = render_description;
    staging_description.Usage = D3D11_USAGE_STAGING;
    staging_description.BindFlags = 0U;
    staging_description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging_texture;
    hr = device->CreateTexture2D(&staging_description, nullptr, &staging_texture);
    if (FAILED(hr)) {
      return Fail(ReferenceRenderStatus::gpu_failed,
                  "D3D11 staging target creation failed: "
                      + HexCode(static_cast<std::uint32_t>(hr)));
    }

    D3D11_BUFFER_DESC parameter_description{};
    parameter_description.ByteWidth = sizeof(SceneParameters);
    parameter_description.Usage = D3D11_USAGE_DEFAULT;
    parameter_description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    const auto parameters = ParametersFor(scene);
    D3D11_SUBRESOURCE_DATA parameter_data{};
    parameter_data.pSysMem = &parameters;
    ComPtr<ID3D11Buffer> parameter_buffer;
    hr = device->CreateBuffer(
        &parameter_description, &parameter_data, &parameter_buffer);
    if (FAILED(hr)) {
      return Fail(ReferenceRenderStatus::gpu_failed,
                  "D3D11 scene constant-buffer creation failed: "
                      + HexCode(static_cast<std::uint32_t>(hr)));
    }

    ID3D11RenderTargetView* targets[]{render_target.Get()};
    ID3D11Buffer* buffers[]{parameter_buffer.Get()};
    constexpr FLOAT clear_color[]{0.0F, 0.0F, 0.0F, 1.0F};
    const D3D11_VIEWPORT viewport{
        0.0F, 0.0F, static_cast<FLOAT>(width), static_cast<FLOAT>(height), 0.0F, 1.0F};
    context->ClearRenderTargetView(render_target.Get(), clear_color);
    context->OMSetRenderTargets(1U, targets, nullptr);
    context->RSSetViewports(1U, &viewport);
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(vertex_shader.Get(), nullptr, 0U);
    context->PSSetShader(pixel_shader.Get(), nullptr, 0U);
    context->PSSetConstantBuffers(0U, 1U, buffers);
    context->Draw(3U, 0U);
    context->CopyResource(staging_texture.Get(), render_texture.Get());

    D3D11_MAPPED_SUBRESOURCE mapped{};
    hr = context->Map(staging_texture.Get(), 0U, D3D11_MAP_READ, 0U, &mapped);
    if (FAILED(hr)) {
      return Fail(ReferenceRenderStatus::readback_failed,
                  "D3D11 WARP readback map failed: "
                      + HexCode(static_cast<std::uint32_t>(hr)));
    }

    ReferenceImage image;
    image.width = width;
    image.height = height;
    const auto row_size = static_cast<std::size_t>(width) * 4U;
    image.rgba8.resize(row_size * height);
    const auto* source = static_cast<const std::byte*>(mapped.pData);
    for (std::uint32_t row = 0; row < height; ++row) {
      const auto* source_row = source + static_cast<std::size_t>(mapped.RowPitch) * row;
      auto* destination_row = image.rgba8.data() + row_size * row;
      std::memcpy(destination_row, source_row, row_size);
    }
    context->Unmap(staging_texture.Get(), 0U);

    diagnostic.clear();
    const auto hash = Sha256Hex(image.rgba8, diagnostic);
    if (hash.empty()) {
      return Fail(ReferenceRenderStatus::readback_failed, std::move(diagnostic));
    }

    ReferenceRenderResult result;
    result.status = ReferenceRenderStatus::rendered;
    result.image = std::move(image);
    result.sha256_hex = hash;
    const auto end_time = std::chrono::steady_clock::now();
    result.shader_compile_milliseconds = std::chrono::duration<double, std::milli>(
        shader_compile_end_time - start_time).count();
    result.render_milliseconds = std::chrono::duration<double, std::milli>(
        end_time - render_start_time).count();
    result.elapsed_milliseconds = std::chrono::duration<double, std::milli>(
        end_time - start_time).count();
    return result;
  } catch (const std::exception& exception) {
    return Fail(ReferenceRenderStatus::gpu_failed,
                std::string{"WARP renderer exception: "} + exception.what());
  } catch (...) {
    return Fail(ReferenceRenderStatus::gpu_failed,
                "WARP renderer encountered an unknown exception");
  }
}

[[nodiscard]] const char* AuroraQualityDefine(
    const AuroraQuality quality) noexcept {
  switch (quality) {
    case AuroraQuality::fallback:
      return "0";
    case AuroraQuality::low:
      return "1";
    case AuroraQuality::balanced:
      return "2";
    case AuroraQuality::high:
      return "3";
  }
  return nullptr;
}

ReferenceRenderResult RenderWarpReference(
    const ReferenceScene scene,
    const std::filesystem::path& shader_path,
    const std::uint32_t width,
    const std::uint32_t height) noexcept {
  return RenderWarpPass(
      scene, shader_path, width, height, "TruthReferencePixelMain");
}

ReferenceRenderResult RenderWarpSkyFieldScalars(
    const ReferenceScene scene,
    const std::filesystem::path& shader_path,
    const std::uint32_t width,
    const std::uint32_t height) noexcept {
  return RenderWarpPass(
      scene, shader_path, width, height, "TruthSkyFieldScalarProbePixelMain");
}

ReferenceRenderResult RenderWarpSkyFieldScalars(
    const ReferenceScene scene,
    const std::filesystem::path& shader_path,
    const std::uint32_t width,
    const std::uint32_t height,
    const AuroraQuality quality) noexcept {
  const char* const define = AuroraQualityDefine(quality);
  if (define == nullptr) {
    return Fail(ReferenceRenderStatus::invalid_request,
                "unknown aurora quality tier");
  }
  return RenderWarpPass(scene,
                        shader_path,
                        width,
                        height,
                        "TruthSkyFieldScalarProbePixelMain",
                        define);
}

ReferenceRenderResult RenderWarpSkyFieldRadiance(
    const ReferenceScene scene,
    const std::filesystem::path& shader_path,
    const std::uint32_t width,
    const std::uint32_t height) noexcept {
  return RenderWarpPass(
      scene, shader_path, width, height, "TruthSkyFieldRadianceProbePixelMain");
}

ReferenceRenderResult RenderWarpSkyFieldRadiance(
    const ReferenceScene scene,
    const std::filesystem::path& shader_path,
    const std::uint32_t width,
    const std::uint32_t height,
    const AuroraQuality quality) noexcept {
  const char* const define = AuroraQualityDefine(quality);
  if (define == nullptr) {
    return Fail(ReferenceRenderStatus::invalid_request,
                "unknown aurora quality tier");
  }
  return RenderWarpPass(scene,
                        shader_path,
                        width,
                        height,
                        "TruthSkyFieldRadianceProbePixelMain",
                        define);
}

ReferenceRenderResult RenderWarpCloudVolumeScalars(
    const ReferenceScene scene,
    const std::filesystem::path& shader_path,
    const std::uint32_t width,
    const std::uint32_t height) noexcept {
  return RenderWarpPass(
      scene, shader_path, width, height, "TruthCloudVolumeScalarProbePixelMain");
}

ReferenceRenderResult RenderWarpCloudVolumeRadiance(
    const ReferenceScene scene,
    const std::filesystem::path& shader_path,
    const std::uint32_t width,
    const std::uint32_t height) noexcept {
  return RenderWarpPass(
      scene, shader_path, width, height, "TruthCloudVolumeRadianceProbePixelMain");
}

bool WriteBinaryPpm(
    const ReferenceImage& image,
    const std::filesystem::path& output_path,
    std::string& diagnostic) noexcept {
  try {
    const auto expected_size = static_cast<std::size_t>(image.width)
        * image.height * 4U;
    if (image.width == 0U || image.height == 0U
        || image.rgba8.size() != expected_size) {
      diagnostic = "PPM source image dimensions do not match its RGBA8 bytes";
      return false;
    }
    if (output_path.has_parent_path()) {
      std::error_code error;
      std::filesystem::create_directories(output_path.parent_path(), error);
      if (error) {
        diagnostic = "could not create PPM output directory: " + error.message();
        return false;
      }
    }

    std::ofstream output{output_path, std::ios::binary | std::ios::trunc};
    if (!output) {
      diagnostic = "could not open PPM output: " + output_path.string();
      return false;
    }
    output << "P6\n" << image.width << ' ' << image.height << "\n255\n";
    for (std::size_t offset = 0; offset < image.rgba8.size(); offset += 4U) {
      const std::array<char, 3> rgb{
          static_cast<char>(image.rgba8[offset]),
          static_cast<char>(image.rgba8[offset + 1U]),
          static_cast<char>(image.rgba8[offset + 2U]),
      };
      output.write(rgb.data(), static_cast<std::streamsize>(rgb.size()));
    }
    output.flush();
    if (!output) {
      diagnostic = "failed while writing PPM output: " + output_path.string();
      return false;
    }
    diagnostic.clear();
    return true;
  } catch (const std::exception& exception) {
    diagnostic = std::string{"PPM writer exception: "} + exception.what();
    return false;
  } catch (...) {
    diagnostic = "PPM writer encountered an unknown exception";
    return false;
  }
}

}  // namespace truth::render
