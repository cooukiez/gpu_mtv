#version 450

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

layout (location = 0) in vec4 vs_pos[];
layout (location = 1) in vec3 vs_normal[];
layout (location = 2) in vec3 vs_color[];
layout (location = 3) in vec2 vs_uv[];
layout (location = 4) flat in uint vs_mat_id[];

layout (location = 0) out vec4 gs_pos;
layout (location = 1) out vec3 gs_normal;
layout (location = 2) out vec3 gs_color;
layout (location = 3) out vec2 gs_uv;
layout (location = 4) flat out uint gs_mat_id;
layout (location = 5) flat out int gs_swizzle;

void main() {
    vec3 normal = normalize(vs_normal[0] + vs_normal[1] + vs_normal[2]);

    // determine the most prominent normal component
    int swizzle = 0;
    if (abs(normal.x) > abs(normal.y) && abs(normal.x) > abs(normal.z)) {
        swizzle = 0; // X-axis
    } else if (abs(normal.y) > abs(normal.x) && abs(normal.y) > abs(normal.z)) {
        swizzle = 1; // Y-axis
    } else {
        swizzle = 2; // Z-axis
    }

    // Emit vertices with swizzled coordinates
    for (int i = 0; i < 3; ++i) {
        gs_normal = vs_normal[i];
        gs_color = vs_color[i];
        gs_uv = vs_uv[i];
        gs_mat_id = vs_mat_id[i];
        gs_swizzle = swizzle;

        switch (swizzle) {
            case 0: gs_pos = vs_pos[i].yzxw; break;
            case 1: gs_pos = vs_pos[i].xzyw; break;
            case 2: gs_pos = vs_pos[i]; break;
        }

        gl_Position = gs_pos;
        EmitVertex();
    }
    EndPrimitive();
}