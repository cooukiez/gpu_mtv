//
// Created by Ludw on 4/25/2024.
//

#include "../app.h"

void App::create_cmd_pool() {
    VkCommandPoolCreateInfo cmd_pool_info{};
    cmd_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmd_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cmd_pool_info.queueFamilyIndex = qf_indices.qf_graph.value();

    if (vkCreateCommandPool(dev, &cmd_pool_info, nullptr, &cmd_pool) != VK_SUCCESS)
        throw std::runtime_error("failed to create graphics command pool.");
}

VkCommandBuffer App::begin_cmd() const {
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = cmd_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer cmd_buf;
    vkAllocateCommandBuffers(dev, &alloc_info, &cmd_buf);

    return cmd_buf;
}

VkCommandBuffer App::begin_single_time_cmd() const {
    VkCommandBuffer cmd_buf = begin_cmd();

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(cmd_buf, &beginInfo);

    return cmd_buf;
}

void App::end_single_time_cmd(VkCommandBuffer cmd_buf) const {
    vkEndCommandBuffer(cmd_buf);

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd_buf;

    vkQueueSubmit(q_graph, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(q_graph);

    vkFreeCommandBuffers(dev, cmd_pool, 1, &cmd_buf);
}

void App::create_cmd_bufs() {
    cmd_bufs.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = cmd_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = static_cast<uint32_t>(cmd_bufs.size());

    if (vkAllocateCommandBuffers(dev, &alloc_info, cmd_bufs.data()) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate command buffers.");
}

void App::render() {
    vkWaitForFences(dev, 1, &fens[cur_frame], VK_TRUE, UINT64_MAX);

    update_bufs(cur_frame);

    vkResetFences(dev, 1, &fens[cur_frame]);

    vkResetCommandBuffer(cmd_bufs[cur_frame], /*VkCommandBufferResetFlagBits*/ 0);
    record_cmd_buf(cmd_bufs[cur_frame], img_index);

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;


    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd_bufs[cur_frame];

    if (vkQueueSubmit(q_graph, 1, &submit, fens[cur_frame]) != VK_SUCCESS)
        throw std::runtime_error("failed to submit render command buffer.");

    VkPresentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = signal_semps;

    const VkSwapchainKHR swaps[] = {swap};
    present.swapchainCount = 1;
    present.pSwapchains = swaps;

    present.pImageIndices = &img_index;

    vkQueuePresentKHR(q_pres, &present);

    fetch_queries(img_index);

    cur_frame = (cur_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}
