//
// Created by Ludw on 4/25/2024.
//
#include "app.h"

int main() {
    VoxelizeParams params{};
    params.chunk_res = 256;
    params.chunk_size = static_cast<uint32_t>(pow(256, 3));
    params.load_textures = false;

    App app(params);

    try {
        app.run();
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}