#version 450 core


layout (location = 0) in vec3 inPos;


uniform mat4 _Model;
uniform int  _TerrainWidth;


layout(std430, binding = 0) buffer TerrainHeightData { float height[]; };


void main()
{
    vec3 pos   = inPos;
         pos.y = height[gl_VertexID];
         
    gl_Position = _Model * vec4(pos, 1.0);
}
