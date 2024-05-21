#version 450
#extension GL_EXT_debug_printf: enable

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

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inPosition;
layout (location = 4) flat in vec3 inMinAABB;
layout (location = 5) flat in vec3 inMaxAABB;

layout (location = 0) out vec4 out_col;

const vec3 light_dir = normalize(vec3(1.0, 1.0, 1.0));

void main()
{
    if( any( lessThan(inPosition,inMinAABB) ) || any(lessThan(inMaxAABB,inPosition) ) )
    discard;

    vec4 color     = vec4(inColor,1);
    vec3 address3D = inPosition * 0.5 + vec3(0.5);
    imageStore(render_target, ivec3(imageSize(render_target) * address3D), color);

    out_col = vec4(1);
}