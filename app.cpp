//
// Created by Ludw on 4/25/2024.
//

#include "app.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION

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

    create_cmd_pool();

    load_model();
    create_textures();
    create_textures_sampler();

    create_vert_buf();
    create_index_buf();

    create_unif_bufs();

    create_render_target();

    create_frame_bufs(swap_imgs);

    create_desc_pool_layout();
    create_pipe();

    create_desc_pool(MAX_FRAMES_IN_FLIGHT);
    write_desc_pool();

    create_cmd_bufs();
    create_sync();

    create_query_pool(2);

    const float largest_dim = max_component(max_vert_coord - min_vert_coord);
    std::cout << "largest scene dimension: " << largest_dim << std::endl;
}

void App::load_model() {
    std::string warn, err;
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH, TEXTURE_PATH))
        throw std::runtime_error(err);
    std::cout << warn << std::endl;

    std::unordered_map<Vertex, uint32_t> unique_vertices{};

    if (attrib.normals.empty())
        throw std::runtime_error("model does not contain normals.");

    for (const auto &shape: shapes) {
        size_t index_offset = 0;
        for (size_t i = 0; i < shape.mesh.num_face_vertices.size(); i++) {
            size_t face_vertices = shape.mesh.num_face_vertices[i];
            size_t mat_id = shape.mesh.material_ids[i];

            for (size_t j = 0; j < face_vertices; j++) {
                tinyobj::index_t index = shape.mesh.indices[index_offset + j];

                Vertex vertex{};

                vertex.pos = {
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]
                };

                min_vert_coord = glm::min(min_vert_coord, vertex.pos);
                max_vert_coord = glm::max(max_vert_coord, vertex.pos);

                vertex.normal = {
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2]
                };

                if (!materials.empty()) {
                    tinyobj::real_t *color = materials[mat_id].diffuse;
                    vertex.color = {
                        color[0],
                        color[1],
                        color[2]
                    };
                } else {
                    vertex.color = {1.0f, 1.0f, 1.0f};
                }


                if (!attrib.texcoords.empty()) {
                    vertex.uv = {
                        attrib.texcoords[2 * index.texcoord_index + 0],
                        1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                    };
                }

                vertex.mat_id = static_cast<uint32_t>(mat_id);

                if (!unique_vertices.contains(vertex)) {
                    unique_vertices[vertex] = static_cast<uint32_t>(vertices.size());
                    vertices.push_back(vertex);
                }

                indices.push_back(unique_vertices[vertex]);
            }

            index_offset += face_vertices;
        }
    }

    std::cout << "min vertex coord: " << min_vert_coord << std::endl;
    std::cout << "max vertex coord: " << max_vert_coord << std::endl;
}

