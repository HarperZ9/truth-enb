#include "truth/render/ReferenceRenderer.hpp"

#include <array>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

[[nodiscard]] bool ParseDimension(const char* text, std::uint32_t& value) noexcept {
  const std::string input{text};
  const auto result = std::from_chars(
      input.data(), input.data() + input.size(), value);
  return result.ec == std::errc{} && result.ptr == input.data() + input.size();
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3 && argc != 5) {
    std::cerr << "usage: truth_reference_renderer <shader.hlsl> <output-directory> "
                 "[width height]\n";
    return 2;
  }

  std::uint32_t width = 256U;
  std::uint32_t height = 128U;
  if (argc == 5
      && (!ParseDimension(argv[3], width) || !ParseDimension(argv[4], height))) {
    std::cerr << "width and height must be unsigned integers\n";
    return 2;
  }

  const std::filesystem::path shader_path{argv[1]};
  const std::filesystem::path output_directory{argv[2]};
  constexpr std::array scenes{
      truth::render::ReferenceScene::day,
      truth::render::ReferenceScene::dusk,
      truth::render::ReferenceScene::clear_night_aurora,
      truth::render::ReferenceScene::storm,
  };

  for (const auto scene : scenes) {
    const auto result = truth::render::RenderWarpReference(
        scene, shader_path, width, height);
    if (result.status != truth::render::ReferenceRenderStatus::rendered) {
      std::cerr << result.diagnostic << '\n';
      return 1;
    }
    const auto name = truth::render::ReferenceSceneName(scene);
    const auto output_path = output_directory / (std::string{name} + ".ppm");
    std::string diagnostic;
    if (!truth::render::WriteBinaryPpm(result.image, output_path, diagnostic)) {
      std::cerr << diagnostic << '\n';
      return 1;
    }
    std::cout << name << " sha256=" << result.sha256_hex << " path="
              << output_path.string() << '\n';
  }
  return 0;
}
