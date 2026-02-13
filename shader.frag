#version 450

// from https://github.com/pumexx/pumex/tree/master/examples/pumexvoxelizer

layout (set = 0, binding = 0) uniform UBO {
    vec4 chunk_res;
} ubo;

layout (set = 0, binding = 1, r8ui) uniform uimage3D render_target;

layout (location = 0) in vec3 gs_pos;
layout (location = 1) in vec3 gs_normal;
layout (location = 2) in vec3 gs_color;
layout (location = 3) in vec2 gs_uv;
layout (location = 4) flat in uint gs_mat_id;
layout (location = 5) flat in vec3 gs_min_aabb;
layout (location = 6) flat in vec3 gs_max_aabb;

void main() {
    if (any(lessThan(gs_pos, gs_min_aabb)) || any(lessThan(gs_max_aabb, gs_pos))) discard;

    vec3 address = gs_pos * vec3(0.5) + vec3(0.5);
    ivec3 img_coord = ivec3(ubo.chunk_res.xyz * address);

    imageStore(render_target, img_coord, uvec4(1));
}