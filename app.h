//
// Created by Ludw on 4/25/2024.
//

#ifndef VCW_APP_H
#define VCW_APP_H

#include "inc.h"
// ReSharper disable once CppUnusedIncludeDirective
#include "prop.h"
#include "util.h"

#include <tiny_obj_loader.h>

//
// vulkan debug
//
VkResult create_debug_callback(VkInstance inst, const VkDebugUtilsMessengerCreateInfoEXT *p_create_inf,
                               const VkAllocationCallbacks *p_alloc, VkDebugUtilsMessengerEXT *p_debug_msg);

void destroy_debug_callback(VkInstance inst, VkDebugUtilsMessengerEXT debug_msg, const VkAllocationCallbacks *p_alloc);

void get_debug_msg_info(VkDebugUtilsMessengerCreateInfoEXT &debug_info);

VKAPI_ATTR VkBool32 VKAPI_CALL
debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT msg_type,
               const VkDebugUtilsMessengerCallbackDataEXT *p_callback_data, void *p_user_data);

bool check_validation_support();

struct VCW_QueueFamilyIndices {
    std::optional<uint32_t> qf_graph;

    bool is_complete() const {
        return qf_graph.has_value();
    }
};

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec3 color;
    glm::vec2 uv;
    uint32_t mat_id;

    static VkVertexInputBindingDescription get_binding_desc();

    static std::array<VkVertexInputAttributeDescription, 5> get_attrib_descs();

    bool operator==(const Vertex &other) const {
        return pos == other.pos && normal == other.normal && uv == other.uv;
    }
};

template<>
struct std::hash<Vertex> {
    size_t operator()(Vertex const &vertex) const noexcept {
        return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.normal) << 1)) >> 1) ^ (
                hash<glm::vec2>()(vertex.uv) << 1);
    }
};

struct VCW_PushConstants {
    alignas(16) glm::mat4 view_proj;
    alignas(8) glm::vec2 res;
    alignas(4) uint32_t time;
};

struct VCW_Uniform {
    glm::vec4 min_vert;
    glm::vec4 max_vert;
    glm::vec4 chunk_res;
    glm::vec4 sector_start;
    glm::vec4 sector_end;
    float scalar;
    uint32_t use_textures;
};

struct VCW_Buffer {
    VkDeviceSize size;
    VkBuffer buf;

    VkDeviceMemory mem;
    void *p_mapped_mem = nullptr;

    VkAccessFlags cur_access_mask;
};

struct VCW_Image {
    VkImage img;
    VkDeviceMemory mem;

    VkImageView view;

    VkSampler sampler;
    bool combined_img_sampler = false;

    VkExtent3D extent;
    VkFormat format;

    VkImageLayout cur_layout;
    VkAccessFlags cur_access_mask;
};

struct VCW_OrthographicChunkModule {
    glm::mat4 proj;
    glm::mat4 trans_mat;

    glm::vec3 min_coord;
    glm::vec3 max_coord;
    glm::vec3 coord_diff;

    glm::vec3 axis_scalar = {1.f, 1.f, 1.f};

    void init(const glm::vec3 loc_min_coord, const glm::vec3 loc_max_coord) {
        min_coord = loc_min_coord;
        max_coord = loc_max_coord;
        coord_diff = loc_max_coord - loc_min_coord;

        trans_mat = glm::translate(glm::mat4(1.f), -glm::vec3(min_coord.x, min_coord.y, 0.f));
    }

    void update_proj() {
        const glm::mat4 ortho_mat_v1 = glm::ortho(0.f, coord_diff.x * axis_scalar.x,
                                                  0.f, coord_diff.y * axis_scalar.y,
                                                  0.f, coord_diff.z * axis_scalar.z);

        float max_max = max_component(max_coord);
        float min_max = max_component(min_coord);

        const glm::mat4 ortho_mat_v2 = glm::ortho(min_coord.x * (min_max / min_coord.x),
                                                  max_coord.x * (max_max / max_coord.x),
                                                  min_coord.y * (min_max / min_coord.y),
                                                  max_coord.y * (max_max / max_coord.y),
                                                  min_coord.z * (min_max / min_coord.z),
                                                  max_coord.z * (max_max / max_coord.z));

        float coord_diff_max = max_component(coord_diff);

        float scale_z = coord_diff.z / coord_diff_max;
        float scale_x = coord_diff.x / coord_diff_max;
        float scale_y = coord_diff.y / coord_diff_max * 2.f;
        std::cout << "scale_z: " << scale_z << std::endl;

        const glm::mat4 ortho_mat_v3 = glm::ortho(min_coord.x, max_coord.x,
                                                  min_coord.y, max_coord.y,
                                                  0.f, coord_diff.z * .75f);

        glm::mat4 trans_mat_v2 = glm::translate(glm::mat4(1.f), -glm::vec3(0.f, 0.f, min_coord.z + max_coord.z));

        proj = ortho_mat_v3; // * trans_mat_v2;
    }
};

