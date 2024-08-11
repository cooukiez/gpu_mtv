//
// Created by Ludw on 4/25/2024.
//

#include "app.h"
#include "vss/include/bvox.h"

#define STB_IMAGE_IMPLEMENTATION

#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION


#define MODEL_INDEX 0

#if MODEL_INDEX == 0
#define MODEL_PATH "models/armadillo/armadillo.obj"
#define TEXTURE_PATH "models/armadillo/"
#endif

#if MODEL_INDEX == 1
#define MODEL_PATH "models/bunny/bunny.obj"
#define TEXTURE_PATH "models/bunny/"
#endif

#if MODEL_INDEX == 2
#define MODEL_PATH "models/cornell-box/cornell-box.obj"
#define TEXTURE_PATH "models/cornell-box/"
#endif

#if MODEL_INDEX == 3
#define MODEL_PATH "models/cow/cow.obj"
#define TEXTURE_PATH "models/cow/"
#endif

#if MODEL_INDEX == 4
#define MODEL_PATH "models/dragon/dragon.obj"
#define TEXTURE_PATH "models/dragon/"
#endif

#if MODEL_INDEX == 5
#define MODEL_PATH "models/duck/duck.obj"
#define TEXTURE_PATH "models/duck/"
#endif

#if MODEL_INDEX == 6
#define MODEL_PATH "models/lucy/lucy.obj"
#define TEXTURE_PATH "models/lucy/"
#endif

#if MODEL_INDEX == 7
#define MODEL_PATH "models/sponza/sponza.obj"
#define TEXTURE_PATH "models/sponza/"
#endif

#if MODEL_INDEX == 8
#define MODEL_PATH "models/suzanne/suzanne.obj"
#define TEXTURE_PATH "models/suzanne/"
#endif

#if MODEL_INDEX == 9
#define MODEL_PATH "models/teapot/teapot.obj"
#define TEXTURE_PATH "models/teapot/"
#endif

void App::load_model() {
    std::cout << std::endl << "--- Model loading ---" << std::endl;
    std::string warn, err;
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH, TEXTURE_PATH))
        throw std::runtime_error(err);
    if (!warn.empty())
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

    dim = max_vert_coord - min_vert_coord;

    std::cout << "min vertex coord: " << min_vert_coord << std::endl;
    std::cout << "max vertex coord: " << max_vert_coord << std::endl;
    std::cout << "dimensions: " << dim << std::endl;
    std::cout << "found " << vertices.size() << " vertices." << std::endl;
}

