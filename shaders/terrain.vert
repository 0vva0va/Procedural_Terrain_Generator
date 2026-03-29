#version 450 core

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;


uniform mat4 _MVP;
uniform mat4 _Model;
uniform int  _TerrainWidth;

// _LightSpaceMatrix removed — point light shadows are sampled via world-pos
// direction into a samplerCube, so no light-space transform is needed here.


// ---- terrain params -----
layout(std430, binding = 0) buffer TerrainHeightData { float height[];  };
layout(std430, binding = 1) buffer TerrainNormalData { vec4  normals[]; };


out vec3  FS_Normal;
out vec3  FS_WorldPos;
// FS_LightSpace removed — fragment shader computes (FS_WorldPos - _LightPos)
// and passes that direction straight to texture(_ShadowMap, dir).


void main() {
    vec3 pos = inPos;

    int x   = gl_VertexID % _TerrainWidth;
    int z   = gl_VertexID / _TerrainWidth;
    
    float h = height[gl_VertexID];
    pos.y   = h;
    
    vec4 normal = normals[gl_VertexID];
    vec3 n      = normalize(vec3(normal.xyz));
    
    gl_Position = _MVP * vec4(pos, 1.0);

    FS_Normal   = mat3(_Model) * n;
    FS_WorldPos = vec3(_Model * vec4(pos, 1.0));
}
