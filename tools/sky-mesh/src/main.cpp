// SPDX-License-Identifier: GPL-3.0-or-later

#include <truth/sky_mesh/AtmosphereMesh.hpp>

#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

namespace fs = std::filesystem;

namespace {

void usage(std::ostream& output) {
    output << "usage: truth_sky_mesh_generate --output-root <directory> [--emit-obj]\n";
}

} // namespace

int main(const int argc, char** argv) {
    try {
        std::optional<fs::path> outputRoot;
        bool emitObj = false;

        for (int index = 1; index < argc; ++index) {
            const std::string argument = argv[index];
            if (argument == "--output-root") {
                if (outputRoot.has_value() || index + 1 >= argc) {
                    throw std::invalid_argument("--output-root requires exactly one directory");
                }
                outputRoot = fs::path(argv[++index]);
            } else if (argument == "--emit-obj") {
                if (emitObj) {
                    throw std::invalid_argument("--emit-obj may be specified only once");
                }
                emitObj = true;
            } else if (argument == "--help" || argument == "-h") {
                usage(std::cout);
                return 0;
            } else {
                throw std::invalid_argument("unknown argument: " + argument);
            }
        }

        if (!outputRoot.has_value() || outputRoot->empty()) {
            throw std::invalid_argument("--output-root is required");
        }

        const auto result = truth::sky_mesh::generateAtmosphereFallback({*outputRoot, emitObj});
        std::cout << "generated " << result.nifPath.string() << '\n'
                  << "sha256 " << result.sha256 << '\n'
                  << "size " << result.sizeBytes << '\n'
                  << "topology " << result.vertices << " vertices / "
                  << result.triangles << " triangles\n"
                  << "manifest " << result.manifestPath.string() << '\n';
        if (!result.objPath.empty()) {
            std::cout << "diagnostic " << result.objPath.string() << '\n';
        }
        return 0;
    } catch (const std::invalid_argument& error) {
        std::cerr << "argument error: " << error.what() << '\n';
        usage(std::cerr);
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "generation failed: " << error.what() << '\n';
        return 1;
    }
}
