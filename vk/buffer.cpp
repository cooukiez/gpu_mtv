//
// Created by Ludw on 4/25/2024.
//

#include "../app.h"

uint32_t App::find_mem_type(const uint32_t type_filter, const VkMemoryPropertyFlags mem_flags) const {
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(phy_dev, &mem_props);

    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) && (mem_props.memoryTypes[i].propertyFlags & mem_flags) == mem_flags) {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type.");
}

VCW_Buffer App::create_buf(const VkDeviceSize size, const VkBufferUsageFlags usage,
                           const VkMemoryPropertyFlags mem_props) const {
    VCW_Buffer buf{};
    buf.size = size;
    buf.cur_access_mask = 0;

    VkBufferCreateInfo buf_info{};
    buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf_info.size = buf.size;
    buf_info.usage = usage;
    buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(dev, &buf_info, nullptr, &buf.buf) != VK_SUCCESS)
        throw std::runtime_error("failed to create buffer.");

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(dev, buf.buf, &mem_reqs);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = find_mem_type(mem_reqs.memoryTypeBits, mem_props);

    if (vkAllocateMemory(dev, &alloc_info, nullptr, &buf.mem) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate buffer memory.");

    vkBindBufferMemory(dev, buf.buf, buf.mem, 0);

    return buf;
}

void App::map_buf(VCW_Buffer *p_buf) const {
    vkMapMemory(dev, p_buf->mem, 0, p_buf->size, 0, &p_buf->p_mapped_mem);

    if (p_buf->p_mapped_mem == nullptr)
        throw std::runtime_error("failed to map buffer memory.");
}

void App::unmap_buf(VCW_Buffer *p_buf) const {
    vkUnmapMemory(dev, p_buf->mem);
    p_buf->p_mapped_mem = nullptr;
}

void App::cp_data_to_buf(VCW_Buffer *p_buf, const void *p_data) const {
    if (p_buf == nullptr || p_data == nullptr)
        throw std::invalid_argument("cannot copy data from buffer, argument is nullptr.");

    map_buf(p_buf);
    memcpy(p_buf->p_mapped_mem, p_data, p_buf->size);
    unmap_buf(p_buf);
}

void App::cp_data_from_buf(VCW_Buffer *p_buf, void *p_data) const {
    if (p_buf == nullptr || p_data == nullptr)
        throw std::invalid_argument("cannot copy data from buffer, argument is nullptr.");

    map_buf(p_buf);
    memcpy(p_data, p_buf->p_mapped_mem, p_buf->size);
    unmap_buf(p_buf);
}

// allocates new command buffers
void App::cp_buf(const VCW_Buffer &src_buf, const VCW_Buffer &dst_buf) {
    if (src_buf.size != dst_buf.size)
        throw std::invalid_argument("src buffer size is not equal to dst buffer size.");

    VkCommandBuffer cmd_buf = begin_single_time_cmd();

    VkBufferCopy cp_region{};
    cp_region.size = src_buf.size;
    vkCmdCopyBuffer(cmd_buf, src_buf.buf, dst_buf.buf, 1, &cp_region);

    end_single_time_cmd(cmd_buf);
}

void App::buffer_memory_barrier(VkCommandBuffer cmd_buf, VCW_Buffer *p_buf, const VkAccessFlags access_mask,
                                const VkPipelineStageFlags src_stage, const VkPipelineStageFlags dst_stage) {
    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = p_buf->cur_access_mask;
    barrier.dstAccessMask = access_mask;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = p_buf->buf;
    barrier.offset = 0;
    barrier.size = p_buf->size;

    vkCmdPipelineBarrier(cmd_buf, src_stage, dst_stage, 0, 0, nullptr, 1, &barrier, 0, nullptr);

    p_buf->cur_access_mask = access_mask;
}

void App::clean_up_buf(const VCW_Buffer &buf) const {
    vkDestroyBuffer(dev, buf.buf, nullptr);
    vkFreeMemory(dev, buf.mem, nullptr);
}
