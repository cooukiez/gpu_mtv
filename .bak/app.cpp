//
// Created by Ludw on 4/25/2024.
//

#include "app.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

void App::init_app() {
    //
    // vulkan core initialization
    //
    create_inst();
    setup_debug_msg();
    create_surf();

    pick_phy_dev();
    create_dev();

    create_swap();

    //
    // pipeline creation
    //
    create_rendp();
    create_desc_pool_layout();
    create_pipe();

    create_cmd_pool();

#ifdef BIND_SAMPLE_TEXTURE
    create_tex_img();
#endif

    load_model();

    create_vert_buf();
    create_index_buf();

    create_frame_bufs(swap_imgs);

    create_desc_pool(MAX_FRAMES_IN_FLIGHT);

    write_desc_pool();

    create_cmd_bufs();
    create_sync();
}

void App::load_model() {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, "models/sponza/sponza.obj", "models/sponza"))
        throw std::runtime_error(warn + err);

    // std::cout << warn << err << std::endl;

    for (const auto &shape: shapes) {
        for (const auto &index: shape.mesh.indices) {
            Vertex vertex{};

            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            vertex.uv = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                attrib.texcoords[2 * index.texcoord_index + 1]
            };

            vertices.push_back(vertex);
            indices.push_back(indices.size());
        }
    }
}

