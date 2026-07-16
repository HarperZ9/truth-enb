#include <truth/runtime/CameraFrame.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>

namespace truth::runtime {
namespace {

using MatrixD = std::array<double, 16>;

[[nodiscard]] constexpr double& At(
    MatrixD& matrix,
    const std::size_t row,
    const std::size_t column) noexcept
{
    return matrix[(row * 4U) + column];
}

[[nodiscard]] constexpr double At(
    const MatrixD& matrix,
    const std::size_t row,
    const std::size_t column) noexcept
{
    return matrix[(row * 4U) + column];
}

[[nodiscard]] bool Finite(const float value) noexcept
{
    return std::isfinite(value);
}

template <typename Range>
[[nodiscard]] bool AllFinite(const Range& values) noexcept
{
    return std::ranges::all_of(values, [](const float value) {
        return Finite(value);
    });
}

[[nodiscard]] MatrixD ToDouble(const Matrix4& matrix) noexcept
{
    MatrixD result{};
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = matrix.values[index];
    }
    return result;
}

[[nodiscard]] MatrixD Multiply(
    const MatrixD& left,
    const MatrixD& right) noexcept
{
    MatrixD product{};
    for (std::size_t row = 0; row < 4U; ++row) {
        for (std::size_t column = 0; column < 4U; ++column) {
            double value = 0.0;
            for (std::size_t inner = 0; inner < 4U; ++inner) {
                value += At(left, row, inner) * At(right, inner, column);
            }
            At(product, row, column) = value;
        }
    }
    return product;
}

[[nodiscard]] bool Invert(const MatrixD& input, MatrixD& output) noexcept
{
    std::array<double, 32> augmented{};
    const auto cell = [&augmented](
                          const std::size_t row,
                          const std::size_t column) -> double& {
        return augmented[(row * 8U) + column];
    };
    for (std::size_t row = 0; row < 4U; ++row) {
        for (std::size_t column = 0; column < 4U; ++column) {
            cell(row, column) = At(input, row, column);
            cell(row, column + 4U) = row == column ? 1.0 : 0.0;
        }
    }

    constexpr double minimum_pivot = 1.0e-12;
    for (std::size_t column = 0; column < 4U; ++column) {
        std::size_t pivot_row = column;
        double pivot_magnitude = std::fabs(cell(pivot_row, column));
        for (std::size_t row = column + 1U; row < 4U; ++row) {
            const double magnitude = std::fabs(cell(row, column));
            if (magnitude > pivot_magnitude) {
                pivot_magnitude = magnitude;
                pivot_row = row;
            }
        }
        if (!std::isfinite(pivot_magnitude)
            || pivot_magnitude < minimum_pivot) {
            return false;
        }
        if (pivot_row != column) {
            for (std::size_t index = 0; index < 8U; ++index) {
                std::swap(cell(column, index), cell(pivot_row, index));
            }
        }

        const double pivot = cell(column, column);
        for (std::size_t index = 0; index < 8U; ++index) {
            cell(column, index) /= pivot;
        }
        for (std::size_t row = 0; row < 4U; ++row) {
            if (row == column) {
                continue;
            }
            const double factor = cell(row, column);
            for (std::size_t index = 0; index < 8U; ++index) {
                cell(row, index) -= factor * cell(column, index);
            }
        }
    }

    for (std::size_t row = 0; row < 4U; ++row) {
        for (std::size_t column = 0; column < 4U; ++column) {
            At(output, row, column) = cell(row, column + 4U);
            if (!std::isfinite(At(output, row, column))) {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] bool Approximately(
    const double value,
    const double expected,
    const double tolerance = 2.0e-3) noexcept
{
    return std::fabs(value - expected) <= tolerance;
}

[[nodiscard]] bool ViewBasisIsValid(const Matrix4& view) noexcept
{
    for (std::size_t row = 0; row < 3U; ++row) {
        double length_squared = 0.0;
        for (std::size_t column = 0; column < 3U; ++column) {
            length_squared += static_cast<double>(view.at(row, column))
                * view.at(row, column);
        }
        if (!Approximately(length_squared, 1.0, 1.0e-2)) {
            return false;
        }
    }
    for (std::size_t left = 0; left < 3U; ++left) {
        for (std::size_t right = left + 1U; right < 3U; ++right) {
            double dot = 0.0;
            for (std::size_t column = 0; column < 3U; ++column) {
                dot += static_cast<double>(view.at(left, column))
                    * view.at(right, column);
            }
            if (!Approximately(dot, 0.0, 1.0e-2)) {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] MatrixD PerspectiveProjection(
    const NiFrustumAbi& frustum) noexcept
{
    const double left = frustum.left;
    const double right = frustum.right;
    const double top = frustum.top;
    const double bottom = frustum.bottom;
    const double near_plane = frustum.near_plane;
    const double far_plane = frustum.far_plane;
    MatrixD projection{};
    At(projection, 0, 0) = (2.0 * near_plane) / (right - left);
    At(projection, 1, 1) = (2.0 * near_plane) / (top - bottom);
    At(projection, 2, 0) = (left + right) / (left - right);
    At(projection, 2, 1) = (top + bottom) / (bottom - top);
    At(projection, 2, 2) = far_plane / (far_plane - near_plane);
    At(projection, 2, 3) = 1.0;
    At(projection, 3, 2) =
        -(near_plane * far_plane) / (far_plane - near_plane);
    return projection;
}

[[nodiscard]] MatrixD OrthographicProjection(
    const NiFrustumAbi& frustum) noexcept
{
    const double left = frustum.left;
    const double right = frustum.right;
    const double top = frustum.top;
    const double bottom = frustum.bottom;
    const double near_plane = frustum.near_plane;
    const double far_plane = frustum.far_plane;
    MatrixD projection{};
    At(projection, 0, 0) = 2.0 / (right - left);
    At(projection, 1, 1) = 2.0 / (top - bottom);
    At(projection, 2, 2) = 1.0 / (far_plane - near_plane);
    At(projection, 3, 0) = (left + right) / (left - right);
    At(projection, 3, 1) = (top + bottom) / (bottom - top);
    At(projection, 3, 2) = near_plane / (near_plane - far_plane);
    At(projection, 3, 3) = 1.0;
    return projection;
}

[[nodiscard]] CameraFrameResult Reject(
    const CameraFrameDiagnostic diagnostic) noexcept
{
    return CameraFrameResult{diagnostic, {}};
}

} // namespace

CameraFrameResult BuildCameraFrame(const CameraSnapshot& snapshot) noexcept
{
    if (!AllFinite(snapshot.world_to_camera.values)) {
        return Reject(CameraFrameDiagnostic::NonFiniteWorldToCamera);
    }
    if (!Approximately(snapshot.world_to_camera.at(0, 3), 0.0)
        || !Approximately(snapshot.world_to_camera.at(1, 3), 0.0)
        || !Approximately(snapshot.world_to_camera.at(2, 3), 0.0)
        || !Approximately(snapshot.world_to_camera.at(3, 3), 1.0)) {
        return Reject(CameraFrameDiagnostic::InvalidAffineWorldToCamera);
    }
    if (!ViewBasisIsValid(snapshot.world_to_camera)) {
        return Reject(CameraFrameDiagnostic::InvalidViewBasis);
    }

    const NiFrustumAbi& frustum = snapshot.projection.view_frustum;
    const std::array frustum_values{
        frustum.left,
        frustum.right,
        frustum.top,
        frustum.bottom,
        frustum.near_plane,
        frustum.far_plane,
    };
    if (!AllFinite(frustum_values)) {
        return Reject(CameraFrameDiagnostic::NonFiniteFrustum);
    }
    if (frustum.orthographic > 1U) {
        return Reject(CameraFrameDiagnostic::InvalidOrthographicFlag);
    }
    if (!(frustum.right > frustum.left)
        || !(frustum.top > frustum.bottom)
        || !(frustum.near_plane > 0.0F)
        || !(frustum.far_plane > frustum.near_plane)
        || frustum.far_plane > 1.0e9F) {
        return Reject(CameraFrameDiagnostic::InvalidFrustum);
    }

    const NiViewportAbi& viewport = snapshot.projection.viewport;
    const std::array viewport_values{
        viewport.left,
        viewport.right,
        viewport.top,
        viewport.bottom,
    };
    if (!AllFinite(viewport_values)) {
        return Reject(CameraFrameDiagnostic::NonFiniteViewport);
    }
    if (std::fabs(viewport.right - viewport.left) < 1.0e-6F
        || std::fabs(viewport.top - viewport.bottom) < 1.0e-6F
        || std::ranges::any_of(viewport_values, [](const float value) {
            return std::fabs(value) > 1.0e6F;
        })) {
        return Reject(CameraFrameDiagnostic::InvalidViewport);
    }

    const MatrixD view = ToDouble(snapshot.world_to_camera);
    MatrixD inverse_view{};
    if (!Invert(view, inverse_view)) {
        return Reject(CameraFrameDiagnostic::SingularView);
    }
    const MatrixD projection = frustum.orthographic == 0U
        ? PerspectiveProjection(frustum)
        : OrthographicProjection(frustum);
    const MatrixD view_projection = Multiply(view, projection);
    MatrixD inverse_view_projection{};
    if (!Invert(view_projection, inverse_view_projection)) {
        return Reject(CameraFrameDiagnostic::SingularViewProjection);
    }

    CameraFrame frame;
    for (std::size_t row = 0; row < 4U; ++row) {
        Float4& output = frame.inverse_view_projection_rows[row];
        std::array<float*, 4> components{&output.x, &output.y, &output.z, &output.w};
        for (std::size_t column = 0; column < 4U; ++column) {
            const double value = At(inverse_view_projection, column, row);
            if (!std::isfinite(value)
                || value > (std::numeric_limits<float>::max)()
                || value < -(std::numeric_limits<float>::max)()) {
                return Reject(CameraFrameDiagnostic::NonFiniteResult);
            }
            *components[column] = static_cast<float>(value);
        }
    }
    frame.camera_world = {
        static_cast<float>(At(inverse_view, 3, 0)),
        static_cast<float>(At(inverse_view, 3, 1)),
        static_cast<float>(At(inverse_view, 3, 2)),
        0.0F,
    };
    constexpr float maximum_world_coordinate = 100000000.0F;
    if (!Finite(frame.camera_world.x)
        || !Finite(frame.camera_world.y)
        || !Finite(frame.camera_world.z)
        || std::fabs(frame.camera_world.x) > maximum_world_coordinate
        || std::fabs(frame.camera_world.y) > maximum_world_coordinate
        || std::fabs(frame.camera_world.z) > maximum_world_coordinate) {
        return Reject(CameraFrameDiagnostic::CameraOutOfRange);
    }
    return CameraFrameResult{CameraFrameDiagnostic::None, frame};
}

CameraFrameResult ReadCameraFrame(
    const ProcessMemoryReader& memory,
    const std::uintptr_t camera_address) noexcept
{
    if (camera_address == 0U) {
        return Reject(CameraFrameDiagnostic::NullCamera);
    }
    constexpr std::size_t final_offset =
        kNiCameraFrustumViewportOffset + sizeof(NiCameraRuntimeData2Abi);
    if (camera_address
        > (std::numeric_limits<std::uintptr_t>::max)() - final_offset) {
        return Reject(CameraFrameDiagnostic::CameraAddressOverflow);
    }

    CameraSnapshot snapshot;
    auto world_bytes = std::span{
        reinterpret_cast<std::uint8_t*>(&snapshot.world_to_camera),
        sizeof(snapshot.world_to_camera),
    };
    if (!memory.Read(
            camera_address + kNiCameraWorldToCameraOffset,
            world_bytes)) {
        return Reject(CameraFrameDiagnostic::WorldToCameraReadFailed);
    }
    auto projection_bytes = std::span{
        reinterpret_cast<std::uint8_t*>(&snapshot.projection),
        sizeof(snapshot.projection),
    };
    if (!memory.Read(
            camera_address + kNiCameraFrustumViewportOffset,
            projection_bytes)) {
        return Reject(CameraFrameDiagnostic::FrustumViewportReadFailed);
    }
    return BuildCameraFrame(snapshot);
}

} // namespace truth::runtime
