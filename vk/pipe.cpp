//
// Created by Ludw on 4/25/2024.
//

#include "../app.h"

void App::create_rendp() {
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    VkRenderPassCreateInfo rendp_info{};
    rendp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rendp_info.subpassCount = 1;
    rendp_info.pSubpasses = &subpass;

    if (vkCreateRenderPass(dev, &rendp_info, nullptr, &rendp) != VK_SUCCESS)
        throw std::runtime_error("failed to create render pass.");
}

std::vector<uint32_t> App::compile_shader(const std::string& source, shaderc_shader_kind kind, const char* entry_point) {
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
    options.SetTargetSpirv(shaderc_spirv_version_1_0);

    shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(source, kind, entry_point, options);

    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        std::cerr << "shader compilation error: " << result.GetErrorMessage() << std::endl;
        throw std::runtime_error("shader compilation failed.");
    }

    return {result.cbegin(), result.cend()};
}

VkShaderModule App::create_shader_mod(const std::vector<uint32_t> &code) const {
    VkShaderModuleCreateInfo mod_info{};
    mod_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    mod_info.codeSize = sizeof(uint32_t) * code.size();
    mod_info.pCode = code.data();

    VkShaderModule mod;
    if (vkCreateShaderModule(dev, &mod_info, nullptr, &mod) != VK_SUCCESS)
        throw std::runtime_error("failed to create shader module.");

    return mod;
}


void App::create_frame_buf() {
    VkFramebufferCreateInfo frame_buf_info{};
    frame_buf_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    frame_buf_info.renderPass = rendp;
    frame_buf_info.width = render_extent.width;
    frame_buf_info.height = render_extent.height;
    frame_buf_info.layers = 1;

    if (vkCreateFramebuffer(dev, &frame_buf_info, nullptr, &frame_buf) != VK_SUCCESS)
        throw std::runtime_error("failed to create framebuffer.");
}

void App::clean_up_pipe() const {
    vkDestroyPipeline(dev, pipe, nullptr);
    vkDestroyPipelineLayout(dev, pipe_layout, nullptr);
}