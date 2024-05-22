#version 450

layout (push_constant) uniform PushConstants {
    mat4 view_proj;
    vec2 res;
    uint time;
} pc;

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_color;
layout (location = 3) in vec2 in_uv;
layout (location = 4) in uint in_mat_id;

layout (location = 0) out vec4 vs_pos;
layout (location = 1) out vec3 vs_normal;
layout (location = 2) out vec3 vs_color;
layout (location = 3) out vec2 vs_uv;
layout (location = 4) flat out uint vs_mat_id;

void main() {
    vs_pos = pc.view_proj * vec4(in_pos, 1.0);
    gl_Position = vs_pos;

    vs_normal = in_normal;
    vs_color = in_color;
    vs_uv = in_uv;
    vs_mat_id = in_mat_id;
}
