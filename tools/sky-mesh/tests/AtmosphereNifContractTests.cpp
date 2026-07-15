// SPDX-License-Identifier: GPL-3.0-or-later

#include <NifFile.hpp>
#include <Shaders.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace fs = std::filesystem;
using namespace nifly;

namespace {

constexpr std::size_t kExpectedVertices = 2562;
constexpr std::size_t kExpectedTriangles = 5120;
constexpr float kExpectedRadius = 500.0F;
constexpr std::uint32_t kAlwaysDrawFlags = 0x0008000EU;

[[noreturn]] void fail(const std::string& message) {
    throw std::runtime_error(message);
}

void require(const bool condition, const std::string& message) {
    if (!condition) {
        fail(message);
    }
}

bool finite(const Vector3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool finite(const Color4& value) {
    return std::isfinite(value.r) && std::isfinite(value.g)
        && std::isfinite(value.b) && std::isfinite(value.a);
}

float length(const Vector3& value) {
    return std::sqrt(value.dot(value));
}

std::vector<std::uint8_t> readBytes(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    require(static_cast<bool>(input), "cannot read file: " + path.string());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string readText(const fs::path& path) {
    const auto bytes = readBytes(path);
    return {bytes.begin(), bytes.end()};
}

std::string sha256(const fs::path& path) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD objectSize = 0;
    DWORD hashSize = 0;
    DWORD received = 0;

    auto check = [](const NTSTATUS status, const std::string_view operation) {
        if (status < 0) {
            fail(std::string(operation) + " failed with NTSTATUS " + std::to_string(status));
        }
    };

    check(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0),
          "BCryptOpenAlgorithmProvider");
    check(BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                            reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize), &received, 0),
          "BCryptGetProperty(object length)");
    check(BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH,
                            reinterpret_cast<PUCHAR>(&hashSize), sizeof(hashSize), &received, 0),
          "BCryptGetProperty(hash length)");

    std::vector<std::uint8_t> object(objectSize);
    std::vector<std::uint8_t> digest(hashSize);
    check(BCryptCreateHash(algorithm, &hash, object.data(), objectSize, nullptr, 0, 0),
          "BCryptCreateHash");

    std::ifstream input(path, std::ios::binary);
    require(static_cast<bool>(input), "cannot hash file: " + path.string());
    std::array<char, 64 * 1024> chunk{};
    while (input) {
        input.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
        const auto count = input.gcount();
        if (count > 0) {
            check(BCryptHashData(hash, reinterpret_cast<PUCHAR>(chunk.data()),
                                 static_cast<ULONG>(count), 0),
                  "BCryptHashData");
        }
    }
    check(BCryptFinishHash(hash, digest.data(), hashSize, 0), "BCryptFinishHash");

    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);

    std::ostringstream result;
    result << std::hex << std::setfill('0');
    for (const auto byte : digest) {
        result << std::setw(2) << static_cast<unsigned int>(byte);
    }
    return result.str();
}

std::wstring quoteArgument(const fs::path& path) {
    std::wstring result = L"\"";
    for (const wchar_t character : path.wstring()) {
        if (character == L'\"') {
            result += L"\\\"";
        } else {
            result += character;
        }
    }
    result += L"\"";
    return result;
}

