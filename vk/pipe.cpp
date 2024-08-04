//
// Created by Ludw on 4/25/2024.
//

#include "../app.h"

VkShaderModule App::create_shader_mod(const std::vector<char> &code) const {
    VkShaderModuleCreateInfo mod_info{};
    mod_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    mod_info.codeSize = code.size();
    mod_info.pCode = reinterpret_cast<const uint32_t *>(code.data());

    VkShaderModule mod;
    if (vkCreateShaderModule(dev, &mod_info, nullptr, &mod) != VK_SUCCESS)
        throw std::runtime_error("failed to create shader module.");

    return mod;
}


void App::clean_up_pipe() const {
    vkDestroyPipeline(dev, pipe, nullptr);
    vkDestroyPipelineLayout(dev, pipe_layout, nullptr);
}