#version 450
#extension GL_EXT_debug_printf: enable

// from https://github.com/pumexx/pumex/tree/master/examples/pumexvoxelizer

layout (set = 0, binding = 0) uniform UBO {
    vec4 min_vert;
    vec4 max_vert;

    uint use_textures;
    float shadow_attenuation;

    float min_z;
    float max_z;
} ubo;
layout (set = 0, binding = 1) uniform sampler samp;
layout (set = 0, binding = 2) uniform texture2D textures[32];
layout (set = 0, binding = 3, rgba8) uniform image3D render_target;

layout (location = 0) in vec3 gs_pos;
layout (location = 1) in vec3 gs_normal;
layout (location = 2) in vec3 gs_color;
layout (location = 3) in vec2 gs_uv;
layout (location = 4) flat in uint gs_mat_id;
layout (location = 5) flat in vec3 gs_min_aabb;
layout (location = 6) flat in vec3 gs_max_aabb;

layout (location = 0) out vec4 out_col;

void main()
{
    if (any(lessThan(gs_pos, gs_min_aabb)) || any(lessThan(gs_max_aabb, gs_pos))) discard;

    vec4 color = vec4(gs_color, 1);
    vec3 address = gs_pos * 0.5;
    ivec3 img_coord = ivec3(imageSize(render_target) * address);
    imageStore(render_target, img_coord, color);
    // ivec3(128, 256, -128)
    out_col = vec4(1);
}