void App::create_vert_buf() {
    VkDeviceSize buf_size = sizeof(vertices[0]) * vertices.size();

    VCW_Buffer staging_buf = create_buf(buf_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    cp_data_to_buf(&staging_buf, (void *) vertices.data());

    vert_buf = create_buf(buf_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    cp_buf(staging_buf, vert_buf);

    clean_up_buf(staging_buf);
}

void App::create_index_buf() {
    VkDeviceSize buf_size = sizeof(indices[0]) * indices.size();

    VCW_Buffer staging_buf = create_buf(buf_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    cp_data_to_buf(&staging_buf, (void *) indices.data());

    index_buf = create_buf(buf_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    cp_buf(staging_buf, index_buf);

    clean_up_buf(staging_buf);
}

void App::create_tex_img() {
    int tex_width, tex_height, tex_channels;
    stbi_uc *pixels = stbi_load("textures/texture.jpg", &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);
    VkDeviceSize img_size = tex_width * tex_height * 4;

    if (!pixels)
        throw std::runtime_error("failed to load texture image.");

    VCW_Buffer staging_buf = create_buf(img_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    cp_data_to_buf(&staging_buf, pixels);

    stbi_image_free(pixels);

    VkExtent2D extent = {(uint32_t) tex_width, (uint32_t) tex_height};
    tex_img = create_img(extent, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
                         VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkCommandBuffer cmd_buf = begin_single_time_cmd();
    transition_img_layout(cmd_buf, &tex_img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                          VK_PIPELINE_STAGE_TRANSFER_BIT);
    cp_buf_to_img(cmd_buf, staging_buf, tex_img, extent);
    transition_img_layout(cmd_buf, &tex_img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    end_single_time_cmd(cmd_buf);

    create_img_view(&tex_img, VK_IMAGE_ASPECT_COLOR_BIT);
    create_sampler(&tex_img, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);

    clean_up_buf(staging_buf);
}

void App::create_desc_pool_layout() {
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    uint32_t last_binding = 0;

#ifdef BIND_SAMPLE_TEXTURE
    VkDescriptorSetLayoutBinding sampler_layout_binding{};
    sampler_layout_binding.binding = last_binding;
    sampler_layout_binding.descriptorCount = 1;
    sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sampler_layout_binding.pImmutableSamplers = nullptr;
    sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings.push_back(sampler_layout_binding);
    last_binding++;
#endif

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        add_desc_set_layout(static_cast<uint32_t>(bindings.size()), bindings.data());
    }

    add_pool_size(MAX_FRAMES_IN_FLIGHT, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
}

void App::create_pipe() {
    auto vert_code = read_file("vert.spv");
    auto frag_code = read_file("frag.spv");

    VkShaderModule vert_module = create_shader_mod(vert_code);
    VkShaderModule frag_module = create_shader_mod(frag_code);

    VkPipelineShaderStageCreateInfo vert_stage_info{};
    vert_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_stage_info.module = vert_module;
    vert_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo frag_stage_info{};
    frag_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_stage_info.module = frag_module;
    frag_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo stages[] = {vert_stage_info, frag_stage_info};

    VkPipelineVertexInputStateCreateInfo vert_input_info{};
    vert_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    auto binding_desc = Vertex::get_binding_desc();
    auto attrib_descs = Vertex::get_attrib_descs();

    vert_input_info.vertexBindingDescriptionCount = 1;
    vert_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrib_descs.size());
    vert_input_info.pVertexBindingDescriptions = &binding_desc;
    vert_input_info.pVertexAttributeDescriptions = attrib_descs.data();

    VkPipelineInputAssemblyStateCreateInfo input_asm_info{};
    input_asm_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_asm_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_asm_info.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewport_info{};
    viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_info.viewportCount = 1;
    viewport_info.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo raster_info{};
    raster_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster_info.depthClampEnable = DEPTH_CLAMP_ENABLE;
    raster_info.rasterizerDiscardEnable = RASTERIZER_DISCARD_ENABLE;
    raster_info.polygonMode = POLYGON_MODE;
    raster_info.lineWidth = 1.0f;
    raster_info.cullMode = CULL_MODE;
    raster_info.frontFace = FRONT_FACE;
    raster_info.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisample_info{};
    multisample_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_info.sampleShadingEnable = VK_FALSE;
    multisample_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blend_attach{};
    blend_attach.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT;
    blend_attach.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo blend_info{};
    blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend_info.logicOpEnable = VK_FALSE;
    blend_info.logicOp = VK_LOGIC_OP_COPY;
    blend_info.attachmentCount = 1;
    blend_info.pAttachments = &blend_attach;
    blend_info.blendConstants[0] = 0.0f;
    blend_info.blendConstants[1] = 0.0f;
    blend_info.blendConstants[2] = 0.0f;
    blend_info.blendConstants[3] = 0.0f;

    std::vector<VkDynamicState> dynamic_states = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamic_state_info{};
    dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state_info.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
    dynamic_state_info.pDynamicStates = dynamic_states.data();

    VkPushConstantRange push_const_range{};
    push_const_range.stageFlags = PUSH_CONSTANTS_STAGE;
    push_const_range.offset = 0;
    push_const_range.size = sizeof(VCW_PushConstants);

    VkPipelineLayoutCreateInfo pipe_layout_info{};
    pipe_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipe_layout_info.setLayoutCount = static_cast<uint32_t>(desc_set_layouts.size());
    pipe_layout_info.pSetLayouts = desc_set_layouts.data();
    pipe_layout_info.pushConstantRangeCount = 1;
    pipe_layout_info.pPushConstantRanges = &push_const_range;

    if (vkCreatePipelineLayout(dev, &pipe_layout_info, nullptr, &pipe_layout) != VK_SUCCESS)
        throw std::runtime_error("failed to create pipeline layout.");

    VkGraphicsPipelineCreateInfo pipe_info{};
    pipe_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipe_info.stageCount = 2;
    pipe_info.pStages = stages;
    pipe_info.pVertexInputState = &vert_input_info;
    pipe_info.pInputAssemblyState = &input_asm_info;
    pipe_info.pViewportState = &viewport_info;
    pipe_info.pRasterizationState = &raster_info;
    pipe_info.pMultisampleState = &multisample_info;
    pipe_info.pColorBlendState = &blend_info;
    pipe_info.pDynamicState = &dynamic_state_info;
    pipe_info.layout = pipe_layout;
    pipe_info.renderPass = rendp;
    pipe_info.subpass = 0;
    pipe_info.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipe_info, nullptr, &pipe) != VK_SUCCESS)
        throw std::runtime_error("failed to create graphics pipeline.");

    vkDestroyShaderModule(dev, frag_module, nullptr);
    vkDestroyShaderModule(dev, vert_module, nullptr);
}

void App::write_desc_pool() {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        uint32_t last_binding = 0;
#ifdef BIND_SAMPLE_TEXTURE
        write_img_desc_binding(tex_img, i, last_binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
#endif
    }
}

void App::update_bufs(uint32_t index_inflight_frame, float pane_index) {
    float thickness = 0.1f;
    float slicingPlaneZ = -1.0f + 3.0f / pane_index;

    float minZ = slicingPlaneZ - thickness / 2.0f;
    float maxZ = slicingPlaneZ + thickness / 2.0f;

    minZ = -1.0f;
    maxZ = 1.0f;

    push_const.proj = glm::ortho(-0.5f, 50.0f, -0.5f, 50.0f, minZ, maxZ);
}

void App::record_cmd_buf(VkCommandBuffer cmd_buf, uint32_t img_index) {
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(cmd_buf, &begin_info) != VK_SUCCESS)
        throw std::runtime_error("failed to begin recording command buffer.");

    VkRenderPassBeginInfo rendp_begin_info{};
    rendp_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rendp_begin_info.renderPass = rendp;
    rendp_begin_info.framebuffer = frame_bufs[img_index];
    rendp_begin_info.renderArea.offset = {0, 0};
    rendp_begin_info.renderArea.extent = render_extent;

    std::array<VkClearValue, 1> clear_values{};
    clear_values[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};

    rendp_begin_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
    rendp_begin_info.pClearValues = clear_values.data();

    vkCmdBeginRenderPass(cmd_buf, &rendp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) render_extent.width;
    viewport.height = (float) render_extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd_buf, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = render_extent;
    vkCmdSetScissor(cmd_buf, 0, 1, &scissor);

    VkBuffer vert_bufs[] = {vert_buf.buf};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd_buf, 0, 1, vert_bufs, offsets);

    vkCmdBindIndexBuffer(cmd_buf, index_buf.buf, 0, VK_INDEX_TYPE_UINT16);

    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe_layout, 0, 1,
                            &desc_sets[cur_frame], 0, nullptr);

    vkCmdPushConstants(cmd_buf, pipe_layout, VK_SHADER_STAGE_ALL_GRAPHICS, 0, sizeof(VCW_PushConstants),
                       &push_const);

    vkCmdDrawIndexed(cmd_buf, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

    vkCmdEndRenderPass(cmd_buf);

    if (vkEndCommandBuffer(cmd_buf) != VK_SUCCESS)
        throw std::runtime_error("failed to record command buffer.");
}

void App::comp_vox_grid() {
    auto last_frame_checkpoint = std::chrono::high_resolution_clock::now();
    while (!glfwWindowShouldClose(window)) {
        for (int i = 0; i < CHUNK_SIDE_LENGTH; i++) {
            glfwPollEvents();

            auto start_time = std::chrono::high_resolution_clock::now();
            render(static_cast<float>(i));
            auto end_time = std::chrono::high_resolution_clock::now();
            auto render_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        }
    }

    vkDeviceWaitIdle(dev);
}

void App::clean_up() {
    clean_up_swap();

    clean_up_pipe();
    clean_up_desc();

#ifdef BIND_SAMPLE_TEXTURE
    clean_up_img(tex_img);
#endif

    clean_up_buf(vert_buf);
    clean_up_buf(index_buf);

    clean_up_sync();

    vkDestroyCommandPool(dev, cmd_pool, nullptr);
    vkDestroyDevice(dev, nullptr);

#ifdef VALIDATION
    destroy_debug_callback(inst, debug_msg, nullptr);
#endif

    vkDestroySurfaceKHR(inst, surf, nullptr);
    vkDestroyInstance(inst, nullptr);

    glfwDestroyWindow(window);

    glfwTerminate();
}