struct VCW_RenderStats {
    double frame_time;
    double gpu_frame_time;
    uint32_t frame_count;
};

class App {
public:
    void run() {
        load_model();

        render_extent = VkExtent2D{256, 256};

        init_app();
        comp_vox_grid();
        clean_up();
    }

    VkInstance inst;
    VkDebugUtilsMessengerEXT debug_msg;

    VkPhysicalDevice phy_dev = VK_NULL_HANDLE;
    VkPhysicalDeviceMemoryProperties phy_dev_mem_props;
    VkPhysicalDeviceProperties phy_dev_props;

    std::vector<VkQueueFamilyProperties> qf_props;
    VCW_QueueFamilyIndices qf_indices;
    VkDevice dev;
    VkQueue q_graph;

    VkExtent2D render_extent;

    VkRenderPass rendp;
    VkFramebuffer frame_buf;
    VkPipelineLayout pipe_layout;
    VkPipeline pipe;

    std::vector<VkDescriptorSetLayout> desc_set_layouts;
    std::vector<VkDescriptorPoolSize> desc_pool_sizes;
    VkDescriptorPool desc_pool;
    std::vector<VkDescriptorSet> desc_sets;

    VkCommandPool cmd_pool;
    std::vector<VkCommandBuffer> cmd_bufs;

    VCW_Image depth_img;

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    VkSampler sampler;
    std::vector<VCW_Image> textures;

    std::vector<Vertex> vertices;
    VCW_Buffer vert_buf;
    glm::vec3 min_vert_coord;
    glm::vec3 max_vert_coord;
    glm::vec3 coord_diff;
    float model_scale;

    std::vector<uint32_t> indices;
    int indices_count;
    VCW_Buffer index_buf;

    VCW_Image render_target;
    VCW_Buffer transfer_buf;

    VkQueryPool query_pool;
    uint32_t frame_query_count;

    VCW_PushConstants push_const;

    VCW_Uniform ubo;
    std::vector<VCW_Buffer> unif_bufs;

    std::vector<VkFence> fens;

    uint32_t cur_frame = 0;
    VCW_RenderStats stats;
    VCW_RenderStats readable_stats;

    VCW_OrthographicChunkModule chunk_module;

    //
    //
    //
    // function definitions
    //
    //

    void init_app();

    void comp_vox_grid();

    void clean_up();

    //
    // vulkan instance
    //
    static std::vector<const char *> get_required_exts();

    void create_inst();

    void setup_debug_msg();

    //
    //
    // vulkan primitives
    //
    // physical device
    //
    static std::vector<VkQueueFamilyProperties> get_qf_props(VkPhysicalDevice loc_phy_dev);

    VCW_QueueFamilyIndices find_qf(VkPhysicalDevice loc_phy_dev) const;

    static bool check_phy_dev_ext_support(VkPhysicalDevice loc_phy_dev);

    bool is_phy_dev_suitable(VkPhysicalDevice loc_phy_dev) const;

    void pick_phy_dev();

    //
    // logical device
    //
    void create_dev();

    //
    // buffers
    //
    uint32_t find_mem_type(uint32_t type_filter, VkMemoryPropertyFlags mem_flags) const;

    VCW_Buffer create_buf(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags mem_props) const;

    void map_buf(VCW_Buffer *p_buf) const;

    void unmap_buf(VCW_Buffer *p_buf) const;

    void cp_data_to_buf(VCW_Buffer *p_buf, const void *p_data) const;

    void cp_data_from_buf(VCW_Buffer *p_buf, void *p_data) const;

    void cp_buf(const VCW_Buffer &src_buf, const VCW_Buffer &dst_buf);

    static void buffer_memory_barrier(VkCommandBuffer cmd_buf, VCW_Buffer *p_buf, const VkAccessFlags access_mask,
                                      const VkPipelineStageFlags src_stage, const VkPipelineStageFlags dst_stage);

    void clean_up_buf(const VCW_Buffer &buf) const;

    //
    // images
    //
    VkFormat find_supported_format(const std::vector<VkFormat> &possible_formats, VkImageTiling tiling,
                                   VkFormatFeatureFlags features) const;

    VkFormat find_depth_format() const;

    static bool has_stencil_component(VkFormat format);

    VCW_Image create_img(VkExtent2D extent, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags mem_props,
                         VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL) const;

