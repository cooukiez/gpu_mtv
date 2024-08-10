cmake_minimum_required(VERSION 3.8)

project(gpu_mtv)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++20")

set(IMGUI_DIR imgui)
set(VSS_DIR vss)

file(GLOB MAIN_SOURCES *.cpp)
file(GLOB_RECURSE VK_SOURCES vk/*.cpp)
file(GLOB_RECURSE RENDER_SOURCES render/*.cpp)

set(SOURCES)
list(APPEND SOURCES ${MAIN_SOURCES} ${VK_SOURCES} ${RENDER_SOURCES})

add_executable(main ${SOURCES})

find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(Vulkan REQUIRED)
find_package(Stb REQUIRED)
find_package(tinyobjloader REQUIRED)
find_package(unofficial-shaderc REQUIRED)

target_link_libraries(main Vulkan::Vulkan)
target_link_libraries(main glfw)
target_link_libraries(main glm::glm)
target_link_libraries(main tinyobjloader::tinyobjloader)
target_link_libraries(main unofficial::shaderc::shaderc)

target_include_directories(main PRIVATE ${Stb_INCLUDE_DIR})
target_include_directories(main PRIVATE ${VSS_DIR}/include)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    message(STATUS "Detected MinGW compiler.")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Ofast")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    message(STATUS "Detected MSVC compiler.")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /O2")
endif()

set(SHADERS
        ${CMAKE_SOURCE_DIR}/shader.vert
        ${CMAKE_SOURCE_DIR}/shader.geom
        ${CMAKE_SOURCE_DIR}/shader.frag
)

set(SHADERS_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/shaders)

add_custom_command(
        OUTPUT ${SHADERS_OUTPUT_DIRECTORY}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADERS_OUTPUT_DIRECTORY}
        COMMAND ${CMAKE_COMMAND} -E copy ${SHADERS} ${SHADERS_OUTPUT_DIRECTORY}
        COMMENT "Copying shaders to output directory."
        DEPENDS ${SHADERS}
)

add_custom_target(copy_shaders ALL DEPENDS ${SHADERS_OUTPUT_DIRECTORY})
add_dependencies(main copy_shaders)


file(COPY ${CMAKE_SOURCE_DIR}/models DESTINATION ${CMAKE_BINARY_DIR})
