#version 450

// from https://github.com/pumexx/pumex/tree/master/examples/pumexvoxelizer

#extension GL_ARB_separate_shader_objects: enable
#extension GL_ARB_shading_language_420pack: enable
#extension GL_EXT_debug_printf: enable

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

layout (set = 0, binding = 3, rgba8) uniform image3D render_target;

layout (location = 0) in vec4 vs_pos[];
layout (location = 1) in vec3 vs_normal[];
layout (location = 2) in vec3 vs_color[];
layout (location = 3) in vec2 vs_uv[];
layout (location = 4) flat in uint vs_mat_id[];

layout (location = 0) out vec3 gs_pos;
layout (location = 1) out vec3 gs_normal;
layout (location = 2) out vec3 gs_color;
layout (location = 3) out vec2 gs_uv;
layout (location = 4) flat out uint gs_mat_id;
layout (location = 5) flat out vec3 gs_min_aabb;
layout (location = 6) flat out vec3 gs_max_aabb;

void main() {
    // calculate triangle normal
    vec3 norm = normalize(cross(gl_in[1].gl_Position.xyz - gl_in[0].gl_Position.xyz, gl_in[2].gl_Position.xyz - gl_in[0].gl_Position.xyz));
    vec3 abs_norm = abs(norm);

    // calculate pixel size
    vec3 chunk_size = imageSize(render_target);
    vec3 px_size = 1.0 / chunk_size;
    float px_diagonal = 1.732050808 * px_size.x;

    vec4 vert_pos[3];
    vec3 edges[3];
    vec3 edge_norms[3];
    vec3 min_aabb = vec3(2, 2, 2);
    vec3 max_aabb = vec3(-2, -2, -2);

    for (uint i = 0; i < 3; ++i) {
        vert_pos[i] = gl_in[i].gl_Position;

        edges[i] = normalize(gl_in[(i + 1) % 3].gl_Position.xyz / gl_in[(i + 1) % 3].gl_Position.w - gl_in[i].gl_Position.xyz / gl_in[i].gl_Position.w);
        edge_norms[i] = normalize(cross(edges[i], norm));

        min_aabb = min(min_aabb, vert_pos[i].xyz);
        max_aabb = max(max_aabb, vert_pos[i].xyz);
    }

    // calculating on which plane this triangle will be projected.
    // which value is maximum ? x = 0, y = 1, z = 2
    uint max_idx = (abs_norm.y > abs_norm.x) ? ((abs_norm.z > abs_norm.y) ? 2 : 1) : ((abs_norm.z > abs_norm.x) ? 2 : 0);

    min_aabb -= px_size;
    max_aabb += px_size;

    gs_min_aabb = min_aabb;
    gs_max_aabb = max_aabb;

    // 1. project triangle on xy, yz or yz plane where it's visible most
    // 2. calculate data for conservative rasterization
    for (uint i = 0; i < 3; ++i) {
        // calculate bisector for conservative rasterization
        vec3 bisector = px_diagonal * ((edges[(i + 2) % 3] / dot(edges[(i + 2) % 3], edge_norms[i])) + (edges[i] / dot(edges[i], edge_norms[(i + 2) % 3])));

        gs_normal = vs_normal[i];
        gs_color = vs_color[i];
        gs_uv = vs_uv[i];
        gs_pos = vec3(vert_pos[i].xyz + bisector);

        switch (max_idx) {
            case 0:  gl_Position = vec4(vert_pos[i].yz + bisector.yz, 0, vert_pos[i].w);  break;
            case 1:  gl_Position = vec4(vert_pos[i].xz + bisector.xz, 0, vert_pos[i].w);  break;
            case 2:  gl_Position = vec4(vert_pos[i].xy + bisector.xy, 0, vert_pos[i].w);  break;
        }

        EmitVertex();
    }
    EndPrimitive();
}