void App::create_vert_buf() {
    const VkDeviceSize buf_size = sizeof(vertices[0]) * vertices.size();

    VCW_Buffer staging_buf = create_buf(buf_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    cp_data_to_buf(&staging_buf, vertices.data());

    vert_buf = create_buf(buf_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    cp_buf(staging_buf, vert_buf);

    clean_up_buf(staging_buf);
}

void App::create_index_buf() {
    indices_count = static_cast<int>(indices.size());
    const VkDeviceSize buf_size = sizeof(indices[0]) * indices.size();

    VCW_Buffer staging_buf = create_buf(buf_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    cp_data_to_buf(&staging_buf, indices.data());

    index_buf = create_buf(buf_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    cp_buf(staging_buf, index_buf);

    clean_up_buf(staging_buf);
}

// TODO: implement texture set (every texture single time)
void App::create_textures() {
    std::cout << "material count: " << materials.size() << std::endl;
    for (const auto &mat: materials) {
        if (mat.diffuse_texname.empty())
            continue;

        std::cout << "loading texture: " << mat.diffuse_texname << std::endl;

        /*
        if (textures.find(mat.diffuse_texname) != textures.end())
            continue;
        */

        int tex_width, tex_height, tex_channels;
        stbi_uc *pixels = stbi_load((TEXTURE_PATH + mat.diffuse_texname).c_str(), &tex_width, &tex_height,
                                    &tex_channels, STBI_rgb_alpha);
        const VkDeviceSize img_size = tex_width * tex_height * 4;

        if (!pixels)
            throw std::runtime_error("failed to load texture image.");

        VCW_Buffer staging_buf = create_buf(img_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        cp_data_to_buf(&staging_buf, pixels);

        stbi_image_free(pixels);

        const VkExtent2D extent = {static_cast<uint32_t>(tex_width), static_cast<uint32_t>(tex_height)};
        VCW_Image tex_img = create_img(extent, VK_FORMAT_R8G8B8A8_SRGB,
                                       VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkCommandBuffer cmd_buf = begin_single_time_cmd();
        transition_img_layout(cmd_buf, &tex_img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                              VK_PIPELINE_STAGE_TRANSFER_BIT);
        cp_buf_to_img(cmd_buf, staging_buf, tex_img, extent);
        transition_img_layout(cmd_buf, &tex_img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        end_single_time_cmd(cmd_buf);

        create_img_view(&tex_img, VK_IMAGE_ASPECT_COLOR_BIT);

        clean_up_buf(staging_buf);

        /*
        textures[mat.diffuse_texname] = tex_img;
        */
        textures.push_back(tex_img);
    }
    std::cout << "loaded textures: " << textures.size() << std::endl;
}

void App::create_textures_sampler() {
    sampler = create_sampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
}

void App::create_unif_bufs() {
    if (!textures.empty())
        ubo.use_textures = true;
    ubo.shadow_attenuation = 0.5f;
    ubo.min_vert = glm::vec4(min_vert_coord, 1.0f);
    ubo.max_vert = glm::vec4(max_vert_coord, 1.0f);

    VkDeviceSize buf_size = sizeof(VCW_Uniform);
    unif_bufs.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        unif_bufs[i] = create_buf(buf_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        map_buf(&unif_bufs[i]);
    }
}

void App::create_render_target() {
    VkExtent3D extent = {CHUNK_SIDE_LENGTH, CHUNK_SIDE_LENGTH, CHUNK_SIDE_LENGTH};
    render_target = create_img(extent, VK_FORMAT_R8G8B8A8_UNORM,
                               VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_TYPE_3D);

    create_img_view(&render_target, VK_IMAGE_VIEW_TYPE_3D, DEFAULT_SUBRESOURCE_RANGE);

    VkDeviceSize size = CHUNK_SIZE * sizeof(glm::u8vec4);
    transfer_buf = create_buf(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

void App::create_desc_pool_layout() {
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    uint32_t last_binding = 0;

    VkDescriptorSetLayoutBinding uniform_layout_binding{};
    uniform_layout_binding.binding = last_binding;
    uniform_layout_binding.descriptorCount = 1;
    uniform_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniform_layout_binding.pImmutableSamplers = nullptr;
    uniform_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings.push_back(uniform_layout_binding);
    last_binding++;

    VkDescriptorSetLayoutBinding sampler_layout_binding{};
    sampler_layout_binding.binding = last_binding;
    sampler_layout_binding.descriptorCount = 1;
    sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    sampler_layout_binding.pImmutableSamplers = nullptr;
    sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings.push_back(sampler_layout_binding);
    last_binding++;

    VkDescriptorSetLayoutBinding textures_layout_binding{};
    textures_layout_binding.binding = last_binding;
    if (textures.size() > DESCRIPTOR_TEXTURE_COUNT)
        throw std::runtime_error("allowed texture count to small to fit all textures.");
    textures_layout_binding.descriptorCount = DESCRIPTOR_TEXTURE_COUNT;
    textures_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    textures_layout_binding.pImmutableSamplers = nullptr;
    textures_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings.push_back(textures_layout_binding);
    last_binding++;

    VkDescriptorSetLayoutBinding render_target_layout_binding{};
    render_target_layout_binding.binding = last_binding;
    render_target_layout_binding.descriptorCount = 1;
    render_target_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    render_target_layout_binding.pImmutableSamplers = nullptr;
    render_target_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings.push_back(render_target_layout_binding);
    last_binding++;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        add_desc_set_layout(static_cast<uint32_t>(bindings.size()), bindings.data());
    }

    add_pool_size(MAX_FRAMES_IN_FLIGHT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    add_pool_size(MAX_FRAMES_IN_FLIGHT, VK_DESCRIPTOR_TYPE_SAMPLER);
    add_pool_size(MAX_FRAMES_IN_FLIGHT * DESCRIPTOR_TEXTURE_COUNT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
    add_pool_size(MAX_FRAMES_IN_FLIGHT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
}

void App::create_pipe() {
    auto vert_code = read_file<char>("vert.spv");
    auto frag_code = read_file<char>("frag.spv");
    auto geom_code = read_file<char>("geom.spv");

    VkShaderModule vert_module = create_shader_mod(vert_code);
    VkShaderModule frag_module = create_shader_mod(frag_code);
    VkShaderModule geom_module = create_shader_mod(geom_code);

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

    VkPipelineShaderStageCreateInfo geom_stage_info{};
    geom_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    geom_stage_info.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
    geom_stage_info.module = geom_module;
    geom_stage_info.pName = "main";

    std::array stages = {vert_stage_info, frag_stage_info, geom_stage_info};

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

    VkPipelineRasterizationConservativeStateCreateInfoEXT conservative_raster_info{};
    conservative_raster_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT;
    conservative_raster_info.flags = 0;
    conservative_raster_info.conservativeRasterizationMode = VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT;
    conservative_raster_info.extraPrimitiveOverestimationSize = 0.0f;

    VkPipelineRasterizationStateCreateInfo raster_info{};
    raster_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster_info.depthClampEnable = VK_FALSE;
    raster_info.rasterizerDiscardEnable = VK_FALSE;
    raster_info.polygonMode = POLYGON_MODE;
    raster_info.lineWidth = 1.0f;
    raster_info.cullMode = CULL_MODE;
    raster_info.frontFace = FRONT_FACE;
    raster_info.depthBiasEnable = VK_FALSE;
    raster_info.pNext = &conservative_raster_info;

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

    VkPipelineViewportStateCreateInfo viewport_info{};
    viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_info.viewportCount = 1;
    viewport_info.scissorCount = 1;

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
    pipe_info.stageCount = static_cast<uint32_t>(stages.size());
    pipe_info.pStages = stages.data();
    pipe_info.pVertexInputState = &vert_input_info;
    pipe_info.pInputAssemblyState = &input_asm_info;
    pipe_info.pRasterizationState = &raster_info;
    pipe_info.pMultisampleState = &multisample_info;
    pipe_info.pColorBlendState = &blend_info;
    pipe_info.pViewportState = &viewport_info;
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

void App::write_desc_pool() const {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        uint32_t last_binding = 0;

        write_buf_desc_binding(unif_bufs[i], static_cast<uint32_t>(i), last_binding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        last_binding++;

        write_sampler_desc_binding(sampler, static_cast<uint32_t>(i), last_binding);
        last_binding++;

        if (!textures.empty()) {
            write_img_desc_array(textures, static_cast<uint32_t>(i), last_binding, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
        }
        last_binding++;

        write_img_desc_binding(render_target, static_cast<uint32_t>(i), last_binding, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               VK_IMAGE_LAYOUT_GENERAL);
        last_binding++;
    }
}

void App::update_bufs(uint32_t index_inflight_frame) {
    const float min_z = min_vert_coord.z;
    const float max_z = max_vert_coord.z;
    push_const.view_proj = glm::ortho(min_vert_coord.x, max_vert_coord.x, min_vert_coord.y, max_vert_coord.y, min_z, max_z);
    push_const.res = {render_extent.width, render_extent.height};
    push_const.time = stats.frame_count;

    ubo.min_z = min_z;
    ubo.max_z = max_z;

    memcpy(unif_bufs[index_inflight_frame].p_mapped_mem, &ubo, sizeof(ubo));
}

void App::record_cmd_buf(VkCommandBuffer cmd_buf, const uint32_t img_index) {
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

    vkCmdResetQueryPool(cmd_buf, query_pool, img_index * frame_query_count, frame_query_count);

    vkCmdWriteTimestamp(cmd_buf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, query_pool, img_index * frame_query_count);
    transition_img_layout(cmd_buf, &render_target, VK_IMAGE_LAYOUT_GENERAL,
                          VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);


    vkCmdBeginRenderPass(cmd_buf, &rendp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(render_extent.width);
    viewport.height = static_cast<float>(render_extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd_buf, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = render_extent;
    vkCmdSetScissor(cmd_buf, 0, 1, &scissor);

    const VkBuffer vert_bufs[] = {vert_buf.buf};
    constexpr VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd_buf, 0, 1, vert_bufs, offsets);

    vkCmdBindIndexBuffer(cmd_buf, index_buf.buf, 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe_layout, 0, 1,
                            &desc_sets[cur_frame], 0, nullptr);
    vkCmdPushConstants(cmd_buf, pipe_layout, VK_SHADER_STAGE_ALL_GRAPHICS, 0, sizeof(VCW_PushConstants),
                       &push_const);

    vkCmdDrawIndexed(cmd_buf, static_cast<uint32_t>(indices_count), 1, 0, 0, 0);

    vkCmdEndRenderPass(cmd_buf);

    vkCmdWriteTimestamp(cmd_buf, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, query_pool, img_index * frame_query_count + 1);
    transition_img_layout(cmd_buf, &render_target, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

    if (vkEndCommandBuffer(cmd_buf) != VK_SUCCESS)
        throw std::runtime_error("failed to record command buffer.");
}

void App::fetch_queries(const uint32_t img_index) {
    std::vector<uint64_t> buffer(frame_query_count);

    const VkResult result = vkGetQueryPoolResults(dev, query_pool, img_index * frame_query_count, frame_query_count,
                                                  sizeof(uint64_t) * frame_query_count, buffer.data(), sizeof(uint64_t),
                                                  VK_QUERY_RESULT_64_BIT);
    if (result == VK_NOT_READY) {
        return;
    } else if (result == VK_SUCCESS) {
        stats.gpu_frame_time = static_cast<float>(buffer[1] - buffer[0]) * phy_dev_props.limits.timestampPeriod /
                               1000000.0f;
    } else {
        throw std::runtime_error("failed to receive query results.");
    }
}

void App::comp_vox_grid() {
    pane_module.min_coord = min_vert_coord;
    pane_module.max_coord = max_vert_coord;
    pane_module.total_pane_count = CHUNK_SIDE_LENGTH;
    pane_module.init();

    auto last_frame_checkpoint = std::chrono::high_resolution_clock::now();
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        auto start_time = std::chrono::high_resolution_clock::now();
        render();
        auto end_time = std::chrono::high_resolution_clock::now();
        auto render_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        stats.frame_time = static_cast<double>(render_duration.count()) / 1000.0;

        stats.frame_count++;
    }

    VkCommandBuffer cmd_buf = begin_single_time_cmd();
    cp_img_to_buf(cmd_buf, render_target, transfer_buf, render_target.extent);
    end_single_time_cmd(cmd_buf);

    auto *output = new glm::u8vec4[CHUNK_SIZE];
    cp_data_from_buf(&transfer_buf, output);

    auto *compressed_data = new uint8_t[CHUNK_SIZE];

    uint32_t vox_count = 0;
    for (int i = 0; i < CHUNK_SIZE; i++) {
        vox_count += output[i].a;
        compressed_data[i] = output[i].a;
    }

    std::cout << "collected " << vox_count << " voxels." << std::endl;

    write_file("output/output.bvox", compressed_data, CHUNK_SIZE);

    vkDeviceWaitIdle(dev);
}

void App::clean_up() {
    clean_up_swap();

    clean_up_pipe();
    clean_up_desc();

    for (const auto buf: unif_bufs)
        clean_up_buf(buf);

    for (const auto texture: textures)
        clean_up_img(texture);

    clean_up_buf(vert_buf);
    clean_up_buf(index_buf);

    vkDestroyQueryPool(dev, query_pool, nullptr);

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
