#pragma once

#include <cstdint>

namespace truth::render::detail {

[[nodiscard]] float SkyFieldLatticeHash3D(
    std::int32_t x,
    std::int32_t y,
    std::int32_t z) noexcept;

[[nodiscard]] float SkyFieldValueNoise3D(float x, float y, float z) noexcept;

}  // namespace truth::render::detail
