//
// Created by Ludw on 4/25/2024.
//

#include "../app.h"

VkFormat App::find_supported_format(const std::vector<VkFormat> &possible_formats, const VkImageTiling tiling,
                                    const VkFormatFeatureFlags features) const {
    for (const VkFormat format: possible_formats) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(phy_dev, format, &props);

        if ((tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) ||
            (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)) {
            return format;
        }
    }

    throw std::runtime_error("failed to find supported format.");
}

VkFormat App::find_depth_format() const {
    return find_supported_format(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

bool App::has_stencil_component(const VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

VCW_Image App::create_img(const VkExtent2D extent, const VkFormat format, const VkImageUsageFlags usage,
                          const VkMemoryPropertyFlags mem_props, const VkImageTiling tiling) const {
    VkExtent3D extent_3d = {extent.width, extent.height, 1};
    return create_img(extent_3d, format, usage, mem_props, tiling, VK_IMAGE_TYPE_2D);
}

VCW_Image App::create_img(const VkExtent3D extent, const VkFormat format, const VkImageUsageFlags usage,
                          const VkMemoryPropertyFlags mem_props, const VkImageTiling tiling,
                          const VkImageType img_type) const {
    VCW_Image img{};
    img.format = format;
    img.cur_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    img.extent = extent;

    VkImageCreateInfo img_info{};
    img_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    img_info.imageType = img_type;
    img_info.extent = img.extent;
    img_info.mipLevels = 1;
    img_info.arrayLayers = 1;
    img_info.format = img.format;
    img_info.tiling = tiling;
    img_info.initialLayout = img.cur_layout;
    img_info.usage = usage;
    img_info.samples = VK_SAMPLE_COUNT_1_BIT;
    img_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(dev, &img_info, nullptr, &img.img) != VK_SUCCESS)
        throw std::runtime_error("failed to create image.");

    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(dev, img.img, &mem_reqs);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = find_mem_type(mem_reqs.memoryTypeBits, mem_props);

    if (vkAllocateMemory(dev, &alloc_info, nullptr, &img.mem) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate image memory.");

    vkBindImageMemory(dev, img.img, img.mem, 0);

    return img;
}

VkImageView App::get_img_view(const VCW_Image img, const VkImageViewType img_view_type,
                              const VkImageSubresourceRange &subres_range) const {
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = img.img;
    view_info.viewType = img_view_type;
    view_info.format = img.format;
    view_info.subresourceRange = subres_range;

    VkImageView view;
    if (vkCreateImageView(dev, &view_info, nullptr, &view) != VK_SUCCESS)
        throw std::runtime_error("failed to create image view.");

    return view;
}

void App::create_img_view(VCW_Image *p_img, const VkImageViewType img_view_type,
                          const VkImageSubresourceRange &subres_range) const {
    p_img->view = get_img_view(*p_img, img_view_type, subres_range);
}

void App::create_img_view(VCW_Image *p_img, const VkImageAspectFlags aspect_flags) const {
    VkImageSubresourceRange subres_range = DEFAULT_SUBRESOURCE_RANGE;
    subres_range.aspectMask = aspect_flags;

    create_img_view(p_img, VK_IMAGE_VIEW_TYPE_2D, subres_range);
}

VkSampler App::create_sampler(const VkFilter filter, const VkSamplerAddressMode address_mode) const {
    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = filter;
    sampler_info.minFilter = filter;
    sampler_info.addressModeU = address_mode;
    sampler_info.addressModeV = address_mode;
    sampler_info.addressModeW = address_mode;
    sampler_info.anisotropyEnable = VK_TRUE;
    sampler_info.maxAnisotropy = phy_dev_props.limits.maxSamplerAnisotropy;
    sampler_info.borderColor = DEFAULT_SAMPLER_BORDER_COLOR;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    VkSampler sampler;
    if (vkCreateSampler(dev, &sampler_info, nullptr, &sampler) != VK_SUCCESS)
        throw std::runtime_error("failed to create sampler.");

    return sampler;
}

void App::create_sampler(VCW_Image *p_img, const VkFilter filter, const VkSamplerAddressMode address_mode) const {
    p_img->combined_img_sampler = true;
    p_img->sampler = create_sampler(filter, address_mode);
}

VkAccessFlags App::get_access_mask(const VkImageLayout layout) {
    switch (layout) {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            return 0;

        case VK_IMAGE_LAYOUT_PREINITIALIZED:
            return VK_ACCESS_HOST_WRITE_BIT;
        case VK_IMAGE_LAYOUT_GENERAL:
            return VK_ACCESS_SHADER_WRITE_BIT;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            return VK_ACCESS_TRANSFER_READ_BIT;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            return VK_ACCESS_TRANSFER_WRITE_BIT;


        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return VK_ACCESS_SHADER_READ_BIT;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            return VK_ACCESS_MEMORY_READ_BIT;

        default:
            throw std::invalid_argument("unsupported image layout.");
    }
}

void App::transition_img_layout(VkCommandBuffer cmd_buf, VCW_Image *p_img, const VkImageLayout layout,
                                const VkPipelineStageFlags src_stage, const VkPipelineStageFlags dst_stage) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = p_img->cur_layout;
    barrier.newLayout = layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = p_img->img;
    barrier.subresourceRange = DEFAULT_SUBRESOURCE_RANGE;

    barrier.srcAccessMask = get_access_mask(p_img->cur_layout);
    barrier.dstAccessMask = get_access_mask(layout);

    vkCmdPipelineBarrier(cmd_buf, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    p_img->cur_layout = layout;
}

void App::cp_buf_to_img(VkCommandBuffer cmd_buf, const VCW_Buffer &buf, const VCW_Image &img, const VkExtent3D extent) {
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource = DEFAULT_SUBRESOURCE_LAYERS;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = extent;

    vkCmdCopyBufferToImage(cmd_buf, buf.buf, img.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

void App::cp_buf_to_img(VkCommandBuffer cmd_buf, const VCW_Buffer &buf, const VCW_Image &img, const VkExtent2D extent) {
    const VkExtent3D extent_3d = {extent.width, extent.height, 1};
    cp_buf_to_img(cmd_buf, buf, img, extent_3d);
}

void App::cp_img_to_buf(VkCommandBuffer cmd_buf, const VCW_Image &img, const VCW_Buffer &buf, const VkExtent3D extent) {
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource = DEFAULT_SUBRESOURCE_LAYERS;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = extent;

    vkCmdCopyImageToBuffer(cmd_buf, img.img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buf.buf, 1, &region);
}

void App::cp_img_to_buf(VkCommandBuffer cmd_buf, const VCW_Image &img, const VCW_Buffer &buf, const VkExtent2D extent) {
    const VkExtent3D extent_3d = {extent.width, extent.height, 1};
    cp_img_to_buf(cmd_buf, img, buf, extent_3d);
}


void App::blit_img(VkCommandBuffer cmd_buf, const VCW_Image &src, const VkExtent3D src_extent, const VCW_Image &dst,
                   const VkExtent3D dst_extent, const VkFilter filter) {
    VkImageBlit region{};
    region.srcSubresource = DEFAULT_SUBRESOURCE_LAYERS;
    region.srcOffsets[0] = {0, 0, 0};
    region.srcOffsets[1] = {static_cast<int32_t>(src_extent.width), static_cast<int32_t>(src_extent.height), 1};
    region.dstSubresource = DEFAULT_SUBRESOURCE_LAYERS;
    region.dstOffsets[0] = {0, 0, 0};
    region.dstOffsets[1] = {static_cast<int32_t>(dst_extent.width), static_cast<int32_t>(dst_extent.height), 1};

    vkCmdBlitImage(cmd_buf, src.img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst.img,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, filter);
}

void App::blit_img(VkCommandBuffer cmd_buf, const VCW_Image &src, const VCW_Image &dst, const VkFilter filter) {
    blit_img(cmd_buf, src, src.extent, dst, dst.extent, filter);
}

void App::copy_img(VkCommandBuffer cmd_buf, const VCW_Image &src, const VCW_Image &dst) {
    VkImageCopy region{};
    region.srcSubresource = DEFAULT_SUBRESOURCE_LAYERS;
    region.srcOffset = {0, 0, 0};
    region.dstSubresource = DEFAULT_SUBRESOURCE_LAYERS;
    region.dstOffset = {0, 0, 0};
    region.extent = src.extent;

    vkCmdCopyImage(cmd_buf, src.img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst.img,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

void App::clean_up_img(const VCW_Image &img) const {
    if (img.combined_img_sampler)
        vkDestroySampler(dev, img.sampler, nullptr);

    vkDestroyImageView(dev, img.view, nullptr);

    vkDestroyImage(dev, img.img, nullptr);
    vkFreeMemory(dev, img.mem, nullptr);
}
