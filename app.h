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
    glm::vec4 chunk_res;
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

    void init(const glm::vec3 min_coord, const glm::vec3 max_coord, const float chunk_res) {
        glm::vec3 dim = max_coord - min_coord;
        glm::vec3 center = (min_coord + max_coord) / 2.0f;

        float max_dim = glm::max(dim.x, glm::max(dim.y, dim.z));

        // scaling
        float scale_factor = chunk_res / max_dim;
        glm::vec3 scale = glm::vec3(scale_factor);
        scale.z *= 2.f;

        // translation - centering & scaling
        glm::mat4 model_matrix = glm::mat4(1.0f);
        model_matrix = glm::translate(model_matrix, -center * scale);
        model_matrix = glm::scale(model_matrix, scale);

        // create orthographic projection
        glm::mat4 ortho_proj = glm::ortho(
                -chunk_res * .5f, chunk_res * .5f, // left, right
                -chunk_res * .5f, chunk_res * .5f, // bottom, top
                0.f, chunk_res // near, far
        );

        // combine orthographic and model matrix
        proj = ortho_proj * model_matrix;
    }
};

struct VoxelizeParams {
    uint32_t chunk_res;
    uint32_t chunk_size;

    std::string input_file;
    std::string output_file;

    std::string material_dir;

    bool run_length_encode;
    bool morton_encode;

    bool generate_svo;
    uint32_t max_depth;
    std::string svo_file;
};

class App {
public:
    VoxelizeParams params;

    void run() {
        load_model();
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

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    std::vector<Vertex> vertices;
    VCW_Buffer vert_buf;
    glm::vec3 min_vert_coord;
    glm::vec3 max_vert_coord;
    glm::vec3 dim;

    std::vector<uint32_t> indices;
    int indices_count;
    VCW_Buffer index_buf;

    VCW_Image render_target;
    VCW_Buffer transfer_buf;

    VCW_PushConstants push_const;

    VCW_Uniform ubo;
    VCW_Buffer unif_buf;

    std::vector<VkFence> fens;

    uint32_t cur_frame = 0;

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

    std::vector<uint32_t> compile_shader(const std::string& source, shaderc_shader_kind kind, const char* entry_point);

    VkShaderModule create_shader_mod(const std::vector<uint32_t> &code) const;

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

    void create_unif_buf();

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