    VCW_Image create_img(VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags mem_props,
                         VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL, VkImageType img_type = VK_IMAGE_TYPE_2D) const;

    VkImageView get_img_view(VCW_Image img, VkImageViewType img_view_type = VK_IMAGE_VIEW_TYPE_2D,
                             const VkImageSubresourceRange &subres_range = DEFAULT_SUBRESOURCE_RANGE) const;

    void create_img_view(VCW_Image *p_img, VkImageViewType img_view_type = VK_IMAGE_VIEW_TYPE_2D,
                         const VkImageSubresourceRange &subres_range = DEFAULT_SUBRESOURCE_RANGE) const;

    void create_img_view(VCW_Image *p_img, VkImageAspectFlags aspect_flags = 0) const;

    VkSampler create_sampler(VkFilter filter, VkSamplerAddressMode address_mode) const;

    void create_sampler(VCW_Image *p_img, VkFilter filter, VkSamplerAddressMode address_mode) const;

    static VkAccessFlags get_access_mask(VkImageLayout layout);

    static void transition_img_layout(VkCommandBuffer cmd_buf, VCW_Image *p_img, VkImageLayout layout,
                                      VkAccessFlags access_mask, VkPipelineStageFlags src_stage,
                                      VkPipelineStageFlags dst_stage);

    static void transition_img_layout(VkCommandBuffer cmd_buf, VCW_Image *p_img, VkImageLayout layout,
                                      VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage);

    static void cp_buf_to_img(VkCommandBuffer cmd_buf, const VCW_Buffer &buf, const VCW_Image &img, VkExtent3D extent);

    static void cp_buf_to_img(VkCommandBuffer cmd_buf, const VCW_Buffer &buf, const VCW_Image &img, VkExtent2D extent);

    static void cp_img_to_buf(VkCommandBuffer cmd_buf, const VCW_Image &img, const VCW_Buffer &buf, VkExtent3D extent);

    static void cp_img_to_buf(VkCommandBuffer cmd_buf, const VCW_Image &img, const VCW_Buffer &buf, VkExtent2D extent);

    static void
    blit_img(VkCommandBuffer cmd_buf, const VCW_Image &src, VkExtent3D src_extent, const VCW_Image &dst,
             VkExtent3D dst_extent,
             VkFilter filter);

    static void blit_img(VkCommandBuffer cmd_buf, const VCW_Image &src, const VCW_Image &dst, VkFilter filter);

    static void copy_img(VkCommandBuffer cmd_buf, const VCW_Image &src, const VCW_Image &dst);

    void clean_up_img(const VCW_Image &img) const;

    //
    // descriptor pool
    //
    void add_desc_set_layout(uint32_t binding_count, const VkDescriptorSetLayoutBinding *p_bindings);

    void add_pool_size(uint32_t desc_count, VkDescriptorType desc_type);

    void create_desc_pool(uint32_t max_sets);

    void write_buf_desc_binding(const VCW_Buffer &buf, uint32_t dst_set, uint32_t dst_binding,
                                VkDescriptorType desc_type) const;

    void write_img_desc_binding(const VCW_Image &img, uint32_t dst_set, uint32_t dst_binding,
                                VkDescriptorType desc_type,
                                VkImageLayout img_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) const;

    void write_sampler_desc_binding(VkSampler sampler, uint32_t dst_set, uint32_t dst_binding) const;

    void write_img_desc_array(const std::vector<VCW_Image> &imgs, uint32_t dst_set, uint32_t dst_binding,
                              VkDescriptorType desc_type) const;

    void clean_up_desc() const;

    //
    // pipeline prerequisites
    //
    void create_rendp();

    VkShaderModule create_shader_mod(const std::vector<char> &code) const;

    void create_frame_buf();

    void clean_up_pipe() const;

    //
    // render prerequisites
    //
    void create_cmd_pool();

    VkCommandBuffer begin_cmd() const;

    VkCommandBuffer begin_single_time_cmd() const;

    void end_single_time_cmd(VkCommandBuffer cmd_buf) const;

    void create_cmd_bufs();

    void create_sync();

    void render();

    void clean_up_sync() const;

    //
    //
    // personalized vulkan initialization
    //
    void load_model();

    void create_vert_buf();

    void create_index_buf();

    void create_unif_bufs();

    void create_textures_sampler();

    void create_textures();

    void create_render_target();

    void create_desc_pool_layout();

    void create_pipe();

    void write_desc_pool() const;

    void update_bufs(uint32_t index_inflight_frame);

    void record_cmd_buf(VkCommandBuffer cmd_buf);
};

#endif //VCW_APP_H
