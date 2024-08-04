//
// Created by Ludw on 4/25/2024.
//

#include "app.h"
#include "vss/include/bvox.h"

#define STB_IMAGE_IMPLEMENTATION

#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION


#define MODEL_INDEX 9

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

    coord_diff = max_vert_coord - min_vert_coord;

    std::cout << "min vertex coord: " << min_vert_coord << std::endl;
    std::cout << "max vertex coord: " << max_vert_coord << std::endl;
    std::cout << "coord diff: " << coord_diff << std::endl;
}

void App::init_app() {
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

    create_cmd_pool();

    create_textures();
    create_textures_sampler();

    create_vert_buf();
    create_index_buf();

    create_unif_bufs();

    create_render_target();

    create_desc_pool_layout();
    create_pipe();

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

    ubo.min_vert = glm::vec4(min_vert_coord, 1.0f);
    ubo.max_vert = glm::vec4(max_vert_coord, 1.0f);
    ubo.chunk_res = glm::vec4(glm::vec3(CHUNK_SIDE_LENGTH), 0);
    ubo.scalar = model_scale;

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
    render_target = create_img(extent, VK_FORMAT_R8_UINT,
                               VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
                               VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_TYPE_3D);

    create_img_view(&render_target, VK_IMAGE_VIEW_TYPE_3D, DEFAULT_SUBRESOURCE_RANGE);

    VkDeviceSize size = CHUNK_SIZE * sizeof(uint8_t);
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

    VkPipelineRasterizationStateCreateInfo raster_info{};
    raster_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster_info.depthClampEnable = VK_FALSE;
    raster_info.rasterizerDiscardEnable = VK_FALSE;
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

void App::update_bufs(const uint32_t index_inflight_frame) {
    push_const.view_proj = chunk_module.proj;
    push_const.res = {render_extent.width, render_extent.height};
    push_const.time = stats.frame_count;
}

void App::record_cmd_buf(VkCommandBuffer cmd_buf, const uint32_t img_index) {
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(cmd_buf, &begin_info) != VK_SUCCESS)
        throw std::runtime_error("failed to begin recording command buffer.");

    vkCmdFillBuffer(cmd_buf, transfer_buf.buf, 0, transfer_buf.size, 0);

    transition_img_layout(cmd_buf, &render_target, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT,
                          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);

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
    chunk_module.init(min_vert_coord, max_vert_coord);
    chunk_module.axis_scalar = {1.f, 1.f, .5f};
    chunk_module.update_proj();

    std::vector<uint8_t> cached_output(CHUNK_SIZE);
    std::cout << "render extent: " << render_extent.width << "x" << render_extent.height << std::endl;

    BvoxHeader header{};
    header.chunk_res = CHUNK_SIDE_LENGTH;
    header.chunk_size = CHUNK_SIZE;

    write_empty_bvox("output.bvox", header);

    glfwPollEvents();
    ubo.sector_start = glm::vec4(glm::vec3(0.f), 0.f);
    ubo.sector_end = glm::vec4(glm::vec3(CHUNK_SIDE_LENGTH), 0.f);

    for (const auto &unif_buf: unif_bufs)
        memcpy(unif_buf.p_mapped_mem, &ubo, sizeof(ubo));


    auto start_time = std::chrono::high_resolution_clock::now();
    render();
    auto end_time = std::chrono::high_resolution_clock::now();
    auto render_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    stats.frame_time = static_cast<double>(render_duration.count()) / 1000.0;
    std::cout << "render time: " << stats.frame_time << "ms" << std::endl;


    start_time = std::chrono::high_resolution_clock::now();
    cp_data_from_buf(&transfer_buf, cached_output.data());

    uint32_t vox_count = 0;
    for (int l = 0; l < CHUNK_SIZE; l++) {
        if (cached_output[l] > 0) {
            // std::cout << cached_output[l] << " at " << l << std::endl;
            vox_count++;
        }
    }
    std::cout << "voxel count: " << vox_count << std::endl;

    end_time = std::chrono::high_resolution_clock::now();
    auto copy_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::cout << "copy time: " << copy_duration.count() << "ms" << std::endl;


    start_time = std::chrono::high_resolution_clock::now();
    append_to_bvox("output.bvox", cached_output);
    end_time = std::chrono::high_resolution_clock::now();
    auto write_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::cout << "write time: " << write_duration.count() << "ms" << std::endl;

    vkDeviceWaitIdle(dev);
}

void App::clean_up() {
    clean_up_pipe();
    clean_up_desc();

    for (const auto buf: unif_bufs)
        clean_up_buf(buf);

    for (const auto texture: textures)
        clean_up_img(texture);

    clean_up_buf(transfer_buf);

    vkDestroySampler(dev, sampler, nullptr);

    clean_up_img(render_target);

    clean_up_buf(vert_buf);
    clean_up_buf(index_buf);

    vkDestroyCommandPool(dev, cmd_pool, nullptr);
    vkDestroyDevice(dev, nullptr);

#ifdef VALIDATION
    destroy_debug_callback(inst, debug_msg, nullptr);
#endif

    vkDestroyInstance(inst, nullptr);
}
