// SPDX-License-Identifier: GPL-3.0-or-later

#include <truth/sky_mesh/AtmosphereMesh.hpp>

#include <Geometry.hpp>
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
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifndef TRUTH_NIFLY_REVISION
#error "TRUTH_NIFLY_REVISION must be supplied by CMake"
#endif

namespace fs = std::filesystem;
using namespace nifly;

namespace truth::sky_mesh {
namespace {

constexpr std::uint32_t kSubdivisionLevel = 4;
constexpr float kRadius = 500.0F;
constexpr double kPositionQuantization = 4096.0;
constexpr std::size_t kExpectedVertices = 2562;
constexpr std::size_t kExpectedTriangles = 5120;
constexpr std::uint32_t kAlwaysDrawFlags = 0x0008000EU;

struct Double3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct IndexTriangle {
    std::uint32_t a = 0;
    std::uint32_t b = 0;
    std::uint32_t c = 0;
};

struct MeshData {
    std::vector<Vector3> vertices;
    std::vector<Vector3> normals;
    std::vector<Color4> colors;
    std::vector<Triangle> triangles;
};

[[noreturn]] void fail(const std::string& message) {
    throw std::runtime_error(message);
}

void require(const bool condition, const std::string& message) {
    if (!condition) {
        fail(message);
    }
}

Double3 normalize(const Double3 value) {
    const double magnitude = std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    require(std::isfinite(magnitude) && magnitude > 0.0, "cannot normalize an invalid direction");
    return {value.x / magnitude, value.y / magnitude, value.z / magnitude};
}

Double3 midpoint(const Double3 lhs, const Double3 rhs) {
    return normalize({(lhs.x + rhs.x) * 0.5, (lhs.y + rhs.y) * 0.5, (lhs.z + rhs.z) * 0.5});
}

float quantizePosition(const double value) {
    return static_cast<float>(std::round(value * static_cast<double>(kRadius)
                                         * kPositionQuantization)
                              / kPositionQuantization);
}

float smoothstep(const float edge0, const float edge1, const float value) {
    const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0F, 1.0F);
    return t * t * (3.0F - 2.0F * t);
}

Color4 elevationWeights(const Vector3& position) {
    const float height = std::clamp(position.z / kRadius, -1.0F, 1.0F);
    const float t = (height + 1.0F) * 0.5F;
    const float shaped = smoothstep(0.0F, 1.0F, t);

    // The RGB channels are upper/middle/lower weather-color blend weights.
    const float lower = std::max(1.0F - 2.0F * shaped, 0.0F);
    const float upper = std::max(2.0F * shaped - 1.0F, 0.0F);
    const float middle = 1.0F - lower - upper;
    const float alpha = smoothstep(0.08F, 0.52F, t)
        * (1.0F - 0.08F * smoothstep(0.75F, 1.0F, t));
    return {upper, middle, lower, alpha};
}

MeshData buildIcosphere() {
    constexpr double goldenRatio = 1.6180339887498948482;
    std::vector<Double3> directions{
        {-1.0, goldenRatio, 0.0}, {1.0, goldenRatio, 0.0},
        {-1.0, -goldenRatio, 0.0}, {1.0, -goldenRatio, 0.0},
        {0.0, -1.0, goldenRatio}, {0.0, 1.0, goldenRatio},
        {0.0, -1.0, -goldenRatio}, {0.0, 1.0, -goldenRatio},
        {goldenRatio, 0.0, -1.0}, {goldenRatio, 0.0, 1.0},
        {-goldenRatio, 0.0, -1.0}, {-goldenRatio, 0.0, 1.0}};
    for (auto& direction : directions) {
        direction = normalize(direction);
    }

    std::vector<IndexTriangle> faces{
        {0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11},
        {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
        {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9},
        {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1}};

    for (std::uint32_t level = 0; level < kSubdivisionLevel; ++level) {
        std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> midpointCache;
        std::vector<IndexTriangle> refined;
        refined.reserve(faces.size() * 4U);

        auto midpointIndex = [&](std::uint32_t first, std::uint32_t second) {
            if (first > second) {
                std::swap(first, second);
            }
            const auto key = std::pair{first, second};
            if (const auto existing = midpointCache.find(key); existing != midpointCache.end()) {
                return existing->second;
            }
            const auto index = static_cast<std::uint32_t>(directions.size());
            directions.push_back(midpoint(directions[first], directions[second]));
            midpointCache.emplace(key, index);
            return index;
        };

        for (const auto face : faces) {
            const auto ab = midpointIndex(face.a, face.b);
            const auto bc = midpointIndex(face.b, face.c);
            const auto ca = midpointIndex(face.c, face.a);
            refined.push_back({face.a, ab, ca});
            refined.push_back({face.b, bc, ab});
            refined.push_back({face.c, ca, bc});
            refined.push_back({ab, bc, ca});
        }
        faces = std::move(refined);
    }

    require(directions.size() == kExpectedVertices, "Icosphere vertex count construction failure");
    require(faces.size() == kExpectedTriangles, "Icosphere triangle count construction failure");
    require(directions.size() <= std::numeric_limits<std::uint16_t>::max(),
            "Icosphere exceeds Skyrim SE's 16-bit vertex index limit");

    MeshData mesh;
    mesh.vertices.reserve(directions.size());
    mesh.normals.reserve(directions.size());
    mesh.colors.reserve(directions.size());
    mesh.triangles.reserve(faces.size());

    for (const auto direction : directions) {
        const Vector3 position{
            quantizePosition(direction.x),
            quantizePosition(direction.y),
            quantizePosition(direction.z)};
        const float magnitude = std::sqrt(position.dot(position));
        require(std::isfinite(magnitude) && magnitude > 0.0F, "quantized vertex is invalid");
        mesh.vertices.push_back(position);
        mesh.normals.emplace_back(-position.x / magnitude, -position.y / magnitude, -position.z / magnitude);
        mesh.colors.push_back(elevationWeights(position));
    }

    for (const auto face : faces) {
        // Standard Icosphere faces are outward; reverse B/C for an interior sky dome.
        mesh.triangles.emplace_back(
            static_cast<std::uint16_t>(face.a),
            static_cast<std::uint16_t>(face.c),
            static_cast<std::uint16_t>(face.b));
    }
    return mesh;
}

void validateMesh(const MeshData& mesh) {
    require(mesh.vertices.size() == kExpectedVertices, "mesh vertex count mismatch");
    require(mesh.normals.size() == mesh.vertices.size(), "mesh normal count mismatch");
    require(mesh.colors.size() == mesh.vertices.size(), "mesh color count mismatch");
    require(mesh.triangles.size() == kExpectedTriangles, "mesh triangle count mismatch");

    for (std::size_t index = 0; index < mesh.vertices.size(); ++index) {
        const auto& vertex = mesh.vertices[index];
        const auto& normal = mesh.normals[index];
        const float radialDistance = std::sqrt(vertex.dot(vertex));
        const float normalLength = std::sqrt(normal.dot(normal));
        require(std::isfinite(radialDistance) && std::abs(radialDistance - kRadius) <= 0.001F,
                "mesh vertex does not lie on the radius-500 shell");
        require(std::isfinite(normalLength) && std::abs(normalLength - 1.0F) <= 1.0e-5F,
                "mesh contains an invalid normal");
        require(normal.dot(vertex) < 0.0F, "mesh contains a non-inward vertex normal");
    }

    for (const auto triangle : mesh.triangles) {
        require(triangle.p1 < mesh.vertices.size() && triangle.p2 < mesh.vertices.size()
                    && triangle.p3 < mesh.vertices.size(),
                "mesh contains an out-of-range triangle");
        const auto& a = mesh.vertices[triangle.p1];
        const auto& b = mesh.vertices[triangle.p2];
        const auto& c = mesh.vertices[triangle.p3];
        const Vector3 faceNormal = (b - a).cross(c - a);
        const Vector3 centroid = (a + b + c) / 3.0F;
        require(faceNormal.dot(faceNormal) > 1.0e-6F, "mesh contains a degenerate triangle");
        require(faceNormal.dot(centroid) < 0.0F, "mesh contains non-inward triangle winding");
    }
}

void writeBinaryText(const fs::path& path, const std::string& contents) {
    fs::create_directories(path.parent_path());
    const fs::path temporary = path.string() + ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        require(static_cast<bool>(output), "cannot create temporary file: " + temporary.string());
        output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        require(static_cast<bool>(output), "cannot finish temporary file: " + temporary.string());
    }
    std::error_code error;
    fs::remove(path, error);
    require(!error, "cannot replace generated file " + path.string() + ": " + error.message());
    error.clear();
    fs::rename(temporary, path, error);
    require(!error, "cannot publish generated file " + path.string() + ": " + error.message());
}

void writeNif(const MeshData& mesh, const fs::path& path) {
    fs::create_directories(path.parent_path());

    NifFile nif;
    nif.Create(NiVersion::getSSE());
    auto* root = nif.GetRootNode();
    require(root != nullptr, "nifly did not create a root NiNode");
    root->name.get() = "Scene";
    root->flags = kAlwaysDrawFlags;
    nif.GetHeader().SetCreatorInfo("Truth ENB");
    nif.GetHeader().SetExportInfo("Truth Sky Mesh Generator 1.0.0");

    auto sky = std::make_unique<BSSkyShaderProperty>();
    sky->shaderFlags1 = SLSF1_ZBUFFER_TEST;
    sky->shaderFlags2 = SLSF2_ZBUFFER_WRITE | SLSF2_VERTEX_COLORS;
    sky->baseTexture.get().clear();
    sky->skyFlags = 2U;
    const auto skyId = nif.GetHeader().AddBlock(std::move(sky));

    auto shape = std::make_unique<BSTriShape>();
    shape->Create(nif.GetHeader().GetVersion(), &mesh.vertices, &mesh.triangles, nullptr, &mesh.normals);
    shape->name.get() = "TruthAtmosphereDome";
    shape->flags = kAlwaysDrawFlags;
    shape->SetSkinned(false);
    shape->SetUVs(false);
    shape->SetTangents(false);
    shape->SetFullPrecision(true);
    shape->SetBounds(BoundingSphere({0.0F, 0.0F, 0.0F}, kRadius));
    shape->ShaderPropertyRef()->index = skyId;
    auto* shapePointer = shape.get();
    const auto shapeId = nif.GetHeader().AddBlock(std::move(shape));
    root->childRefs.AddBlockRef(shapeId);
    nif.SetColorsForShape(shapePointer, mesh.colors);
    shapePointer->SetBounds(BoundingSphere({0.0F, 0.0F, 0.0F}, kRadius));

    const fs::path temporary = path.string() + ".tmp";
    NifSaveOptions saveOptions;
    saveOptions.optimize = false;
    saveOptions.sortBlocks = true;
    require(nif.Save(temporary, saveOptions) == 0, "nifly failed to serialize the atmosphere NIF");

    std::error_code error;
    fs::remove(path, error);
    require(!error, "cannot replace atmosphere NIF: " + error.message());
    error.clear();
    fs::rename(temporary, path, error);
    require(!error, "cannot publish atmosphere NIF: " + error.message());
}

void validateSerializedNif(const fs::path& path) {
    NifFile nif;
    require(nif.Load(path) == 0 && nif.IsValid() && !nif.HasUnknown(),
            "nifly could not parse its serialized atmosphere NIF");
    const auto& version = nif.GetHeader().GetVersion();
    require(version.File() == NiFileVersion::V20_2_0_7
                && version.User() == 12 && version.Stream() == 100,
            "serialized atmosphere NIF has the wrong Skyrim SE version tuple");
    require(nif.GetHeader().GetNumBlocks() == 3, "serialized NIF has unexpected blocks");
    const auto shapes = nif.GetShapes();
    require(shapes.size() == 1, "serialized NIF must contain one shape");
    auto* shape = dynamic_cast<BSTriShape*>(shapes.front());
    require(shape != nullptr, "serialized NIF shape is not BSTriShape");
    const auto* vertices = nif.GetVertsForShape(shape);
    const auto* normals = nif.GetNormalsForShape(shape);
    const auto* uvs = nif.GetUvsForShape(shape);
    const auto* colors = nif.GetColorsForShape(shape);
    std::vector<Triangle> triangles;
    shape->GetTriangles(triangles);
    require(vertices != nullptr && vertices->size() == kExpectedVertices,
            "serialized NIF vertex count mismatch");
    require(normals != nullptr && normals->size() == kExpectedVertices,
            "serialized NIF normal count mismatch");
    require((uvs == nullptr || uvs->empty()) && !shape->HasUVs(),
            "serialized NIF unexpectedly contains UVs");
    require(colors != nullptr && colors->size() == kExpectedVertices,
            "serialized NIF color count mismatch");
    require(triangles.size() == kExpectedTriangles, "serialized NIF triangle count mismatch");
    auto* sky = dynamic_cast<BSSkyShaderProperty*>(nif.GetShader(shape));
    require(sky != nullptr && sky->skyFlags == 2U,
            "serialized NIF lacks the required atmosphere sky shader block");
    require(sky->shaderFlags1 == SLSF1_ZBUFFER_TEST
                && sky->shaderFlags2 == (SLSF2_ZBUFFER_WRITE | SLSF2_VERTEX_COLORS),
            "serialized NIF has incorrect sky shader flags");
    const auto bounds = shape->GetBounds();
    require(std::abs(bounds.radius - kRadius) <= 1.0e-6F
                && std::sqrt(bounds.center.dot(bounds.center)) <= 1.0e-6F,
            "serialized NIF has incorrect bounds");
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
    require(static_cast<bool>(input), "cannot hash NIF: " + path.string());
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

void writeObj(const MeshData& mesh, const fs::path& path) {
    std::ostringstream output;
    output << "# Truth ENB original deterministic inward Icosphere L4\n"
           << "# Diagnostic only; atmosphere.nif is the product output.\n"
           << std::fixed << std::setprecision(9);
    for (const auto& vertex : mesh.vertices) {
        output << "v " << vertex.x << ' ' << vertex.y << ' ' << vertex.z << '\n';
    }
    for (const auto& normal : mesh.normals) {
        output << "vn " << normal.x << ' ' << normal.y << ' ' << normal.z << '\n';
    }
    for (const auto triangle : mesh.triangles) {
        const auto first = static_cast<std::uint32_t>(triangle.p1) + 1U;
        const auto second = static_cast<std::uint32_t>(triangle.p2) + 1U;
        const auto third = static_cast<std::uint32_t>(triangle.p3) + 1U;
        output << "f " << first << "//" << first << ' '
               << second << "//" << second << ' '
               << third << "//" << third << '\n';
    }
    writeBinaryText(path, output.str());
}

void writeManifest(const fs::path& path, const std::string& digest,
                   const std::uintmax_t sizeBytes) {
    std::ostringstream output;
    output << "{\n"
           << "  \"schema_version\": 1,\n"
           << "  \"asset_path\": \"meshes/sky/atmosphere.nif\",\n"
           << "  \"generator\": \"Truth Sky Mesh Generator 1.0.0\",\n"
           << "  \"generator_license\": \"GPL-3.0-or-later\",\n"
           << "  \"generated_asset_license\": \"not-selected-at-repository-level\",\n"
           << "  \"geometry\": \"original deterministic inward Icosphere L4\",\n"
           << "  \"template_source\": null,\n"
           << "  \"nifly_revision\": \"" << TRUTH_NIFLY_REVISION << "\",\n"
           << "  \"nifly_role\": \"optional external build-time serializer only\",\n"
           << "  \"nifly_license\": \"GPL-3.0-or-later\",\n"
           << "  \"nif_version\": \"20.2.0.7\",\n"
           << "  \"user_version\": 12,\n"
           << "  \"stream_version\": 100,\n"
           << "  \"sha256\": \"" << digest << "\",\n"
           << "  \"size_bytes\": " << sizeBytes << ",\n"
           << "  \"subdivision_level\": " << kSubdivisionLevel << ",\n"
           << "  \"radius\": 500.0,\n"
           << "  \"vertices\": " << kExpectedVertices << ",\n"
           << "  \"triangles\": " << kExpectedTriangles << ",\n"
           << "  \"uv_channels\": 0,\n"
           << "  \"position_quantization\": \"1/4096 game unit\",\n"
           << "  \"normal_encoding\": \"normalized inward vectors; NIF 8-bit packed\",\n"
           << "  \"color_encoding\": \"8-bit upper/middle/lower elevation weights plus alpha\"\n"
           << "}\n";
    writeBinaryText(path, output.str());
}

} // namespace

GenerationResult generateAtmosphereFallback(const GenerationOptions& options) {
    require(!options.outputRoot.empty(), "output root must not be empty");
    const MeshData mesh = buildIcosphere();
    validateMesh(mesh);

    GenerationResult result;
    result.nifPath = options.outputRoot / "meshes" / "sky" / "atmosphere.nif";
    result.manifestPath = options.outputRoot / "truth-atmosphere-mesh.manifest.json";
    result.vertices = mesh.vertices.size();
    result.triangles = mesh.triangles.size();

    writeNif(mesh, result.nifPath);
    validateSerializedNif(result.nifPath);
    result.sizeBytes = fs::file_size(result.nifPath);
    result.sha256 = sha256(result.nifPath);
    writeManifest(result.manifestPath, result.sha256, result.sizeBytes);

    if (options.emitObj) {
        result.objPath = options.outputRoot / "truth-atmosphere-mesh.obj";
        writeObj(mesh, result.objPath);
    }
    return result;
}

} // namespace truth::sky_mesh
