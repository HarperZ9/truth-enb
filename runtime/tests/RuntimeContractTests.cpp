#include "TestHarness.hpp"

#include <truth/runtime/CameraFrame.hpp>
#include <truth/runtime/RuntimeContract.hpp>

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

namespace {

using namespace truth::runtime;
using truth::runtime::tests::Context;

template <typename Integer>
void Append(std::vector<std::uint8_t>& bytes, const Integer value)
{
    using Unsigned = std::make_unsigned_t<Integer>;
    const Unsigned encoded = static_cast<Unsigned>(value);
    for (std::size_t index = 0; index < sizeof(Integer); ++index) {
        bytes.push_back(static_cast<std::uint8_t>(encoded >> (index * 8U)));
    }
}

struct Entry final {
    std::uint64_t id;
    std::uint64_t offset;
};

[[nodiscard]] std::vector<std::uint8_t> Database(
    const RuntimeVersion version,
    const std::int32_t format,
    const std::span<const Entry> entries)
{
    std::vector<std::uint8_t> bytes;
    Append(bytes, format);
    Append(bytes, version.major);
    Append(bytes, version.minor);
    Append(bytes, version.patch);
    Append(bytes, version.build);
    const std::string name = "SkyrimSE.exe";
    Append(bytes, static_cast<std::int32_t>(name.size()));
    bytes.insert(bytes.end(), name.begin(), name.end());
    Append(bytes, std::int32_t{8});
    Append(bytes, static_cast<std::int32_t>(entries.size()));
    for (const Entry entry : entries) {
        bytes.push_back(0x00U);
        Append(bytes, entry.id);
        Append(bytes, entry.offset);
    }
    return bytes;
}

[[nodiscard]] std::vector<std::uint8_t> DatabaseWithRecords(
    const RuntimeVersion version,
    const std::int32_t format,
    const std::int32_t entry_count,
    const std::span<const std::uint8_t> records)
{
    std::vector<std::uint8_t> bytes;
    Append(bytes, format);
    Append(bytes, version.major);
    Append(bytes, version.minor);
    Append(bytes, version.patch);
    Append(bytes, version.build);
    const std::string name = "SkyrimSE.exe";
    Append(bytes, static_cast<std::int32_t>(name.size()));
    bytes.insert(bytes.end(), name.begin(), name.end());
    Append(bytes, std::int32_t{8});
    Append(bytes, entry_count);
    bytes.insert(bytes.end(), records.begin(), records.end());
    return bytes;
}

void RuntimeSelectionAndPathAreExact(Context& context)
{
    const RuntimeVersion se{1, 5, 97, 0};
    const RuntimeSelection se_selection = SelectRuntime(se);
    context.expect(se_selection.family == RuntimeFamily::SpecialEdition,
        "1.5.97 was not admitted as SE");
    context.expect(se_selection.address_library_format == 1U,
        "SE did not select Address Library format 1");
    context.expect(se_selection.world_root_camera_id == kWorldRootCameraSeId,
        "SE selected the wrong WorldRootCamera relocation");
    context.expect(
        BuildAddressLibraryRelativePath(se)
            == L"Data\\SKSE\\Plugins\\version-1-5-97-0.bin",
        "SE Address Library path was not exact");

    const RuntimeVersion ae{1, 6, 1170, 0};
    const RuntimeSelection ae_selection = SelectRuntime(ae);
    context.expect(ae_selection.family == RuntimeFamily::AnniversaryEdition,
        "1.6.x was not admitted as AE");
    context.expect(ae_selection.address_library_format == 2U,
        "AE did not select Address Library format 2");
    context.expect(ae_selection.world_root_camera_id == kWorldRootCameraAeId,
        "AE selected the wrong WorldRootCamera relocation");
    context.expect(
        BuildAddressLibraryRelativePath(ae)
            == L"Data\\SKSE\\Plugins\\versionlib-1-6-1170-0.bin",
        "AE Address Library path was not exact");

    context.expect(!SelectRuntime({1, 5, 96, 0}).supported(),
        "an unsupported SE runtime was admitted");
    context.expect(!SelectRuntime({1, 4, 15, 0}).supported(),
        "Skyrim VR was admitted");
    context.expect(!SelectRuntime({2, 0, 0, 0}).supported(),
        "a foreign runtime was admitted");
}

void BothAddressLibraryFormatsResolveOnlyTheirExactId(Context& context)
{
    constexpr std::uintptr_t module_base = 0x140000000ULL;
    constexpr std::size_t module_size = 0x02000000U;
    const RuntimeVersion se{1, 5, 97, 0};
    const std::array se_entries{
        Entry{100U, 0x1000U},
        Entry{kWorldRootCameraSeId, 0x123456U},
    };
    AddressDatabaseResult result = ResolveAddressLibraryRelocation(
        Database(se, 1, se_entries), se, module_base, module_size);
    context.expect(result.accepted(), "valid SE database was rejected");
    context.expect(result.resolved_address == module_base + 0x123456U,
        "SE relocation resolved to the wrong address");

    const RuntimeVersion ae{1, 6, 640, 0};
    const std::array ae_entries{
        Entry{kWorldRootCameraSeId, 0x111111U},
        Entry{kWorldRootCameraAeId, 0x654321U},
    };
    result = ResolveAddressLibraryRelocation(
        Database(ae, 2, ae_entries), ae, module_base, module_size);
    context.expect(result.accepted(), "valid AE database was rejected");
    context.expect(result.resolved_address == module_base + 0x654321U,
        "AE selected the SE relocation id");
}

void AddressDatabaseRejectsMismatchesAndAmbiguity(Context& context)
{
    constexpr std::uintptr_t module_base = 0x140000000ULL;
    constexpr std::size_t module_size = 0x01000000U;
    const RuntimeVersion runtime{1, 6, 1170, 0};
    const std::array one{Entry{kWorldRootCameraAeId, 0x2000U}};

    auto bytes = Database(runtime, 1, one);
    AddressDatabaseResult result = ResolveAddressLibraryRelocation(
        bytes, runtime, module_base, module_size);
    context.expect(result.diagnostic == AddressDatabaseDiagnostic::WrongFormat,
        "wrong database format was admitted");

    bytes = Database({1, 6, 1130, 0}, 2, one);
    result = ResolveAddressLibraryRelocation(bytes, runtime, module_base, module_size);
    context.expect(result.diagnostic == AddressDatabaseDiagnostic::RuntimeMismatch,
        "wrong database runtime was admitted");

    bytes = Database(runtime, 2, one);
    constexpr std::size_t module_name_offset =
        sizeof(std::int32_t) * 6U;
    bytes[module_name_offset] = static_cast<std::uint8_t>('T');
    result = ResolveAddressLibraryRelocation(bytes, runtime, module_base, module_size);
    context.expect(result.diagnostic
            == AddressDatabaseDiagnostic::ModuleNameMismatch,
        "database for a different executable module was admitted");

    bytes = Database(runtime, 2, one);
    bytes.resize(module_name_offset + 5U);
    result = ResolveAddressLibraryRelocation(bytes, runtime, module_base, module_size);
    context.expect(result.diagnostic == AddressDatabaseDiagnostic::Truncated,
        "truncated module name header was not diagnosed as truncation");

    const std::array duplicate{
        Entry{kWorldRootCameraAeId, 0x2000U},
        Entry{kWorldRootCameraAeId, 0x3000U},
    };
    result = ResolveAddressLibraryRelocation(
        Database(runtime, 2, duplicate), runtime, module_base, module_size);
    context.expect(result.diagnostic == AddressDatabaseDiagnostic::DuplicateId,
        "duplicate relocation IDs were admitted");

    const std::array outside{Entry{kWorldRootCameraAeId, module_size}};
    result = ResolveAddressLibraryRelocation(
        Database(runtime, 2, outside), runtime, module_base, module_size);
    context.expect(result.diagnostic
            == AddressDatabaseDiagnostic::OffsetOutsideModule,
        "out-of-image relocation was admitted");

    bytes = Database(runtime, 2, one);
    bytes.pop_back();
    result = ResolveAddressLibraryRelocation(bytes, runtime, module_base, module_size);
    context.expect(result.diagnostic == AddressDatabaseDiagnostic::Truncated,
        "truncated database was admitted");

    const std::array missing{Entry{42U, 0x2000U}};
    result = ResolveAddressLibraryRelocation(
        Database(runtime, 2, missing), runtime, module_base, module_size);
    context.expect(result.diagnostic == AddressDatabaseDiagnostic::RelocationMissing,
        "database missing WorldRootCamera was admitted");
}

void CompressedAddressLibraryRecordsAreDecodedAndBounded(Context& context)
{
    constexpr std::uintptr_t module_base = 0x140000000ULL;
    constexpr std::size_t module_size = 0x01000000U;
    const RuntimeVersion runtime{1, 6, 1170, 0};
    std::vector<std::uint8_t> records;
    records.push_back(0x00U);
    Append(records, kWorldRootCameraAeId);
    Append(records, std::uint64_t{0x1000U});
    records.push_back(0x91U);
    records.push_back(0x22U);
    records.push_back(5U);
    records.push_back(0x10U);
    AddressDatabaseResult result = ResolveAddressLibraryRelocation(
        DatabaseWithRecords(runtime, 2, 3, records),
        runtime,
        module_base,
        module_size);
    context.expect(result.accepted()
            && result.resolved_address == module_base + 0x1000U,
        "valid compressed/scaled records were rejected");

    records.clear();
    records.push_back(0x00U);
    Append(records, kWorldRootCameraAeId);
    Append(records, std::uint64_t{0x1001U});
    records.push_back(0x91U);
    result = ResolveAddressLibraryRelocation(
        DatabaseWithRecords(runtime, 2, 2, records),
        runtime,
        module_base,
        module_size);
    context.expect(result.diagnostic == AddressDatabaseDiagnostic::InvalidEncoding,
        "scaled delta from an unaligned previous offset was admitted");

    records.clear();
    records.push_back(0x03U);
    records.push_back(1U);
    Append(records, std::uint64_t{0x1000U});
    result = ResolveAddressLibraryRelocation(
        DatabaseWithRecords(runtime, 2, 1, records),
        runtime,
        module_base,
        module_size);
    context.expect(result.diagnostic
            == AddressDatabaseDiagnostic::ArithmeticOverflow,
        "compressed ID underflow was admitted");

    auto trailing = Database(
        runtime,
        2,
        std::array{Entry{kWorldRootCameraAeId, 0x1000U}});
    trailing.push_back(0xAAU);
    result = ResolveAddressLibraryRelocation(
        trailing, runtime, module_base, module_size);
    context.expect(result.diagnostic == AddressDatabaseDiagnostic::TrailingData,
        "trailing database payload was admitted");
}

[[nodiscard]] CameraSnapshot ReferenceCamera()
{
    CameraSnapshot snapshot{};
    snapshot.world_to_camera.values = {
        1.0F, 0.0F, 0.0F, 0.0F,
        0.0F, 1.0F, 0.0F, 0.0F,
        0.0F, 0.0F, 1.0F, 0.0F,
        -10.0F, -20.0F, -30.0F, 1.0F,
    };
    snapshot.projection.view_frustum = {
        -1.0F, 1.0F, 1.0F, -1.0F, 1.0F, 100.0F, 0U, {0U, 0U, 0U},
    };
    snapshot.projection.minimum_near_plane_distance = 0.1F;
    snapshot.projection.maximum_far_near_ratio = 100000.0F;
    snapshot.projection.viewport = {0.0F, 1.0F, 1.0F, 0.0F};
    snapshot.projection.lod_adjust = 1.0F;
    return snapshot;
}

[[nodiscard]] bool Close(
    const float left,
    const float right,
    const float tolerance = 2.0e-4F) noexcept
{
    return std::fabs(left - right) <= tolerance;
}

[[nodiscard]] Float4 Multiply(
    const std::array<Float4, 4>& rows,
    const Float4 vector) noexcept
{
    const auto dot = [&vector](const Float4 row) {
        return (row.x * vector.x) + (row.y * vector.y)
            + (row.z * vector.z) + (row.w * vector.w);
    };
    return {dot(rows[0]), dot(rows[1]), dot(rows[2]), dot(rows[3])};
}

void CameraMathProducesTheShaderColumnVectorContract(Context& context)
{
    const CameraFrameResult result = BuildCameraFrame(ReferenceCamera());
    context.expect(result.valid(), "valid camera snapshot was rejected");
    context.expect(Close(result.frame.camera_world.x, 10.0F)
            && Close(result.frame.camera_world.y, 20.0F)
            && Close(result.frame.camera_world.z, 30.0F),
        "camera position was not derived from inverse worldToCam");

    const Float4 homogeneous = Multiply(
        result.frame.inverse_view_projection_rows,
        Float4{0.0F, 0.0F, 1.0F, 1.0F});
    context.expect(std::fabs(homogeneous.w) > 1.0e-6F,
        "far clip unprojection produced zero W");
    context.expect(Close(homogeneous.x / homogeneous.w, 10.0F)
            && Close(homogeneous.y / homogeneous.w, 20.0F)
            && Close(homogeneous.z / homogeneous.w, 130.0F),
        "row-major shader matrix did not unproject a D3D clip column vector");
}

void CameraLayoutAndMathFailClosed(Context& context)
{
    CameraSnapshot snapshot = ReferenceCamera();
    snapshot.world_to_camera.at(0, 0) =
        std::numeric_limits<float>::quiet_NaN();
    context.expect(BuildCameraFrame(snapshot).diagnostic
            == CameraFrameDiagnostic::NonFiniteWorldToCamera,
        "non-finite view matrix was admitted");

    snapshot = ReferenceCamera();
    snapshot.world_to_camera.at(0, 3) = 1.0F;
    context.expect(BuildCameraFrame(snapshot).diagnostic
            == CameraFrameDiagnostic::InvalidAffineWorldToCamera,
        "non-affine view matrix was admitted");

    snapshot = ReferenceCamera();
    snapshot.projection.view_frustum.near_plane = 0.0F;
    context.expect(BuildCameraFrame(snapshot).diagnostic
            == CameraFrameDiagnostic::InvalidFrustum,
        "invalid frustum was admitted");

    snapshot = ReferenceCamera();
    snapshot.projection.view_frustum.orthographic = 2U;
    context.expect(BuildCameraFrame(snapshot).diagnostic
            == CameraFrameDiagnostic::InvalidOrthographicFlag,
        "invalid orthographic flag was admitted");

    snapshot = ReferenceCamera();
    snapshot.projection.viewport.right = snapshot.projection.viewport.left;
    context.expect(BuildCameraFrame(snapshot).diagnostic
            == CameraFrameDiagnostic::InvalidViewport,
        "empty viewport was admitted");
}

class RecordingMemory final : public ProcessMemoryReader {
public:
    CameraSnapshot snapshot{ReferenceCamera()};
    mutable std::vector<std::uintptr_t> reads;

