#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace truth::runtime {

struct Float4 final {
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};
    float w{0.0F};

    [[nodiscard]] constexpr bool operator==(const Float4&) const noexcept = default;
};

struct Matrix4 final {
    std::array<float, 16> values{};

    [[nodiscard]] constexpr float& at(
        const std::size_t row,
        const std::size_t column) noexcept
    {
        return values[(row * 4U) + column];
    }

    [[nodiscard]] constexpr float at(
        const std::size_t row,
        const std::size_t column) const noexcept
    {
        return values[(row * 4U) + column];
    }
};

struct NiFrustumAbi final {
    float left;
    float right;
    float top;
    float bottom;
    float near_plane;
    float far_plane;
    std::uint8_t orthographic;
    std::array<std::uint8_t, 3> padding;
};

struct NiViewportAbi final {
    float left;
    float right;
    float top;
    float bottom;
};

struct NiCameraRuntimeData2Abi final {
    NiFrustumAbi view_frustum;
    float minimum_near_plane_distance;
    float maximum_far_near_ratio;
    NiViewportAbi viewport;
    float lod_adjust;
};

inline constexpr std::size_t kNiCameraWorldToCameraOffset = 0x110U;
inline constexpr std::size_t kNiCameraFrustumViewportOffset = 0x150U;

static_assert(sizeof(NiFrustumAbi) == 0x1CU);
static_assert(offsetof(NiCameraRuntimeData2Abi, view_frustum) == 0x00U);
static_assert(offsetof(NiCameraRuntimeData2Abi, minimum_near_plane_distance) == 0x1CU);
static_assert(offsetof(NiCameraRuntimeData2Abi, maximum_far_near_ratio) == 0x20U);
static_assert(offsetof(NiCameraRuntimeData2Abi, viewport) == 0x24U);
static_assert(offsetof(NiCameraRuntimeData2Abi, lod_adjust) == 0x34U);
static_assert(sizeof(NiCameraRuntimeData2Abi) == 0x38U);

struct CameraSnapshot final {
    Matrix4 world_to_camera;
    NiCameraRuntimeData2Abi projection{};
};

struct CameraFrame final {
    std::array<Float4, 4> inverse_view_projection_rows{};
    Float4 camera_world{};
};

enum class CameraFrameDiagnostic : std::uint16_t {
    None = 0,
    NullCamera = 1,
    CameraAddressOverflow = 2,
    WorldToCameraReadFailed = 3,
    FrustumViewportReadFailed = 4,
    NonFiniteWorldToCamera = 5,
    InvalidAffineWorldToCamera = 6,
    InvalidViewBasis = 7,
    NonFiniteFrustum = 8,
    InvalidFrustum = 9,
    InvalidOrthographicFlag = 10,
    NonFiniteViewport = 11,
    InvalidViewport = 12,
    SingularView = 13,
    SingularViewProjection = 14,
    NonFiniteResult = 15,
    CameraOutOfRange = 16,
    RuntimeNotReady = 100,
    CameraLocatorFailed = 101,
};

struct CameraFrameResult final {
    CameraFrameDiagnostic diagnostic{CameraFrameDiagnostic::NullCamera};
    CameraFrame frame{};

    [[nodiscard]] constexpr bool valid() const noexcept
    {
        return diagnostic == CameraFrameDiagnostic::None;
    }
};

class ProcessMemoryReader {
public:
    virtual ~ProcessMemoryReader() = default;
    [[nodiscard]] virtual bool Read(
        std::uintptr_t address,
        std::span<std::uint8_t> destination) const noexcept = 0;
};

[[nodiscard]] CameraFrameResult BuildCameraFrame(
    const CameraSnapshot& snapshot) noexcept;
[[nodiscard]] CameraFrameResult ReadCameraFrame(
    const ProcessMemoryReader& memory,
    std::uintptr_t camera_address) noexcept;

} // namespace truth::runtime