DWORD runProcess(const fs::path& executable, const std::wstring& arguments) {
    require(fs::exists(executable), "generator executable is missing: " + executable.string());

    std::wstring commandLine = quoteArgument(executable) + L" " + arguments;
    std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
    mutableCommand.push_back(L'\0');

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    const BOOL created = CreateProcessW(
        executable.c_str(), mutableCommand.data(), nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);
    require(created != FALSE,
            "CreateProcessW failed for generator with Win32 error " + std::to_string(GetLastError()));

    const DWORD waitResult = WaitForSingleObject(process.hProcess, 60'000U);
    if (waitResult != WAIT_OBJECT_0) {
        TerminateProcess(process.hProcess, 1U);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        fail("generator did not complete within 60 seconds");
    }

    DWORD exitCode = 0;
    require(GetExitCodeProcess(process.hProcess, &exitCode) != FALSE,
            "GetExitCodeProcess failed for generator");
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return exitCode;
}

void clearKnownOutputs(const fs::path& outputRoot) {
    const std::array<fs::path, 3> outputs{
        outputRoot / "meshes" / "sky" / "atmosphere.nif",
        outputRoot / "truth-atmosphere-mesh.manifest.json",
        outputRoot / "truth-atmosphere-mesh.obj"};
    for (const auto& output : outputs) {
        std::error_code error;
        fs::remove(output, error);
        require(!error, "cannot clear known test output " + output.string() + ": " + error.message());
    }
}

void generate(const fs::path& executable, const fs::path& outputRoot, const bool emitObj) {
    clearKnownOutputs(outputRoot);

    std::wstring arguments = L"--output-root " + quoteArgument(outputRoot);
    if (emitObj) {
        arguments += L" --emit-obj";
    }
    require(runProcess(executable, arguments) == 0, "generator returned a non-zero exit code");
}

std::string jsonString(const std::string& document, const std::string& key) {
    const std::string prefix = "\"" + key + "\": \"";
    const auto start = document.find(prefix);
    require(start != std::string::npos, "manifest is missing string key: " + key);
    const auto valueStart = start + prefix.size();
    const auto end = document.find('"', valueStart);
    require(end != std::string::npos, "manifest has an unterminated value: " + key);
    return document.substr(valueStart, end - valueStart);
}

std::uint64_t jsonUnsigned(const std::string& document, const std::string& key) {
    const std::string prefix = "\"" + key + "\": ";
    const auto start = document.find(prefix);
    require(start != std::string::npos, "manifest is missing numeric key: " + key);
    const auto valueStart = start + prefix.size();
    std::size_t consumed = 0;
    const auto value = std::stoull(document.substr(valueStart), &consumed);
    require(consumed > 0, "manifest contains an invalid numeric value: " + key);
    return value;
}

void verifyManifest(const fs::path& manifestPath, const fs::path& nifPath) {
    const std::string manifest = readText(manifestPath);
    require(jsonUnsigned(manifest, "schema_version") == 1, "manifest schema version mismatch");
    require(jsonString(manifest, "asset_path") == "meshes/sky/atmosphere.nif",
            "manifest asset path mismatch");
    require(jsonString(manifest, "geometry") == "original deterministic inward Icosphere L4",
            "manifest geometry provenance mismatch");
    require(jsonString(manifest, "generator_license") == "GPL-3.0-or-later",
            "manifest generator license mismatch");
    require(manifest.find("\"template_source\": null") != std::string::npos,
            "manifest must state that no source template was used");
    require(jsonString(manifest, "nifly_revision").size() == 40,
            "manifest must record a full nifly revision");
    require(jsonString(manifest, "sha256") == sha256(nifPath), "manifest SHA-256 mismatch");
    require(jsonUnsigned(manifest, "size_bytes") == fs::file_size(nifPath),
            "manifest size mismatch");
    require(jsonUnsigned(manifest, "subdivision_level") == 4,
            "manifest subdivision level mismatch");
    require(jsonUnsigned(manifest, "vertices") == kExpectedVertices,
            "manifest vertex count mismatch");
    require(jsonUnsigned(manifest, "triangles") == kExpectedTriangles,
            "manifest triangle count mismatch");
    require(jsonUnsigned(manifest, "uv_channels") == 0, "manifest UV count mismatch");
}

void verifyNif(const fs::path& nifPath) {
    NifFile nif;
    require(nif.Load(nifPath) == 0, "nifly could not parse the generated NIF");
    require(nif.IsValid(), "generated NIF is not valid");
    require(!nif.HasUnknown(), "generated NIF contains unknown blocks");

    const auto& header = nif.GetHeader();
    const auto& version = header.GetVersion();
    require(version.File() == NiFileVersion::V20_2_0_7, "NIF file version mismatch");
    require(version.User() == 12, "NIF user version mismatch");
    require(version.Stream() == 100, "NIF stream version mismatch");
    require(header.GetNumBlocks() == 3, "NIF must contain exactly root, shape, and sky shader blocks");

    const auto nodes = nif.GetNodes();
    const auto shapes = nif.GetShapes();
    require(nodes.size() == 1, "NIF must contain exactly one node");
    require(shapes.size() == 1, "NIF must contain exactly one shape");
    require(nodes.front()->name.get() == "Scene", "root node name mismatch");
    require(nodes.front()->flags == kAlwaysDrawFlags, "root node flags mismatch");

    auto* shape = dynamic_cast<BSTriShape*>(shapes.front());
    require(shape != nullptr, "atmosphere shape must be a BSTriShape");
    require(shape->name.get() == "TruthAtmosphereDome", "atmosphere shape name mismatch");
    require(shape->flags == kAlwaysDrawFlags, "atmosphere shape flags mismatch");

    auto* sky = dynamic_cast<BSSkyShaderProperty*>(nif.GetShader(shape));
    require(sky != nullptr, "atmosphere shape must use BSSkyShaderProperty");
    require(sky->skyFlags == 2U, "BSSkyShaderProperty sky type must be 2 (atmosphere)");
    require(sky->baseTexture.get().empty(), "atmosphere fallback must not reference a texture");
    require(sky->shaderFlags1 == SLSF1_ZBUFFER_TEST, "sky shader must enable only Z-test in flags1");
    require(sky->shaderFlags2 == (SLSF2_ZBUFFER_WRITE | SLSF2_VERTEX_COLORS),
            "sky shader must enable Z-write and vertex colors in flags2");

    const auto* vertices = nif.GetVertsForShape(shape);
    const auto* normals = nif.GetNormalsForShape(shape);
    const auto* uvs = nif.GetUvsForShape(shape);
    const auto* colors = nif.GetColorsForShape(shape);
    std::vector<Triangle> triangles;
    shape->GetTriangles(triangles);

    require(vertices != nullptr && vertices->size() == kExpectedVertices, "vertex count mismatch");
    require(normals != nullptr && normals->size() == kExpectedVertices, "normal count mismatch");
    require((uvs == nullptr || uvs->empty()) && !shape->HasUVs(), "mesh must have no UV channel");
    require(colors != nullptr && colors->size() == kExpectedVertices, "vertex color count mismatch");
    require(triangles.size() == kExpectedTriangles, "triangle count mismatch");

    const auto bounds = shape->GetBounds();
    require(length(bounds.center) <= 1.0e-6F, "bounding sphere must be centered at the origin");
    require(std::abs(bounds.radius - kExpectedRadius) <= 1.0e-6F,
            "bounding sphere radius must be exactly 500");

    std::set<std::tuple<std::int64_t, std::int64_t, std::int64_t>> uniquePositions;
    std::map<std::int64_t, std::array<std::uint8_t, 4>> colorByElevation;
    std::set<std::array<std::uint8_t, 4>> uniqueColors;
    float minAlpha = 1.0F;
    float maxAlpha = 0.0F;

    for (std::size_t index = 0; index < vertices->size(); ++index) {
        const auto& vertex = vertices->at(index);
        const auto& normal = normals->at(index);
        const auto& color = colors->at(index);
        require(finite(vertex), "vertex contains a non-finite component");
        require(finite(normal), "normal contains a non-finite component");
        require(finite(color), "vertex color contains a non-finite component");

        const float radialDistance = length(vertex);
        require(std::abs(radialDistance - kExpectedRadius) <= 0.001F,
                "vertex is outside the radius-500 quantization tolerance");
        require(std::abs(vertex.x * 4096.0F - std::round(vertex.x * 4096.0F)) <= 1.0e-5F
                    && std::abs(vertex.y * 4096.0F - std::round(vertex.y * 4096.0F)) <= 1.0e-5F
                    && std::abs(vertex.z * 4096.0F - std::round(vertex.z * 4096.0F)) <= 1.0e-5F,
                "vertex position is not quantized to the 1/4096 grid");

        const float normalLength = length(normal);
        require(normalLength >= 0.985F && normalLength <= 1.015F,
                "packed normal is not normalized within 8-bit tolerance");
        require((normal / normalLength).dot(vertex / radialDistance) <= -0.999F,
                "vertex normal is not inward-facing");

        const auto qx = std::llround(vertex.x * 4096.0F);
        const auto qy = std::llround(vertex.y * 4096.0F);
        const auto qz = std::llround(vertex.z * 4096.0F);
        uniquePositions.emplace(qx, qy, qz);

        const std::array<std::uint8_t, 4> quantizedColor{
            static_cast<std::uint8_t>(std::lround(color.r * 255.0F)),
            static_cast<std::uint8_t>(std::lround(color.g * 255.0F)),
            static_cast<std::uint8_t>(std::lround(color.b * 255.0F)),
            static_cast<std::uint8_t>(std::lround(color.a * 255.0F))};
        require(std::abs(color.r * 255.0F - std::round(color.r * 255.0F)) <= 1.0e-4F
                    && std::abs(color.g * 255.0F - std::round(color.g * 255.0F)) <= 1.0e-4F
                    && std::abs(color.b * 255.0F - std::round(color.b * 255.0F)) <= 1.0e-4F
                    && std::abs(color.a * 255.0F - std::round(color.a * 255.0F)) <= 1.0e-4F,
                "vertex color is not 8-bit quantized");
        require(std::abs((color.r + color.g + color.b) - 1.0F) <= (2.1F / 255.0F),
                "elevation blend weights do not sum to one");
        const auto [existing, inserted] = colorByElevation.emplace(qz, quantizedColor);
        require(inserted || existing->second == quantizedColor,
                "equal elevations must have equal deterministic colors");
        uniqueColors.insert(quantizedColor);
        minAlpha = std::min(minAlpha, color.a);
        maxAlpha = std::max(maxAlpha, color.a);
    }

    require(uniquePositions.size() == kExpectedVertices,
            "welded Icosphere must not duplicate positions or create seams");
    require(uniqueColors.size() >= 32, "elevation function must produce a meaningful color ramp");
    require(minAlpha <= 0.01F && maxAlpha >= 0.95F,
            "elevation function must produce a meaningful alpha ramp");

    std::map<std::pair<std::uint16_t, std::uint16_t>, std::uint32_t> edgeUse;
    std::set<std::array<std::uint16_t, 3>> uniqueTriangles;
    for (const auto& triangle : triangles) {
        const std::array<std::uint16_t, 3> indices{triangle.p1, triangle.p2, triangle.p3};
        require(indices[0] < vertices->size() && indices[1] < vertices->size()
                    && indices[2] < vertices->size(),
                "triangle contains an out-of-range index");
        require(indices[0] != indices[1] && indices[1] != indices[2] && indices[2] != indices[0],
                "triangle repeats a vertex index");

        auto canonical = indices;
        std::sort(canonical.begin(), canonical.end());
        require(uniqueTriangles.insert(canonical).second, "mesh contains a duplicate triangle");

        for (std::size_t edge = 0; edge < 3; ++edge) {
            auto first = indices[edge];
            auto second = indices[(edge + 1) % 3];
            if (first > second) {
                std::swap(first, second);
            }
            ++edgeUse[{first, second}];
        }

        const auto& a = vertices->at(indices[0]);
        const auto& b = vertices->at(indices[1]);
        const auto& c = vertices->at(indices[2]);
        const Vector3 faceNormal = (b - a).cross(c - a);
        const Vector3 centroid = (a + b + c) / 3.0F;
        require(faceNormal.dot(faceNormal) > 1.0e-6F, "mesh contains a degenerate triangle");
        require(faceNormal.dot(centroid) < 0.0F, "triangle winding is not inward-facing");
    }

    require(edgeUse.size() == 7680, "closed Icosphere edge count mismatch");
    require(std::all_of(edgeUse.begin(), edgeUse.end(),
                        [](const auto& edge) { return edge.second == 2U; }),
            "Icosphere must be a closed two-manifold");
    require(static_cast<std::int64_t>(vertices->size())
                - static_cast<std::int64_t>(edgeUse.size())
                + static_cast<std::int64_t>(triangles.size()) == 2,
            "Icosphere Euler characteristic mismatch");
}

void contract(const fs::path& executable, const fs::path& outputRoot) {
    generate(executable, outputRoot, false);
    const fs::path nifPath = outputRoot / "meshes" / "sky" / "atmosphere.nif";
    const fs::path manifestPath = outputRoot / "truth-atmosphere-mesh.manifest.json";
    require(fs::is_regular_file(nifPath), "generator did not emit atmosphere.nif");
    require(fs::is_regular_file(manifestPath), "generator did not emit the provenance manifest");
    require(!fs::exists(outputRoot / "truth-atmosphere-mesh.obj"),
            "OBJ diagnostic must be opt-in");
    verifyNif(nifPath);
    verifyManifest(manifestPath, nifPath);
}

void determinism(const fs::path& executable, const fs::path& outputRoot) {
    const fs::path first = outputRoot / "first";
    const fs::path second = outputRoot / "second";
    generate(executable, first, true);
    generate(executable, second, true);

    const std::array<fs::path, 3> relativeOutputs{
        fs::path("meshes") / "sky" / "atmosphere.nif",
        "truth-atmosphere-mesh.manifest.json",
        "truth-atmosphere-mesh.obj"};
    for (const auto& relative : relativeOutputs) {
        require(readBytes(first / relative) == readBytes(second / relative),
                "two clean generations differ: " + relative.string());
    }

    verifyNif(first / relativeOutputs[0]);
    verifyManifest(first / relativeOutputs[1], first / relativeOutputs[0]);

    const std::string obj = readText(first / relativeOutputs[2]);
    std::istringstream lines(obj);
    std::string line;
    std::size_t positions = 0;
    std::size_t normals = 0;
    std::size_t textureCoordinates = 0;
    std::size_t faces = 0;
    while (std::getline(lines, line)) {
        positions += line.starts_with("v ") ? 1U : 0U;
        normals += line.starts_with("vn ") ? 1U : 0U;
        textureCoordinates += line.starts_with("vt ") ? 1U : 0U;
        faces += line.starts_with("f ") ? 1U : 0U;
    }
    require(positions == kExpectedVertices, "diagnostic OBJ vertex count mismatch");
    require(normals == kExpectedVertices, "diagnostic OBJ normal count mismatch");
    require(textureCoordinates == 0, "diagnostic OBJ must remain UV-free");
    require(faces == kExpectedTriangles, "diagnostic OBJ face count mismatch");
}

void invalidCli(const fs::path& executable, const fs::path& outputRoot) {
    clearKnownOutputs(outputRoot);
    const DWORD result = runProcess(
        executable, L"--output-root " + quoteArgument(outputRoot) + L" --unknown-option");
    require(result != 0, "unknown CLI arguments must fail clearly");
    require(!fs::exists(outputRoot / "meshes" / "sky" / "atmosphere.nif"),
            "invalid CLI input must not emit a NIF");
}

} // namespace

int main(const int argc, char** argv) {
    try {
        require(argc == 4, "usage: truth_sky_mesh_contract_tests <mode> <generator> <output-root>");
        const std::string mode = argv[1];
        const fs::path executable = fs::path(argv[2]);
        const fs::path outputRoot = fs::path(argv[3]);

        if (mode == "contract") {
            contract(executable, outputRoot);
        } else if (mode == "determinism") {
            determinism(executable, outputRoot);
        } else if (mode == "invalid-cli") {
            invalidCli(executable, outputRoot);
        } else {
            fail("unknown test mode: " + mode);
        }

        std::cout << "PASS " << mode << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
