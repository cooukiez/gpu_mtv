#version 450
#extension GL_EXT_debug_printf: enable

// from https://github.com/pumexx/pumex/tree/master/examples/pumexvoxelizer

layout (set = 0, binding = 0) uniform UBO {
    vec4 min_vert;
    vec4 max_vert;
    vec4 chunk_res;

    vec4 sector_start;
    vec4 sector_end;

    float scalar;
    uint use_textures;
} ubo;

layout (set = 0, binding = 1) uniform sampler samp;
layout (set = 0, binding = 2) uniform texture2D textures[32];
layout (set = 0, binding = 3, r8ui) uniform uimage3D render_target;

layout (location = 0) in vec3 gs_pos;
layout (location = 1) in vec3 gs_normal;
layout (location = 2) in vec3 gs_color;
layout (location = 3) in vec2 gs_uv;
layout (location = 4) flat in uint gs_mat_id;
layout (location = 5) flat in vec3 gs_min_aabb;
layout (location = 6) flat in vec3 gs_max_aabb;

layout (location = 0) out vec4 out_col;

void main() {
    if (any(lessThan(gs_pos, gs_min_aabb)) || any(lessThan(gs_max_aabb, gs_pos))) discard;

    vec3 address = gs_pos * vec3(0.5) + vec3(0.5);
    ivec3 img_coord = ivec3(ubo.chunk_res.xyz * address);
    //if (any(lessThan(vec3(img_coord), ubo.sector_start.xyz)) || any(lessThan(ubo.sector_end.xyz, vec3(img_coord)))) discard;
    //img_coord -= ivec3(ubo.sector_start.xyz);

    vec4 tex_col = ubo.use_textures != 0 ? texture(sampler2D(textures[gs_mat_id], samp), gs_uv) : vec4(gs_color, 1);

    imageStore(render_target, img_coord, uvec4(1));
    out_col = tex_col;
}