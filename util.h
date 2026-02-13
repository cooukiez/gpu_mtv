//
// Created by Ludw on 4/25/2024.
//

#include "inc.h"

#ifndef VCW_UTIL_H
#define VCW_UTIL_H

template<typename T>
std::vector<T> read_file(const std::string &filename);

std::string read_file_string(const std::string &filename);

void write_file(const std::string &filename, const void *data, std::streamsize size);

void append_to_file(const std::string &filename, const void *data, std::streamsize size);

float min_component(glm::vec3 v);

float min_component(glm::vec4 v);

float max_component(glm::vec3 v);

float max_component(glm::vec4 v);

#endif //VCW_UTIL_H
