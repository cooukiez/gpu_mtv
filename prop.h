//
// Created by Ludw on 4/25/2024.
//

#ifndef VCW_PROP_H
#define VCW_PROP_H

#define WINDOW_TITLE "mesh to voxel converter"

// chaneging size needs fixing
#define CHUNK_SIDE_LENGTH 256
#define CHUNK_SIZE (CHUNK_SIDE_LENGTH * CHUNK_SIDE_LENGTH * CHUNK_SIDE_LENGTH)

#define GRID_RESOLUTION 256

#define SECTOR_COUNT (GRID_RESOLUTION / CHUNK_SIDE_LENGTH)

#define INITIAL_WIDTH CHUNK_SIDE_LENGTH
#define INITIAL_HEIGHT CHUNK_SIDE_LENGTH

#define MAX_FRAMES_IN_FLIGHT 2

#define APP_NAME "mesh to voxel converter"
#define ENGINE_NAME "No Engine"

#define VERBOSE
#define VALIDATION

const std::vector<const char *> val_layers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char *> dev_exts = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME
    // VK_EXT_PIPELINE_ROBUSTNESS_EXTENSION_NAME // not available
    // VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME // not required anymore
};

//
// scaling resolution
//
#define FULLSCREEN_RES_DIV 1
// #define IMGUI_SCALE_OVERLAY

constexpr VkFormat PREFERRED_FORMAT = VK_FORMAT_B8G8R8A8_SRGB;
constexpr VkColorSpaceKHR PREFERRED_COLOR_SPACE = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
constexpr VkPresentModeKHR PREFERRED_PRES_MODE = VK_PRESENT_MODE_FIFO_KHR;
constexpr VkCompositeAlphaFlagBitsKHR PREFERRED_COMPOSITE_ALPHA = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

constexpr VkImageUsageFlags SWAP_IMG_USAGE = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

constexpr VkComponentMapping DEFAULT_COMPONENT_MAPPING = {
    .r = VK_COMPONENT_SWIZZLE_IDENTITY, .g = VK_COMPONENT_SWIZZLE_IDENTITY, .b = VK_COMPONENT_SWIZZLE_IDENTITY,
    .a = VK_COMPONENT_SWIZZLE_IDENTITY
};
constexpr VkImageSubresourceRange DEFAULT_SUBRESOURCE_RANGE = {
    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1
};
constexpr VkImageSubresourceLayers DEFAULT_SUBRESOURCE_LAYERS = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};

constexpr VkBorderColor DEFAULT_SAMPLER_BORDER_COLOR = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

// Rasterization Stage
constexpr VkPolygonMode POLYGON_MODE = VK_POLYGON_MODE_FILL;
constexpr VkShaderStageFlags PUSH_CONSTANTS_STAGE = VK_SHADER_STAGE_ALL_GRAPHICS;
constexpr VkFrontFace FRONT_FACE = VK_FRONT_FACE_COUNTER_CLOCKWISE;
constexpr VkCullModeFlags CULL_MODE = VK_CULL_MODE_NONE;

#define DESCRIPTOR_TEXTURE_COUNT 32

#endif //VCW_PROP_H