void App::init_app() {
    render_extent = VkExtent2D{params.chunk_res, params.chunk_res};

    //
    // vulkan core initialization
    //
    create_inst();
    setup_debug_msg();

    pick_phy_dev();
    create_dev();

    //
    // pipeline creation
    //
    create_rendp();
    create_frame_buf();

    create_cmd_pool();

    create_vert_buf();
    create_index_buf();

    create_unif_buf();

    create_render_target();

    create_desc_pool_layout();
    create_pipe();

    create_sync();

    create_desc_pool(MAX_FRAMES_IN_FLIGHT);
    write_desc_pool();

    create_cmd_bufs();
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

void App::create_unif_buf() {
    ubo.chunk_res = glm::vec4(glm::vec3(static_cast<float>(params.chunk_res)), 0);

    VkDeviceSize buf_size = sizeof(VCW_Uniform);
    unif_buf = create_buf(buf_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    cp_data_to_buf(&unif_buf, &ubo);
}

void App::create_render_target() {
    VkExtent3D extent = {params.chunk_res, params.chunk_res, params.chunk_res};
    render_target = create_img(extent, VK_FORMAT_R8_UINT,
                               VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
                               VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_TYPE_3D);

    create_img_view(&render_target, VK_IMAGE_VIEW_TYPE_3D, DEFAULT_SUBRESOURCE_RANGE);

    VkDeviceSize size = params.chunk_size * sizeof(uint8_t);
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
    uniform_layout_binding.stageFlags = VK_SHADER_STAGE_ALL;
    bindings.push_back(uniform_layout_binding);
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
    add_pool_size(MAX_FRAMES_IN_FLIGHT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
}

void App::create_pipe() {
    std::cout << std::endl << "--- Pipeline creation ---" << std::endl;
    std::string vert_code = read_file_string("shaders/shader.vert");
    std::string geom_code = read_file_string("shaders/shader.geom");
    std::string frag_code = read_file_string("shaders/shader.frag");

    std::cout << "compiling vertex shader." << std::endl;
    std::vector<uint32_t> vert_bin = compile_shader(vert_code, shaderc_glsl_vertex_shader, "main");
    std::cout << "compiling geometry shader." << std::endl;
    std::vector<uint32_t> geom_bin = compile_shader(geom_code, shaderc_glsl_geometry_shader, "main");
    std::cout << "compiling fragment shader." << std::endl;
    std::vector<uint32_t> frag_bin = compile_shader(frag_code, shaderc_glsl_fragment_shader, "main");

    VkShaderModule vert_module = create_shader_mod(vert_bin);
    VkShaderModule geom_module = create_shader_mod(geom_bin);
    VkShaderModule frag_module = create_shader_mod(frag_bin);

    VkPipelineShaderStageCreateInfo vert_stage_info{};
    vert_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_stage_info.module = vert_module;
    vert_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo geom_stage_info{};
    geom_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    geom_stage_info.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
    geom_stage_info.module = geom_module;
    geom_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo frag_stage_info{};
    frag_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_stage_info.module = frag_module;
    frag_stage_info.pName = "main";

    std::array stages = {vert_stage_info, geom_stage_info, frag_stage_info};

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
    raster_info.depthClampEnable = VK_FALSE;
    raster_info.rasterizerDiscardEnable = VK_FALSE;
    raster_info.polygonMode = VK_POLYGON_MODE_FILL;
    raster_info.lineWidth = 1.0f;
    raster_info.cullMode = VK_CULL_MODE_NONE;
    raster_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
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
    push_const_range.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
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
    vkDestroyShaderModule(dev, geom_module, nullptr);
}

void App::write_desc_pool() const {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        write_buf_desc_binding(unif_buf, static_cast<uint32_t>(i), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        write_img_desc_binding(render_target, static_cast<uint32_t>(i), 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               VK_IMAGE_LAYOUT_GENERAL);
    }
}

void App::update_bufs(const uint32_t index_inflight_frame) {
    push_const.view_proj = chunk_module.proj;
    push_const.res = {render_extent.width, render_extent.height};
}

void App::record_cmd_buf(VkCommandBuffer cmd_buf) {
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(cmd_buf, &begin_info) != VK_SUCCESS)
        throw std::runtime_error("failed to begin recording command buffer.");

    VkRenderPassBeginInfo rendp_begin_info{};
    rendp_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rendp_begin_info.renderPass = rendp;
    rendp_begin_info.framebuffer = frame_buf;
    rendp_begin_info.renderArea.offset = {0, 0};
    rendp_begin_info.renderArea.extent = render_extent;

    vkCmdFillBuffer(cmd_buf, transfer_buf.buf, 0, transfer_buf.size, 0);

    transition_img_layout(cmd_buf, &render_target, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT,
                          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

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

    transition_img_layout(cmd_buf, &render_target, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    buffer_memory_barrier(cmd_buf, &transfer_buf, VK_ACCESS_TRANSFER_WRITE_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

    cp_img_to_buf(cmd_buf, render_target, transfer_buf, render_target.extent);
    transition_img_layout(cmd_buf, &render_target, VK_IMAGE_LAYOUT_GENERAL, 0,
                          VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
    buffer_memory_barrier(cmd_buf, &transfer_buf, 0,
                          VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

    if (vkEndCommandBuffer(cmd_buf) != VK_SUCCESS)
        throw std::runtime_error("failed to record command buffer.");
}

void App::comp_vox_grid() {
    std::cout << std::endl << "--- Voxelization ---" << std::endl;
    chunk_module.init(min_vert_coord, max_vert_coord, static_cast<float>(params.chunk_res));

    std::vector<uint8_t> cached_output(params.chunk_size);
    std::cout << "render extent: " << render_extent.width << "x" << render_extent.height << std::endl;

    BvoxHeader header{};
    header.chunk_res = params.chunk_res;
    header.chunk_size = params.chunk_size;

    write_empty_bvox("output.bvox", header);

    auto start_time = std::chrono::high_resolution_clock::now();
    render();
    auto end_time = std::chrono::high_resolution_clock::now();
    auto voxelization_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    double voxelization_time = static_cast<double>(voxelization_duration.count()) / 1000.0;

    start_time = std::chrono::high_resolution_clock::now();
    cp_data_from_buf(&transfer_buf, cached_output.data());

    int vox_count = std::count_if(cached_output.begin(), cached_output.end(), [](int x) { return x > 0; });

    end_time = std::chrono::high_resolution_clock::now();
    auto copy_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    start_time = std::chrono::high_resolution_clock::now();
    append_to_bvox("output.bvox", cached_output);
    end_time = std::chrono::high_resolution_clock::now();
    auto write_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    vkDeviceWaitIdle(dev);

    std::cout << std::endl << "--- Results ---" << std::endl;
    std::cout << "voxelization time: " << voxelization_time << "ms" << std::endl;
    std::cout << "copy time: " << copy_duration.count() << "ms" << std::endl;
    std::cout << "write time: " << write_duration.count() << "ms" << std::endl;
    std::cout << "voxel count: " << vox_count << std::endl;
}

void App::clean_up() {
    clean_up_pipe();
    clean_up_desc();

    clean_up_buf(unif_buf);

    clean_up_buf(transfer_buf);
    clean_up_img(render_target);

    vkDestroyFramebuffer(dev, frame_buf, nullptr);
    vkDestroyRenderPass(dev, rendp, nullptr);

    clean_up_sync();

    clean_up_buf(vert_buf);
    clean_up_buf(index_buf);

    vkDestroyCommandPool(dev, cmd_pool, nullptr);
    vkDestroyDevice(dev, nullptr);

#ifdef VALIDATION
    destroy_debug_callback(inst, debug_msg, nullptr);
#endif

    vkDestroyInstance(inst, nullptr);
}
