#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

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
layout (location = 4) flat out vec3 outMinAABB;
layout (location = 5) flat out vec3 outMaxAABB;

void main()
{
    // calculate triangle normal
    vec3 tNorm         = normalize(cross( gl_in[1].gl_Position.xyz-gl_in[0].gl_Position.xyz, gl_in[2].gl_Position.xyz-gl_in[0].gl_Position.xyz));
    vec3 tn            = abs( tNorm );

    // calculate pixel size
    vec3 imSize         = vec3(256);
    vec3 pixelSize      = 1.0 / imSize;
    float pixelDiagonal = 1.732050808 * pixelSize.x;

    vec4 vertPosition[3];
    vec3 edges[3];
    vec3 edgeNormals[3];
    vec3 minAABB = vec3(2,2,2);
    vec3 maxAABB = vec3(-2,-2,-2);
    for(uint i=0; i<3; ++i)
    {
        vertPosition[i] = gl_in[i].gl_Position;
        edges[i]        = normalize( gl_in[(i+1)%3].gl_Position.xyz / gl_in[(i+1)%3].gl_Position.w - gl_in[i].gl_Position.xyz / gl_in[i].gl_Position.w );
        edgeNormals[i]  = normalize( cross(edges[i],tNorm) );
        minAABB         = min( minAABB, vertPosition[i].xyz );
        maxAABB         = max( maxAABB, vertPosition[i].xyz );
    }

    // calculating on which plane this triangle will be projected. Which value is maximum ? x=0, y=1, z=2
    uint maxIndex = (tn.y>tn.x) ? ( (tn.z>tn.y) ? 2 : 1 ) : ( (tn.z>tn.x) ? 2 : 0 );

    minAABB -= pixelSize;
    maxAABB += pixelSize;

    outMinAABB = minAABB;
    outMaxAABB = maxAABB;

    // project triangle on xy, yz or yz plane where it's visible most
    // also - calculate data for conservative rasterization
    for(uint i=0; i<3; ++i)
    {
        // calculate bisector for conservative rasterization
        vec3 biSector        = pixelDiagonal * ( ( edges[(i+2)%3] / dot(edges[(i+2)%3], edgeNormals[i])) + ( edges[i] / dot(edges[i],edgeNormals[(i+2)%3])) );
        gs_normal = vs_normal[i];
        gs_color = vs_color[i];
        gs_uv = vs_uv[i];
        gs_pos = vec3(vertPosition[i].xyz/vertPosition[i].w + biSector);

        switch(maxIndex)
        {
            case 0:  gl_Position   = vec4(vertPosition[i].yz + biSector.yz,0,vertPosition[i].w);  break;
            case 1:  gl_Position   = vec4(vertPosition[i].xz + biSector.xz,0,vertPosition[i].w);  break;
            case 2:  gl_Position   = vec4(vertPosition[i].xy + biSector.xy,0,vertPosition[i].w);  break;
        }
        EmitVertex();
    }
    EndPrimitive();
}