    [[nodiscard]] bool Read(
        const std::uintptr_t address,
        const std::span<std::uint8_t> destination) const noexcept override
    {
        reads.push_back(address);
        if (address == 0x100000U + kNiCameraWorldToCameraOffset
            && destination.size() == sizeof(Matrix4)) {
            std::memcpy(destination.data(), &snapshot.world_to_camera, destination.size());
            return true;
        }
        if (address == 0x100000U + kNiCameraFrustumViewportOffset
            && destination.size() == sizeof(NiCameraRuntimeData2Abi)) {
            std::memcpy(destination.data(), &snapshot.projection, destination.size());
            return true;
        }
        return false;
    }
};

void CameraMemoryReadsOnlyTheAdmittedNonVrOffsets(Context& context)
{
    RecordingMemory memory;
    const CameraFrameResult result = ReadCameraFrame(memory, 0x100000U);
    context.expect(result.valid(), "camera memory snapshot was rejected");
    context.expect(memory.reads.size() == 2U,
        "camera reader performed an unexpected number of reads");
    context.expect(memory.reads[0] == 0x100000U + 0x110U,
        "worldToCam was read from the wrong ABI offset");
    context.expect(memory.reads[1] == 0x100000U + 0x150U,
        "frustum/viewport was read from the wrong ABI offset");

    context.expect(ReadCameraFrame(memory, 0U).diagnostic
            == CameraFrameDiagnostic::NullCamera,
        "null camera pointer was admitted");
    context.expect(ReadCameraFrame(
        memory,
        (std::numeric_limits<std::uintptr_t>::max)() - 0x100U).diagnostic
            == CameraFrameDiagnostic::CameraAddressOverflow,
        "camera offset overflow was admitted");
}

constexpr truth::runtime::tests::TestCase kTests[]{
    {"runtime selection and Address Library path are exact",
        &RuntimeSelectionAndPathAreExact},
    {"both Address Library formats resolve only their exact ID",
        &BothAddressLibraryFormatsResolveOnlyTheirExactId},
    {"Address Library rejects mismatches and ambiguity",
        &AddressDatabaseRejectsMismatchesAndAmbiguity},
    {"compressed Address Library records are decoded and bounded",
        &CompressedAddressLibraryRecordsAreDecodedAndBounded},
    {"camera math produces the shader column-vector contract",
        &CameraMathProducesTheShaderColumnVectorContract},
    {"camera layout and math fail closed", &CameraLayoutAndMathFailClosed},
    {"camera memory reads only admitted non-VR offsets",
        &CameraMemoryReadsOnlyTheAdmittedNonVrOffsets},
};

} // namespace

int main()
{
    return truth::runtime::tests::Run(
        "Truth runtime contract cases", kTests, std::size(kTests));
}
