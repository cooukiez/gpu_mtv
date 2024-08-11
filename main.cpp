//
// Created by Ludw on 4/25/2024.
//
#include "app.h"

#define ARG_VALID EXIT_SUCCESS
#define ARG_INVALID EXIT_FAILURE
#define NEXT_ARG_USED 2

void print_usage() {
    std::cout << "Usage: gpu_mtv [options] -i <input_file> -o <output_file>" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -h               Display this help message." << std::endl;
    std::cout << "  -r <resolution>  Set resolution of voxel grid." << std::endl;
    std::cout << "                   Defaults to 256 cubic." << std::endl;
    std::cout << "  -m               Morton encode the output." << std::endl;
    std::cout << "  -c <method>      Compression method, available: [rle]" << std::endl;
    std::cout << "  -z <path>        Specify folder with the materials. (the corresponding .mtl file)" << std::endl;
    std::cout << "                   Defaults to the directory of the .obj file." << std::endl;
    std::cout << std::endl;
    std::cout << "Currently unsupported, will be added later." << std::endl;
    std::cout << "  -t               Use textures and generate color palette." << std::endl;
    std::cout << std::endl;
}

int string_to_int(std::string &string, uint32_t *p_int) {
    try {
        *p_int = static_cast<uint32_t>(std::stoul(string));
        return EXIT_SUCCESS;
    } catch (const std::exception &e) {
        std::cerr << std::endl << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}

int evaluate_args(std::string &arg, std::string &next_arg, VoxelizeParams *p_params) {
    if (arg == "-h") {
        return ARG_INVALID;
    } else if (arg == "-r") {
        return string_to_int(next_arg, &p_params->chunk_res) == EXIT_SUCCESS ? NEXT_ARG_USED : ARG_INVALID;
    } else if (arg == "-m") {
        p_params->morton_encode = true;
        return ARG_VALID;
    } else if (arg == "-c") {
        if (next_arg == "rle") {
            p_params->run_length_encode = true;
            return ARG_VALID;
        } else {
            return ARG_INVALID;
        }
    } else if (arg == "-z") {
        std::filesystem::path path(next_arg);
        if (std::filesystem::exists(path)) {
            if (std::filesystem::is_directory(path)) {
                p_params->material_dir = next_arg;
                return NEXT_ARG_USED;
            } else {
                std::cerr << std::endl << "specified material dir is not a directory." << std::endl;
                return ARG_INVALID;
            }
        } else {
            std::cerr << std::endl << "specified material dir does not exist." << std::endl;
            return ARG_INVALID;
        }
    } else if (arg == "-i") {
        std::filesystem::path path(next_arg);

        if (std::filesystem::exists(path)) {
            if (path.extension() == ".obj") {
                if (p_params->material_dir == "")
                    p_params->material_dir = path.parent_path().string();

                p_params->input_file = next_arg;
                return NEXT_ARG_USED;
            } else {
                std::cerr << std::endl << "input file must be an .obj file." << std::endl;
                return ARG_INVALID;
            }
        } else {
            std::cerr << std::endl << "specified input file does not exist." << std::endl;
            return ARG_INVALID;
        }
    } else if (arg == "-o") {
        p_params->output_file = next_arg;
        return NEXT_ARG_USED;
    } else {
        return ARG_INVALID;
    }
}

int validate_args(VoxelizeParams *p_params) {
    if (p_params->input_file == "") {
        std::cerr << std::endl << "no input file specified." << std::endl;
        return ARG_INVALID;
    }

    if (p_params->material_dir == "") {
        std::cerr << std::endl << "no material directory specified." << std::endl;
        return ARG_INVALID;
    }

    if (p_params->output_file == "") {
        std::cerr << std::endl << "no output file specified." << std::endl;
        return ARG_INVALID;
    }

    return ARG_VALID;
}

void print_params(VoxelizeParams p_params) {
    std::cout << std::endl << "--- Voxelization parameters ---" << std::endl;
    std::cout << "chunk resolution: " << p_params.chunk_res << std::endl;
    std::cout << "chunk size: " << p_params.chunk_size << std::endl;

    std::cout << "input file: " << p_params.input_file << std::endl;
    std::cout << "output file: " << p_params.output_file << std::endl;

    std::cout << "material dir: " << p_params.material_dir << std::endl;

    std::cout << "morton encode: " << p_params.morton_encode << std::endl;
    std::cout << "run length encode: " << p_params.run_length_encode << std::endl;
}

int main(int argc, char *argv[]) {
    VoxelizeParams params{};
    for (int i = 1; i < argc; i++) {
        std::string arg = std::string(argv[i]);
        std::string next_arg = std::string(argv[i + 1]);

        int result = evaluate_args(arg, next_arg, &params);

        if (result == ARG_INVALID) {
            print_usage();
            return EXIT_FAILURE;
        }

        if (result == NEXT_ARG_USED)
            i++;

        if (i == (argc - 2) && result != NEXT_ARG_USED) {
            std::cerr << std::endl << "invalid arguments" << std::endl;
            print_usage();
            return EXIT_FAILURE;
        }
    }

    if (params.chunk_res == 0)
        params.chunk_res = 256;
    params.chunk_size = static_cast<uint32_t>(pow(params.chunk_res, 3));

    if (validate_args(&params) == ARG_INVALID) {
        print_usage();
        return EXIT_FAILURE;
    }

    print_params(params);

    std::cout << std::endl;
    std::cout << "##############################" << std::endl;
    std::cout << "#        Voxelization        #" << std::endl;
    std::cout << "##############################" << std::endl;

    App app{};
    app.params = params;

    try {
        app.run();
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}