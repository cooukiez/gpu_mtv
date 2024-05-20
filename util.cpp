//
// Created by Ludw on 4/25/2024.
//

#include "util.h"

template std::vector<char> read_file<char>(const std::string &);

template std::vector<uint8_t> read_file<uint8_t>(const std::string &);

template<typename T>
std::vector<T> read_file(const std::string &filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open())
        throw std::runtime_error("failed to open file.");

    size_t file_size = static_cast<size_t>(file.tellg());
    std::vector<T> buf(file_size / sizeof(T));

    file.seekg(0);
    file.read(reinterpret_cast<char *>(buf.data()), static_cast<std::streamsize>(file_size));

    file.close();

    return buf;
}

void write_file(const std::string &filename, const void *data, const std::streamsize size) {
    std::ofstream file(filename, std::ios::out | std::ios::binary);

    if (!file.is_open())
        throw std::runtime_error("failed to open file.");

    file.write(static_cast<const char*>(data), size);
    file.close();

    if (file.fail())
       throw std::runtime_error("failed to write to file.");
}

float min_component(const glm::vec3 v) {
    return std::min(v.x, std::min(v.y, v.z));
}

float min_component(const glm::vec4 v) {
    return std::min(v.x, std::min(v.y, std::min(v.z, v.w)));
}

float max_component(const glm::vec3 v) {
    return std::max(v.x, std::max(v.y, v.z));
}

float max_component(const glm::vec4 v) {
    return std::max(v.x, std::max(v.y, std::max(v.z, v.w)));
}
