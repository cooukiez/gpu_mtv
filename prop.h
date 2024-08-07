//
// Created by Ludw on 4/25/2024.
//

#ifndef VCW_PROP_H
#define VCW_PROP_H

#define WINDOW_TITLE "mesh to voxel converter"

#define MAX_FRAMES_IN_FLIGHT 2

#define APP_NAME "mesh to voxel converter"
#define ENGINE_NAME "No Engine"

#define VERBOSE
#define VALIDATION

// set max allowed textures
#define DESCRIPTOR_TEXTURE_COUNT 32

const std::vector<const char *> val_layers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char *> dev_exts = {
    // VK_KHR_SWAPCHAIN_EXTENSION_NAME  // not needed
    VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME
    // VK_EXT_PIPELINE_ROBUSTNESS_EXTENSION_NAME  // not available
    // VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME  // not required anymore
};

constexpr VkImageSubresourceRange DEFAULT_SUBRESOURCE_RANGE = {
    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1
};

constexpr VkImageSubresourceLayers DEFAULT_SUBRESOURCE_LAYERS = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
constexpr VkBorderColor DEFAULT_SAMPLER_BORDER_COLOR = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

#endif //VCW_PROP_H
